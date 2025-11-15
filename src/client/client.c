/*
 * Клиент для безопасной передачи файлов с использованием mTLS
 * Реализует защищенное соединение с сервером для операций с файлами
 * Использует протокол BLAKE3 для проверки целостности данных
 */

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

#include <readline/readline.h> // Добавить для удобного ввода команд
#include <readline/history.h>


#include <signal.h>

// только для клиента
#include "../../include/client.h"

#define BLAKE3_IMPLEMENTATION
#include "blake3.h"

// Константы приложения
#define DEFAULT_PORT 8181                    // Порт сервера по умолчанию
#define CLEAR "clear"                        // Команда очистки экрана
#define BUFFER_SIZE 4096                     // Размер буфера для передачи данных
#define FILENAME_MAX_LEN 256                 // Максимальная длина имени файла
#define BAR_LENGTH 20                        // Длина прогресс-бара
#define FINGERPRINT_LEN 65                   // Длина отпечатка (64 hex + '\0')

// Глобальная структура для хранения информации о сессии
typedef struct {
    char session_hash[65];                   // Хеш сессии в hex-формате
    char client_fingerprint[65];             // Отпечаток клиентского сертификата
    pthread_mutex_t mutex;                   // Мьютекс для синхронизации доступа
    int authenticated;                       // Флаг успешной аутентификации
    SSL* ssl;                               // Указатель на SSL-соединение
    int connected;                          // Флаг установленного соединения
} session_info_t;

typedef enum {
    CLIENT_STATE_WAITING_CONNECT, // Ждёт команды CMD_CONNECT
    CLIENT_STATE_WAITING_APPROVAL, // Ждёт подтверждения администратора
    CLIENT_STATE_AUTHENTICATED,   // Подключён и аутентифицирован
    CLIENT_STATE_ERROR            // Ошибка, нужно закрыть соединение
} client_state_t;

// Глобальный экземпляр информации о сессии с инициализацией
static session_info_t g_session = {
    .session_hash = {0},
    .client_fingerprint = {0},
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .authenticated = 0,
    .ssl = NULL,
    .connected = 0
};

// Структура для передачи аргументов в поток подключения
typedef struct {
    int port;                               // Порт сервера
    char ip[16];                            // IP-адрес сервера
} ThreadArgs;

// Прототипы функций
int compute_file_blake3(const char *filepath, uint8_t out_hash[BLAKE3_HASH_LEN]);
void display_progress(float progress);

static volatile sig_atomic_t g_shutdown = 0;

static volatile sig_atomic_t g_command_loop_running = 0;

/*
 * Инициализация SSL-контекста для клиента с поддержкой mTLS
 * Загружает корневой сертификат, клиентский сертификат и приватный ключ
 * Возвращает инициализированный SSL-контекст или завершает программу при ошибке
 */
static SSL_CTX *init_client_ssl_ctx(void) {
    // Инициализация библиотеки OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    
    // Создание нового SSL-контекста для клиента
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    
    // Загрузка корневого сертификата для проверки сервера
    if (SSL_CTX_load_verify_locations(ctx, "src/ca.pem", NULL) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    
    // Загрузка клиентского сертификата и приватного ключа для mTLS
    if (SSL_CTX_use_certificate_file(ctx, "src/client-cert.pem", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "src/client-key.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    
    // Настройка обязательной проверки сертификата сервера
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    
    return ctx;
}

/*
 * Гарантированная отправка всех данных через SSL-соединение
 * Возвращает 0 при успехе, -1 при ошибке
 */
static int ssl_send_all(SSL *ssl, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = SSL_write(ssl, (const char *)buf + sent, len - sent);
        if (n <= 0) {
            return -1;  // Ошибка отправки
        }
        sent += n;
    }
    return 0;  // Все данные успешно отправлены
}

/*
 * Гарантированное чтение всех ожидаемых данных через SSL-соединение
 * Возвращает 0 при успехе, -1 при ошибке
 */
static int ssl_recv_all(SSL *ssl, void *buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        int n = SSL_read(ssl, (char *)buf + received, len - received);
        if (n <= 0) {
            return -1;  // Ошибка чтения
        }
        received += n;
    }
    return 0;  // Все данные успешно получены
}

/*
 * Генерация уникального хеша сессии на основе временной метки, PID и случайных данных
 * Хеш сохраняется в глобальной структуре сессии
 */
static void generate_session_hash() {
    pthread_mutex_lock(&g_session.mutex);  // Блокировка доступа к сессии
    
    // Формирование уникальной строки на основе различных идентификаторов
    char unique_str[256];
    snprintf(unique_str, sizeof(unique_str), "%ld_%d_%ld_%ld",
             time(NULL), getpid(), random(), (long)pthread_self());
    
    // Вычисление BLAKE3 хеша
    uint8_t hash[BLAKE3_HASH_LEN];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, unique_str, strlen(unique_str));
    blake3_hasher_finalize(&hasher, hash, BLAKE3_HASH_LEN);
    
    // Преобразование бинарного хеша в hex-строку
    for (int i = 0; i < BLAKE3_HASH_LEN; i++) {
        sprintf(&g_session.session_hash[i*2], "%02x", hash[i]);
    }
    g_session.session_hash[64] = '\0';  // Завершающий ноль
    
    pthread_mutex_unlock(&g_session.mutex);  // Разблокировка доступа
}

/*
 * Получение текущего хеша сессии
 * Возвращает указатель на строку с хешем
 */
const char *get_current_session_hash() {
    pthread_mutex_lock(&g_session.mutex);
    const char* hash = g_session.session_hash;
    pthread_mutex_unlock(&g_session.mutex);
    return hash;
}

/*
 * Получение отпечатка клиентского сертификата текущей сессии
 * Возвращает указатель на строку с отпечатком
 */
const char *get_current_client_fingerprint() {
    pthread_mutex_lock(&g_session.mutex);
    const char* fp = g_session.client_fingerprint;
    pthread_mutex_unlock(&g_session.mutex);
    return fp;
}

/*
 * Установка отпечатка клиентского сертификата в сессии
 * Вызывается после успешной аутентификации
 */
void set_client_fingerprint_in_session(const char *fingerprint) {
    pthread_mutex_lock(&g_session.mutex);
    strncpy(g_session.client_fingerprint, fingerprint, FINGERPRINT_LEN - 1);
    g_session.client_fingerprint[FINGERPRINT_LEN - 1] = '\0';
    g_session.authenticated = 1;  // Установка флага аутентификации
    pthread_mutex_unlock(&g_session.mutex);
}

/*
 * Проверка статуса аутентификации клиента
 * Возвращает 1 если клиент аутентифицирован, 0 в противном случае
 */
int is_client_authenticated_in_session() {
    pthread_mutex_lock(&g_session.mutex);
    int authenticated = g_session.authenticated;
    pthread_mutex_unlock(&g_session.mutex);
    return authenticated;
}

/*
 * Получение текущего SSL-соединения
 * Возвращает указатель на SSL-структуру
 */
SSL *get_current_ssl() {
    pthread_mutex_lock(&g_session.mutex);
    SSL* ssl = g_session.ssl;
    pthread_mutex_unlock(&g_session.mutex);
    return ssl;
}

/*
 * Установка SSL-соединения в сессии
 * Вызывается после успешного подключения к серверу
 */
void set_ssl_in_session(SSL *ssl) {
    pthread_mutex_lock(&g_session.mutex);
    g_session.ssl = ssl;
    g_session.connected = 1;  // Установка флага подключения
    pthread_mutex_unlock(&g_session.mutex);
}

/*
 * Сброс всех данных сессии
 * Вызывается при завершении работы или ошибке подключения
 */
void reset_session() {
    pthread_mutex_lock(&g_session.mutex);
    memset(g_session.session_hash, 0, sizeof(g_session.session_hash));
    memset(g_session.client_fingerprint, 0, sizeof(g_session.client_fingerprint));
    g_session.authenticated = 0;
    g_session.ssl = NULL;
    g_session.connected = 0;
    pthread_mutex_unlock(&g_session.mutex);
}

/*
 * Проверка состояния подключения к серверу
 * Возвращает 1 если подключение установлено, 0 в противном случае
 */
int is_connected() {
    pthread_mutex_lock(&g_session.mutex);
    int connected = g_session.connected;
    pthread_mutex_unlock(&g_session.mutex);
    return connected;
}

/*
 * Загрузка файла на сервер через защищенное SSL-соединение
 * Выполняет проверку целостности данных с помощью BLAKE3 хеша
 * Отображает прогресс загрузки
 * Возвращает 0 при успехе, -1 при ошибке
 */
static int upload_file_ssl(SSL *ssl, const char *local_filepath, const char *remote_filename, const char *recipient) {
    FILE *fp = NULL;
    char buffer[BUFFER_SIZE];
    struct stat st;
    RequestHeader header;
    ResponseHeader response;
    
    // Получение информации о файле
    if (stat(local_filepath, &st) == -1) {
        perror("stat");
        fprintf(stderr, "Ошибка: Не удалось получить размер файла %s\n", local_filepath);
        return -1;
    }
    long long filesize = st.st_size;
    
    // Открытие файла для чтения
    fp = fopen(local_filepath, "rb");
    if (!fp) {
        perror("fopen");
        fprintf(stderr, "Ошибка: Не удалось открыть файл %s для чтения.\n", local_filepath);
        return -1;
    }
    
    // Подготовка заголовка запроса на загрузку
    header.command = CMD_UPLOAD;
    strncpy(header.filename, remote_filename, FILENAME_MAX_LEN - 1);
    header.filename[FILENAME_MAX_LEN - 1] = '\0';
    header.filesize = filesize;
    
    // Вычисление хеша файла для проверки целостности
    uint8_t file_hash[BLAKE3_HASH_LEN];
    if (compute_file_blake3(local_filepath, file_hash) != 0) {
        fprintf(stderr, "Ошибка: Не удалось вычислить хеш для %s\n", local_filepath);
        fclose(fp);
        return -1;
    }
    memcpy(header.file_hash, file_hash, BLAKE3_HASH_LEN);
    
    // Установка получателя если указан
    memset(header.recipient, 0, sizeof(header.recipient));
    if(recipient && strlen(recipient) > 0) {
        strncpy(header.recipient, recipient, FINGERPRINT_LEN - 1);
        header.recipient[FINGERPRINT_LEN - 1] = '\0';
    }
    
    printf("Загрузка '%s' (%lld байт) как '%s'...\n", local_filepath, filesize, remote_filename);
    
    // Отправка заголовка запроса
    if (ssl_send_all(ssl, &header, sizeof(RequestHeader)) == -1) {
        fclose(fp);
        return -1;
    }
    
    // Ожидание подтверждения готовности сервера
    if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) == -1) {
        fclose(fp);
        return -1;
    }
    
    if (response.status != RESP_SUCCESS) {
        fprintf(stderr, "Сервер отклонил загрузку: Статус %d\n", response.status);
        fclose(fp);
        return -1;
    }
    
    printf("Сервер готов к приёму файла. Отправка данных...\n");
    
    // Отправка содержимого файла с отображением прогресса
    long long total_sent = 0;
    ssize_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (ssl_send_all(ssl, buffer, bytes_read) == -1) {
            fprintf(stderr, "Не удалось отправить данные файла.\n");
            fclose(fp);
            return -1;
        }
        total_sent += bytes_read;
        float progress = (float)total_sent / (float)filesize;
        display_progress(progress);
        usleep(10000);  // Небольшая задержка для плавного отображения прогресса
    }
    
    printf("\nЗагрузка завершена!\n");
    
    // Проверка ошибок чтения файла
    if (ferror(fp)) {
        perror("fread");
        fprintf(stderr, "Ошибка чтения локального файла %s.\n", local_filepath);
        fclose(fp);
        return -1;
    }
    
    printf("Данные файла отправлены. Всего: %lld байт.\n", total_sent);
    
    // Получение финального статуса от сервера
    if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) == -1) {
        fclose(fp);
        return -1;
    }
    
    if (response.status == RESP_SUCCESS) {
        printf("Загрузка успешно завершена!\n");
    } else {
        fprintf(stderr, "Загрузка не удалась на сервере: Статус %d\n", response.status);
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}

/*
 * Вычисление хеша BLAKE3 для файла
 * Используется для проверки целостности передаваемых данных
 * Возвращает 0 при успехе, -1 при ошибке
 */
int compute_file_blake3(const char *filepath, uint8_t out_hash[BLAKE3_HASH_LEN]) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return -1;
    
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    
    unsigned char buf[4096];
    size_t bytes;
    
    // Постепенное чтение и хеширование файла
    while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0) {
        blake3_hasher_update(&hasher, buf, bytes);
    }
    
    // Проверка ошибок чтения
    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }
    
    // Финальное вычисление хеша
    blake3_hasher_finalize(&hasher, out_hash, BLAKE3_HASH_LEN);
    fclose(fp);
    return 0;
}

/*
 * Скачивание файла с сервера через защищенное SSL-соединение
 * Отображает прогресс загрузки
 * Возвращает 0 при успехе, -1 при ошибке
 */
static int download_file_ssl(SSL *ssl, const char *remote_filename, const char *local_filepath) {
    FILE *fp = NULL;
    char buffer[BUFFER_SIZE];
    RequestHeader header;
    ResponseHeader response;
    
    // Подготовка заголовка запроса на скачивание
    header.command = CMD_DOWNLOAD;
    strncpy(header.filename, remote_filename, FILENAME_MAX_LEN - 1);
    header.filename[FILENAME_MAX_LEN - 1] = '\0';
    header.filesize = 0;
    header.offset = 0;  // Начало файла
    
    printf("Запрос файла '%s' для сохранения в '%s'...\n", remote_filename, local_filepath);
    
    // Отправка запроса на скачивание
    if (ssl_send_all(ssl, &header, sizeof(RequestHeader)) == -1) {
        return -1;
    }
    
    // Получение метаданных файла от сервера
    if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) == -1) {
        return -1;
    }
    
    if (response.status != RESP_SUCCESS) {
        fprintf(stderr, "Сервер отклонил запрос на скачивание: Статус %d\n", response.status);
        return -1;
    }
    
    long long filesize = response.filesize;
    if (filesize <= 0) {
        fprintf(stderr, "Сервер сообщил о недопустимом размере файла (%lld) для скачивания.\n", filesize);
        return -1;
    }
    
    printf("Сервер имеет файл '%s' (%lld байт). Начало скачивания...\n", remote_filename, filesize);
    
    // Открытие файла для записи
    fp = fopen(local_filepath, "wb");
    if (!fp) {
        perror("fopen");
        fprintf(stderr, "Ошибка: Не удалось открыть файл %s для записи.\n", local_filepath);
        return -1;
    }
    
    // Прием содержимого файла с отображением прогресса
    long long total_received = 0;
    while (total_received < filesize) {
        size_t bytes_to_read = (filesize - total_received < BUFFER_SIZE) ?
                              (size_t)(filesize - total_received) : BUFFER_SIZE;
                              
        int bytes_received = SSL_read(ssl, buffer, bytes_to_read);
        if (bytes_received <= 0) {
            perror("SSL_read");
            fclose(fp);
            return -1;
        }

        // Запись полученных данных в файл
        if (fwrite(buffer, 1, bytes_received, fp) != (size_t)bytes_received) {
            perror("fwrite");
            fprintf(stderr, "Ошибка записи в локальный файл %s.\n", local_filepath);
            fclose(fp);
            return -1;
        }
        
        total_received += bytes_received;
        float progress = (float)total_received / (float)filesize;
        display_progress(progress);
    }
    
    // Проверка ошибок записи файла
    if (ferror(fp)) {
        perror("ferror");
        fprintf(stderr, "Ошибка во время записи файла %s.\n", local_filepath);
        fclose(fp);
        return -1;
    }
    
    printf("Скачивание успешно завершено! Сохранено в '%s'. Всего: %lld байт.\n",
           local_filepath, total_received);
    
    fclose(fp);
    return 0;
}

/*
 * Отображение прогресс-бара для визуализации процесса передачи
 * Принимает значение прогресса от 0.0 до 1.0
 */
void display_progress(float progress) {
    // Ограничение прогресса диапазоном 0.0 - 1.0
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
    // Вычисление заполненной части прогресс-бара
    int filled_length = (int)(progress * BAR_LENGTH);
    char bar[BAR_LENGTH + 1];  // +1 для завершающего нуля
    
    // Заполнение прогресс-бара символами
    memset(bar, '#', filled_length);
    memset(bar + filled_length, ' ', BAR_LENGTH - filled_length);
    bar[BAR_LENGTH] = '\0';
    
    // Формирование строки для вывода
    char output[100];
    sprintf(output, "[%s] %.1f%%\n", bar, progress * 100.0);
    
    system(CLEAR);  // Очистка экрана
    printf("%s", output);  // Вывод прогресс-бара
}

/*
 * Запрос списка файлов с сервера
 * Возвращает 0 при успехе, -1 при ошибке
 */
static int list_files_ssl(SSL *ssl) {
    RequestHeader header;
    ResponseHeader response;
    
    // Подготовка заголовка запроса списка файлов
    header.command = CMD_LIST;
    header.filename[0] = '\0';
    header.filesize = 0;
    
    printf("Запрос списка файлов с сервера...\n");
    
    // Отправка запроса
    if (ssl_send_all(ssl, &header, sizeof(RequestHeader)) == -1) {
        return -1;
    }
    
    // Получение метаданных списка
    if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) == -1) {
        return -1;
    }
    
    if (response.status != RESP_SUCCESS) {
        fprintf(stderr, "Сервер отклонил запрос на получение списка: Статус %d\n", response.status);
        return -1;
    }
    
    long long list_len = response.filesize;
    if (list_len <= 0) {
        printf("На сервере нет файлов.\n");
        return 0;
    }
    
    printf("Список файлов с сервера (%lld байт):\n", list_len);
    
    // Прием и отображение списка файлов
    long long total_received = 0;
    char buffer[BUFFER_SIZE];
    
    while (total_received < list_len) {
        size_t bytes_to_read = (list_len - total_received < BUFFER_SIZE) ?
                              (size_t)(list_len - total_received) : BUFFER_SIZE;
                              
        int bytes_received = SSL_read(ssl, buffer, bytes_to_read);
        if (bytes_received <= 0) {
            perror("SSL_read");
            return -1;
        }

        // Вывод полученных данных
        fwrite(buffer, 1, bytes_received, stdout);
        total_received += bytes_received;
    }
    
    printf("\n");
    return 0;
}

/*
 * Функция потока для установки и поддержания соединения с сервером
 * Выполняет повторные попытки подключения при сбоях
 */

// Функция потока для чтения команд пользователя 
void *command_reader_thread(void* arg) {
    (void) arg; // Не используется
    char *input = NULL;
    char cmd[32];
    char arg1[FILENAME_MAX_LEN];
    char arg2[FILENAME_MAX_LEN];
    char arg3[FINGERPRINT_LEN]; // Для получателя

    g_command_loop_running = 1;
    while (g_command_loop_running) {
        input = readline("file_exchange> ");
        if (!input) {
            // EOF (Ctrl+D)
            printf("");
            break;
        }

        if (strlen(input) > 0) {
            add_history(input); // Добавить в историю readline
        }

        // Парсим команду
        int parsed_args = sscanf(input, "%31s %255s %255s %64s", cmd, arg1, arg2, arg3);

        if (parsed_args == 0) {
            free(input);
            continue; // Пустая строка
        }

        if (strcmp(cmd, "upload") == 0) {
            if (parsed_args < 3) {
                printf("Usage: upload <local_file> <remote_name> [recipient_fingerprint]");
                free(input);
                continue;
            }
            const char *recipient = (parsed_args == 4) ? arg3 : "";
            SSL *current_ssl = get_current_ssl();
            if (current_ssl && is_connected()) {
                 // upload_file_ssl ожидает SSL соединение, установленное в сессии
                 int res = upload_file_ssl(current_ssl, arg1, arg2, recipient);
                 printf("Upload %s.", res == 0 ? "successful" : "failed");
            } else {
                printf("Error: Not connected or SSL session not ready.");
            }
        } else if (strcmp(cmd, "download") == 0) {
            if (parsed_args < 3) {
                printf("Usage: download <remote_name> <local_file>");
                free(input);
                continue;
            }
            SSL *current_ssl = get_current_ssl();
            if (current_ssl && is_connected()) {
                int res = download_file_ssl(current_ssl, arg1, arg2);
                printf("Download %s.", res == 0 ? "successful" : "failed");
            } else {
                printf("Error: Not connected or SSL session not ready.");
            }
        } else if (strcmp(cmd, "list") == 0) {
            if (parsed_args != 1) {
                printf("Usage: list");
                free(input);
                continue;
            }
            SSL *current_ssl = get_current_ssl();
            if (current_ssl && is_connected()) {
                int res = list_files_ssl(current_ssl);
                printf("List %s.", res == 0 ? "fetched" : "failed");
            } else {
                printf("Error: Not connected or SSL session not ready.");
            }
        } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            printf("Disconnecting...");
            g_command_loop_running = 0;
            reset_session(); // Сигнализируем основному потоку о завершении
            break;
        } else {
            printf("Unknown command: %s. Available: upload, download, list, exit/quit", cmd);
        }
        free(input);
    }
    return NULL;
}



/*
 * Вывод стартового логотипа приложения
 */
void print_startup_logo(void) {
    printf("\n");
    printf(" // // $$ /\\n");
    printf("∣ / ∣ | _____/ | \\n");
    printf("| $$ /$$ /$$ /$$$ | $$$ | / // $$$| $ $$ /$$ / $$$ /$ $ /$$ \\n");
    printf("∣ / /$$__ $$ / /| $$ ∣ $ | $$ / /_____/| |____ | __ /__ /__ \\n");
    printf("| ∣ ∣ $$$ | $$ | \\n");
    printf(" | __/ \\ /∣ | \\n");
    printf(" /$$$ | \\n");
    printf(" | \\n");
    printf(" | $$$ \\n");
    printf("∣ \\n");
    printf(" | | $$_____/ \\____ $$| | ∣ > | ∣ | /$$__ $$| | ∣ | | $$_____/\\n");
    printf("| $$ \\/ | $$$ /$$$ /| ∣ | $$$ / / \\n");
    printf(" ∣ $$ ∣ | $$$ | ∣ | $$$ | $$$ \\n");
    printf("|/ |/ \\/|/ |/ |/|_/|/ \\_/ \\___/|/ |/ \\_/|/ |_/ \\__ $$ \\______/\\n");
    printf(" / \\n");
    printf(" | $$ \\n");
    printf(" \\______/ \\n");
    printf("\n");
}


// --- Функция потока для чтения ответов от сервера ---
void *response_reader_thread(void* arg) {
    SSL *ssl = (SSL*)arg;
    ResponseHeader response;
    while (is_connected()) { // Проверяем, подключены ли мы
        if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) != 0) {
            fprintf(stderr, "Failed to read response header from server.");
            reset_session(); // Сигнализируем основному потоку об ошибке
            break;
        }
        // Обработка ответа (например, отображение статуса)
        switch(response.status) {
            case RESP_SUCCESS:
                printf("Server response: Success.");
                break;
            case RESP_ERROR:
                printf("Server response: Generic Error.");
                break;
            case RESP_PERMISSION_DENIED:
                printf("Server response: Permission Denied.");
                break;
            case RESP_FILE_NOT_FOUND:
                printf("Server response: File Not Found.");
                break;
            case RESP_INTEGRITY_ERROR:
                printf("Server response: Integrity Error.");
                break;
            case RESP_UNKNOWN_COMMAND:
                printf("Server response: Unknown Command.");
                break;
            case RESP_WAITING_APPROVAL:
                printf("Server response: Waiting for admin approval...");
                break;
            case RESP_APPROVED:
                printf("Server response: Connection approved! You are now authenticated.");
                // Тут можно, например, разблокировать ввод команд
                break;
            case RESP_REJECTED:
                printf("Server response: Connection rejected. Disconnecting...");
                reset_session(); // Сигнализируем основному потоку об отклонении
                break;
            default:
                printf("Server response: Unknown status %d.", response.status);
                break;
        }
        // Если ответ содержит данные (например, список файлов), их нужно читать отдельно
        // Это зависит от команды. CMD_LIST, например, отправляет размер, а затем данные.
        // Для простоты, предположим, что все команды, кроме CMD_LIST, не требуют чтения данных.
        // CMD_LIST уже обрабатывается в list_files_ssl, которая вызывается из command_reader_thread.
    }
    return NULL;
}

static void *connection_thread(void* arg) {
    ThreadArgs *args = (ThreadArgs*)arg;
    struct sockaddr_in serv_addr;
    int sock;
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    pthread_t cmd_thread, resp_thread;

    while (!g_shutdown) { // Используем глобальный флаг, если он есть, или просто while(1)
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            fprintf(stderr, "Не удалось создать сокет в потоке");
            sleep(2);
            continue;
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(args->port);
        if (inet_pton(AF_INET, args->ip, &serv_addr.sin_addr) <= 0) {
            fprintf(stderr, "Неверный адрес в потоке");
            close(sock);
            sleep(2);
            continue;
        }

        printf("Попытка подключения к %s:%d...", args->ip, args->port);
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            fprintf(stderr, "Не удалось подключиться к серверу: %s. Повтор через 2 секунды...", strerror(errno));
            close(sock);
            sleep(2);
            continue;
        }

        if (!ctx) {
            ctx = init_client_ssl_ctx();
            if (!ctx) {
                fprintf(stderr, "Не удалось инициализировать SSL контекст в потоке");
                close(sock);
                sleep(2);
                continue;
            }
        }

        ssl = SSL_new(ctx);
        if (!ssl) {
            fprintf(stderr, "Не удалось создать SSL объект в потоке");
            close(sock);
            sleep(2);
            continue;
        }

        SSL_set_fd(ssl, sock);
        if (SSL_connect(ssl) <= 0) {
            fprintf(stderr, "Не удалось выполнить SSL handshake с сервером в потоке");
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(sock);
            sleep(2);
            continue;
        }

        printf("Успешное подключение к серверу через SSL в потоке");

        X509 *client_cert = SSL_get_certificate(ssl);
        if (client_cert) {
            unsigned char cert_hash[SHA256_DIGEST_LENGTH];
            X509_digest(client_cert, EVP_sha256(), cert_hash, NULL);
            char client_fingerprint[FINGERPRINT_LEN];
            for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
                sprintf(&client_fingerprint[i*2], "%02x", cert_hash[i]);
            }
            client_fingerprint[FINGERPRINT_LEN - 1] = '\0';
            X509_free(client_cert);
            set_client_fingerprint_in_session(client_fingerprint);
            printf("Отпечаток клиента: %s", client_fingerprint);
        }

        generate_session_hash();
        printf("Сессионный ключ сгенерирован: %s", get_current_session_hash());

        set_ssl_in_session(ssl);

        // --- Отправка команды CMD_CONNECT ---
        RequestHeader connect_header = { .command = CMD_CONNECT };
        strncpy(connect_header.filename, "connect_handshake", FILENAME_MAX_LEN - 1);
        connect_header.filename[FILENAME_MAX_LEN - 1] = '\0';
        connect_header.filesize = 0;
        memset(connect_header.file_hash, 0, BLAKE3_HASH_LEN);
        memset(connect_header.recipient, 0, FINGERPRINT_LEN);

        if (ssl_send_all(ssl, &connect_header, sizeof(RequestHeader)) != 0) {
            fprintf(stderr, "Не удалось отправить CMD_CONNECT.");
            reset_session();
            break;
        }
        printf("CMD_CONNECT отправлен. Ожидание подтверждения администратора...");

        // --- Цикл ожидания подтверждения ---
        ResponseHeader approval_resp;
        while (is_connected()) { // Проверяем, не сбросили ли сессию где-то ещё
            if (ssl_recv_all(ssl, &approval_resp, sizeof(ResponseHeader)) != 0) {
                fprintf(stderr, "Ошибка при получении ответа на CMD_CONNECT.");
                reset_session();
                break;
            }

            if (approval_resp.status == RESP_APPROVED) {
                printf("Подключение успешно подтверждено сервером!");
                break; // Выходим из цикла ожидания
            } else if (approval_resp.status == RESP_REJECTED) {
                printf("Подключение отклонено сервером.");
                reset_session();
                break; // Выходим из цикла ожидания и соединения
            } else if (approval_resp.status == RESP_WAITING_APPROVAL) {
                printf("Сервер ожидает подтверждения администратора..."); // Продолжаем ждать
            } else {
                printf("Неожиданный статус при ожидании подтверждения: %d", approval_resp.status);
                reset_session();
                break;
            }
        }

        if (!is_connected()) {
             printf("Соединение разорвано во время ожидания подтверждения.");
             break; // Выходим из основного цикла подключения
        }

        // --- Запуск потоков для обмена командами ---
        if (pthread_create(&cmd_thread, NULL, command_reader_thread, NULL) != 0) {
            fprintf(stderr, "Не удалось создать поток ввода команд.");
            reset_session();
            break;
        }
        if (pthread_create(&resp_thread, NULL, response_reader_thread, ssl) != 0) {
            fprintf(stderr, "Не удалось создать поток чтения ответов.");
            g_command_loop_running = 0; // Останавливаем ввод команд
            pthread_join(cmd_thread, NULL);
            reset_session();
            break;
        }

        // Ждём завершения потоков
        pthread_join(cmd_thread, NULL);
        pthread_join(resp_thread, NULL);

        // После завершения потоков, сессия уже сброшена в command_reader_thread или response_reader_thread
        break; // Выходим из цикла подключения
    }

    if (ctx) SSL_CTX_free(ctx);
    free(args);
    return NULL;
}

/*
 * Основная функция программы
 * Обрабатывает аргументы командной строки, запускает поток подключения
 * и выполняет запрошенную пользователем операцию
 */

 int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    char server_ip[16] = "127.0.0.1";
    int opt;
    int result = EXIT_FAILURE;

    struct option long_options[] = {
        {"ip",    required_argument, 0, 'i'},
        {"port",  required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "i:p:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                strncpy(server_ip, optarg, 15);
                server_ip[15] = '\0';
                break;
            case 'p':
                errno = 0;
                long val = strtol(optarg, NULL, 10);
                if (errno != 0 || val <= 0 || val > 65535) {
                    fprintf(stderr, "Ошибка: Неверный номер порта '%s'. Порт должен быть от 1 до 65535.", optarg);
                    return EXIT_FAILURE;
                }
                port = (int)val;
                break;
            default:
                fprintf(stderr, "Использование: %s [--ip <IP>] [--port <PORT>] <команда> [аргументы...]", argv[0]);
                fprintf(stderr, "Команды:");
                fprintf(stderr, "  connect - Подключиться и ждать подтверждения");
                fprintf(stderr, "  upload <локальный_файл> <имя_на_сервере> [отпечаток_получателя]");
                fprintf(stderr, "  download <имя_на_сервере> <локальный_файл>");
                fprintf(stderr, "  list");
                return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        print_startup_logo();
        fprintf(stderr, "Ошибка: Не указана команда.");
        fprintf(stderr, "Использование: %s [--ip <IP>] [--port <PORT>] <команда> [аргументы...]", argv[0]);
        fprintf(stderr, "Команды:");
        fprintf(stderr, "  connect - Подключиться и ждать подтверждения");
        fprintf(stderr, "  upload <локальный_файл> <имя_на_сервере> [отпечаток_получателя]");
        fprintf(stderr, "  download <имя_на_сервере> <локальный_файл>");
        fprintf(stderr, "  list");
        return EXIT_FAILURE;
    }

    char *cmd_str = argv[optind];

    print_startup_logo();
    printf("Подключение к %s:%d...", server_ip, port);

    if (strcmp(cmd_str, "connect") == 0) {
        if (argc != optind + 1) { // Команда connect не принимает аргументов
             fprintf(stderr, "Использование: %s [--ip <IP>] [--port <PORT>] connect", argv[0]);
             return EXIT_FAILURE;
        }
        ThreadArgs* thread_args = malloc(sizeof(ThreadArgs));
        if (!thread_args) {
            fprintf(stderr, "Ошибка: Не удалось выделить память для аргументов потока.");
            return EXIT_FAILURE;
        }
        thread_args->port = port;
        strncpy(thread_args->ip, server_ip, 15);
        thread_args->ip[15] = '\0';

        pthread_t conn_thread;
        if (pthread_create(&conn_thread, NULL, connection_thread, thread_args) != 0) {
            fprintf(stderr, "Ошибка: Не удалось создать поток подключения");
            free(thread_args);
            return EXIT_FAILURE;
        }

        printf("Ожидание подключения к серверу...");
        while (!is_connected()) {
            usleep(100000); // 100ms
        }
        printf("Подключено к серверу. Сессионный ключ: %s", get_current_session_hash());
        printf("Ожидание подтверждения администратора...");

        pthread_join(conn_thread, NULL);
        result = EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Команда '%s' не поддерживается в этом режиме. Используйте 'connect' для начального подключения.", cmd_str);
        result = EXIT_FAILURE;
    }

    return result;
}