
/**
 *
 *
 *
 * @file server.c
 * @author oxxy
 * @brief file server exchange file, crypto AES-256-GCM sertificate BLAKE3, openssl, mongodb
 * @version 0.1
 * @date 2025-10-27
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

// --- Заголовочные файлы ---
// Ошибки, сигналы и системные константы
#include <errno.h>
#include <signal.h>
#include <linux/limits.h>

// OpenSSL: ошибки, хеши, EVP API, генерация случайных байт, TLS
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

// Для форматированного вывода с переменным числом аргументов
#include <stdarg.h>

// Стандартные типы и IO
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h> // inet_ntoa и сетевые структуры
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

// MongoDB C driver и BSON
#include <mongoc/mongoc.h>
#include <bson/bson.h>

// OpenSSL TLS API
#include <openssl/ssl.h>

#define BLAKE3_IMPLEMENTATION
#include "blake3.h"

// Подмодули
#include "../db/mongo_ops_server.h"
#include "../../include/protocol.h"
#include "../crypto/aes_gcm.h"

// Конфигурация
#define PORT 6515 // порт, на котором слушает сервер
#define BUFFER_SIZE 4096 // размер буфера для операций ввода-вывода
#define MAX_KEY_LENGTH 32 // максимальная длина ключа (байты)
#define LOG_FILE "/tmp/file-server.log" // файл для логов по умолчанию
#define MONGODB_URI "mongodb://localhost:27017" // строка подключения к MongoDB
#define DATABASE_NAME "file_exchange" // имя БД
#define COLLECTION_NAME "file_groups" // имя коллекции
#define STORAGE_DIR "filetrade" // путь к каталогу хранения
#define MAX_USERS_LISTEN 3     // указываем сколько подключений слушаем.

// Hello world 
// Уровни логирования
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;
// Глобальные переменные

// Флаг завершения работы, устанавливаемый обработчиком сигнала (например, SIGINT).
// Объявлен как volatile sig_atomic_t для безопасного использования в обработчиках сигналов.
static volatile sig_atomic_t g_shutdown = 0;

// Глобальный клиент MongoDB — создаётся один раз при старте приложения и используется во всём проекте.
mongoc_client_t *g_mongo_client = NULL;

// Глобальная ссылка на коллекцию MongoDB, в которую записываются метаданные файлов.
mongoc_collection_t *g_collection = NULL;

// Контекст OpenSSL для настройки TLS-соединений. Инициализируется один раз и используется всеми клиентами.
static SSL_CTX *g_ssl_ctx = NULL;

// Указатель на файл журнала. Открывается при запуске и закрывается при завершении работы.
static FILE *g_log_file = NULL;


// Структура для хранения контекста шифрования файлов.
// Содержит 256-битный ключ (32 байта) и флаг инициализации.
typedef struct {
    uint8_t key[32];        // Симметричный ключ (например, для AES-256)
    int initialized;        // 1 — ключ задан, 0 — ключ не инициализирован
} file_crypto_ctx_t;

// Глобальный экземпляр контекста шифрования файлов. Инициализируется нулевыми значениями.
static file_crypto_ctx_t g_file_crypto = {0};


// Структура с информацией о подключённом клиенте.
typedef struct {
    int client_socket;              // Сокет клиента
    struct sockaddr_in client_addr; // Адрес клиента (IPv4)
    SSL *ssl;                       // SSL-соединение с клиентом (для шифрования трафика)
    char fingerprint[65];           // SHA-256 отпечаток сертификата клиента в шестнадцатеричном виде (64 символа + '\0')
} client_info_t;


// Функция логирования: выводит сообщение одновременно в файл и в терминал (stderr).
// Поддерживает цветовую индикацию уровней логирования в консоли.
static void logger(log_level_t level, const char *format, ...) {
    // Строковые представления уровней логирования
    const char *level_str[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

    // ANSI-цвета для терминала: циан, зелёный, жёлтый, красный
    const char *color[] = {"\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m"};

    // Сброс цветовой настройки терминала
    const char *color_reset = "\x1b[0m";

    // Получаем текущее локальное время для временной метки
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    // Формируем строку сообщения с поддержкой переменного числа аргументов (как printf)
    char msgbuf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(msgbuf, sizeof(msgbuf), format, args);  // Безопасная версия vsprintf
    va_end(args);

    // Если файл лога открыт — записываем туда сообщение
    if (g_log_file) {
        fprintf(g_log_file, "[%s] [%s] %s\n", timestamp, level_str[level], msgbuf);
        fflush(g_log_file);  // Принудительно сбрасываем буфер на диск
    }

    // Одновременно выводим сообщение в stderr с цветовой маркировкой уровня
    fprintf(stderr, "%s[%s] [%s] %s%s\n",
            color[level],          // Цвет перед сообщением
            timestamp,             // Временная метка
            level_str[level],      // Уровень логирования
            msgbuf,                // Текст сообщения
            color_reset);          // Сброс цвета в конце
}

// Анимированный ASCII-логотип при запуске программы
static void print_startup_logo(void) {
    // Массив строк, представляющих построчный вывод логотипа.
    // Каждая строка — отдельный "кадр", но на самом деле это просто построчная анимация.
    // Последний элемент — NULL, чтобы обозначить конец массива.
    const char *frames[] = {
            "  __  __  __  __  _____  _   __  __  _   _  _____ \n",
            " |  \/  ||  \/  ||  __ \| | |  \/  || | | ||  __ \ \n",
            " | \  / || \  / || |__) | | | \  / || |_| || |__) |\n",
            " | |\/| || |\/| ||  ___/| | | |\/| ||  _  ||  _  / \n",
            " | |  | || |  | || |    |_| | |  | || | | || | \ \ \n",
            " |_|  |_||_|  |_||_|    (_) |_|  |_||_| |_||_|  \_\ \n",
        NULL
    };

    // Выводим каждую строку логотипа по очереди с небольшой задержкой для эффекта "появления"
    for (int i = 0; frames[i]; ++i) {
        fprintf(stderr, "%s", frames[i]);
        fflush(stderr);               // Принудительно сбрасываем буфер, чтобы строка сразу отобразилась
        usleep(120000);              // Задержка 120 мс между строками
    }
    // После логотипа выводим заголовок приложения жирным шрифтом (ANSI escape-код \x1b[1m)
    fprintf(stderr, "\x1b[1mMeshExchange\x1b[0m - starting up...\n\n");
    fflush(stderr);
}


// Функция для красивой загрузки модулей с имитацией прогресса через анимацию точек
static void print_module_loading(const char **modules, size_t count) {
    // Проходим по списку модулей и показываем "загрузку" каждого
    for (size_t i = 0; i < count; ++i) {
        // Выводим номер текущего модуля и его название
        fprintf(stderr, "[ %2zu/%2zu ] Loading %-20s", i+1, count, modules[i]);
        fflush(stderr);

        // Простая анимация: выводим 6 точек с паузой между ними
        for (int t = 0; t < 6; ++t) {
            fprintf(stderr, ".");
            fflush(stderr);
            usleep(120000); // Задержка 120 мс
        }

        // После анимации выводим "Done" и переходим к следующей строке
        fprintf(stderr, "   Done\n");
        fflush(stderr);
    }
    // Добавляем пустую строку после завершения загрузки всех модулей
    fprintf(stderr, "\n");
}


// Функция для извлечения расширения файла из полного пути
// Возвращает дубликат строки с расширением (включая точку), который должен быть освобождён вызывающей стороной.
static char* get_file_extension(const char *full_path) {
    // Защита от пустого указателя
    if (!full_path) return NULL;

    // Находим последний символ '/' в пути — всё после него считается именем файла
    const char *filename = strrchr(full_path, '/');
    if (!filename) filename = full_path; // если '/' нет, то весь путь — это имя файла
    else filename++;                     // пропускаем сам символ '/'

    // Ищем последнюю точку в имени файла — она указывает на начало расширения
    const char *dot = strrchr(filename, '.');
    // Если точки нет, или она первая (например, ".gitignore"), считаем, что расширения нет
    if (!dot || dot == filename) {
        return strdup(""); // Возвращаем пустую строку
    }

    // Создаём копию расширения (начиная с точки) и возвращаем её
    return strdup(dot);
}


// Функция для получения имени файла без расширения
// Принимает полный путь или просто имя файла.
// Возвращает выделенную в куче строку с именем без расширения (требует free).
static char* get_filename_without_extension(const char* full_filename) {
    // Извлекаем имя файла из пути (всё после последнего '/')
    const char *filename = strrchr(full_filename, '/');
    if (!filename) filename = full_filename;
    else filename++;

    // Ищем последнюю точку — она разделяет имя и расширение
    const char *dot = strrchr(filename, '.');
    // Если точки нет, длина имени — вся строка; иначе — до точки
    size_t name_len = dot ? (size_t)(dot - filename) : strlen(filename);

    // Выделяем память под результат (длина + 1 для завершающего нуля)
    char *result = malloc(name_len + 1);
    if (!result) {
        logger(LOG_ERROR, "Memory allocation failed in get_filename_without_extension");
        return NULL;
    }
    
    // Копируем только часть до точки
    strncpy(result, filename, name_len);
    result[name_len] = '\0';
    // Заполняем буфер и завершаем нулём
    memcpy(result, filename, name_len);
    result[name_len] = '\0';
    return result;
}


// Функция для вычисления хеша BLAKE3 от произвольного буфера данных
// Принимает: указатель на данные, их длину и буфер для результата (должен быть размером BLAKE3_HASH_LEN)
static void compute_buffer_blake3(const uint8_t *data, size_t len, uint8_t out_hash[BLAKE3_HASH_LEN]) {
    // Создаём и инициализируем hasher
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    // Добавляем данные в hasher
    blake3_hasher_update(&hasher, data, len);
    // Завершаем хеширование и записываем результат в out_hash
    blake3_hasher_finalize(&hasher, out_hash, BLAKE3_HASH_LEN);
}
// Получение следующего ключа для поля "proc" в документе MongoDB.
// Поле "proc" представляет собой объект, где ключи — это строки с числами ("1", "2", ...),
// а значения — события обработки файла. Функция находит максимальный числовой ключ и возвращает следующий.
static char* get_next_proc_key(const char *file_id) {
    // Получаем указатель на коллекцию MongoDB по глобальному клиенту и заданным имени БД и коллекции
    mongoc_collection_t *coll = mongoc_client_get_collection(
        g_mongo_client, DATABASE_NAME, COLLECTION_NAME);
    if (!coll) {
        logger(LOG_ERROR, "Failed to get collection for file: %s", file_id);
        return NULL;
    }
    
    // Формируем запрос: ищем документ по _id == file_id
    bson_t *query = BCON_NEW("_id", BCON_UTF8(file_id));
    // Проекция: запрашиваем только поле "proc"
    bson_t *projection = BCON_NEW("proc", BCON_INT32(1));
    bson_error_t error;
    
    char *next_key = NULL;
    const bson_t *doc;
    // Выполняем запрос на чтение одного документа (find)
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        coll, query, NULL, NULL);
    
    int64_t max_key = 0; // Будем искать максимальный числовой ключ в "proc"
    
    // Пытаемся получить документ
    if (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;
        // Ищем поле "proc" и проверяем, что оно — документ (объект)
        if (bson_iter_init_find(&iter, doc, "proc") && 
            BSON_ITER_HOLDS_DOCUMENT(&iter)) {
            
            // Рекурсивно проходим по всем полям объекта "proc"
            bson_iter_t child;
            bson_iter_recurse(&iter, &child);
            
            while (bson_iter_next(&child)) {
                // Имя поля — потенциальный числовой ключ
                const char *key_str = bson_iter_key(&child);
                char *endptr;
                // Пытаемся преобразовать его в число
                long num = strtol(key_str, &endptr, 10);
                
                // Если преобразование прошло успешно и число положительное — обновляем максимум
                if (errno == 0 && *endptr == '\0' && num > 0 && num > max_key) {
                    max_key = num;
                }
            }
        }
    } else {
        // Документ не найден — начинаем с ключа "1"
        logger(LOG_DEBUG, "No existing document found for: %s, starting from key 1", file_id);
    }
    
    // Проверяем, были ли ошибки в курсоре
    if (mongoc_cursor_error(cursor, &error)) {
        logger(LOG_ERROR, "Cursor error for file %s: %s", file_id, error.message);
    }
    
    // Освобождаем ресурсы MongoDB
    mongoc_cursor_destroy(cursor);
    bson_destroy(query);
    bson_destroy(projection);
    mongoc_collection_destroy(coll);
    
    // Выделяем память под строку с новым ключом
    next_key = malloc(MAX_KEY_LENGTH);
    if (!next_key) {
        logger(LOG_ERROR, "Memory allocation failed for next_key");
        return NULL;
    }
    
    // Формируем следующий ключ как строку (max_key + 1)
    snprintf(next_key, MAX_KEY_LENGTH, "%" PRId64, max_key + 1);
    return next_key;
}


// Создание базового документа файла в MongoDB при первом обращении.
// Документ содержит метаданные: путь, имя без расширения, расширение и пустой объект "proc".
static bool create_base_document(const char *fullpath) {
    // Получаем коллекцию MongoDB
    mongoc_collection_t *coll = mongoc_client_get_collection(
        g_mongo_client, DATABASE_NAME, COLLECTION_NAME);
    if (!coll) {
        logger(LOG_ERROR, "Failed to get collection for base document: %s", fullpath);
        return false;
    }
    
    // Извлекаем имя файла и расширение из полного пути
    char *filename = get_filename_without_extension(fullpath);
    char *extension = get_file_extension(fullpath);
    
    if (!filename || !extension) {
        logger(LOG_ERROR, "Failed to parse filename/extension for: %s", fullpath);
        free(filename);
        free(extension);
        mongoc_collection_destroy(coll);
        return false;
    }
    
    // Формируем новый BSON-документ
    bson_t *doc = bson_new();
    BSON_APPEND_UTF8(doc, "_id", fullpath);               // _id — полный путь к файлу
    BSON_APPEND_UTF8(doc, "filename", filename);          // Имя без расширения
    BSON_APPEND_UTF8(doc, "extension", extension);        // Расширение (с точкой)
    
    // Инициализируем поле "proc" как пустой объект
    bson_t proc;
    bson_init(&proc);
    BSON_APPEND_DOCUMENT(doc, "proc", &proc);
    bson_destroy(&proc);
    
    bson_error_t error;
    // Пытаемся вставить документ в коллекцию
    bool success = mongoc_collection_insert_one(coll, doc, NULL, NULL, &error);
    
    if (!success) {
        // Ошибка 11000 — дубликат ключа (_id уже существует). Это нормально.
        if (error.code != 11000) {
            logger(LOG_ERROR, "Failed to create base document for %s: %s", fullpath, error.message);
        } else {
            logger(LOG_DEBUG, "Base document already exists for: %s", fullpath);
            success = true; // Считаем операцию успешной
        }
    } else {
        logger(LOG_INFO, "Created base document for: %s", fullpath);
    }
    
    // Освобождаем ресурсы
    bson_destroy(doc);
    free(filename);
    free(extension);
    mongoc_collection_destroy(coll);
    
    return success;
}


// Добавление нового события обработки в поле "proc" документа файла.
// Событие включает тип изменения (например, "modified") и статус (например, "encrypted").
static bool append_proc_event(const char *file_id, const char *change_type, const char *status) {
    // Убеждаемся, что базовый документ существует (создаём, если нужно)
    if (!create_base_document(file_id)) {
        logger(LOG_ERROR, "Failed to ensure base document for: %s", file_id);
        return false;
    }
    
    // Получаем коллекцию MongoDB
    mongoc_collection_t *coll = mongoc_client_get_collection(
        g_mongo_client, DATABASE_NAME, COLLECTION_NAME);
    if (!coll) {
        logger(LOG_ERROR, "Failed to get collection for event: %s", file_id);
        return false;
    }
    
    // Получаем следующий уникальный ключ для события в "proc"
    char *next_key = get_next_proc_key(file_id);
    if (!next_key) {
        logger(LOG_ERROR, "Failed to get next proc key for: %s", file_id);
        mongoc_collection_destroy(coll);
        return false;
    }
    
    // Получаем текущее время в миллисекундах с начала эпохи
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t now_ms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    
    // Формируем путь обновления вида "proc.42"
    char set_path[64];
    snprintf(set_path, sizeof(set_path), "proc.%s", next_key);
    
    // Создаём вложенный документ события
    bson_t event_doc;
    bson_init(&event_doc);
    BSON_APPEND_DATE_TIME(&event_doc, "date", now_ms); // Время события
    
    // Документ "info" внутри события
    bson_t info_doc;
    bson_init(&info_doc);
    BSON_APPEND_UTF8(&info_doc, "type_of_changes", change_type);
    BSON_APPEND_UTF8(&info_doc, "status", status);
    BSON_APPEND_DOCUMENT(&event_doc, "info", &info_doc);
    
    // Формируем операцию обновления: $set: { "proc.<next_key>": event_doc }
    bson_t *update = BCON_NEW("$set", "{", set_path, BCON_DOCUMENT(&event_doc), "}");
    bson_t *query = BCON_NEW("_id", BCON_UTF8(file_id));
    
    bson_error_t error;
    // Выполняем обновление одного документа
    bool success = mongoc_collection_update_one(
        coll, query, update, NULL, NULL, &error);
    
    if (!success) {
        logger(LOG_ERROR, "Failed to append proc event for %s: %s", file_id, error.message);
    } else {
        logger(LOG_INFO, "Added event %s to %s: %s - %s", next_key, file_id, change_type, status);
    }
    
    // Освобождаем все выделенные BSON-структуры и память
    bson_destroy(&info_doc);
    bson_destroy(&event_doc);
    bson_destroy(update);
    bson_destroy(query);
    free(next_key);
    mongoc_collection_destroy(coll);
    
    return success;
}


// Надёжная отправка данных через SSL-соединение.
// Гарантирует, что весь буфер будет отправлен (если не произойдёт ошибка).
// Возвращает 0 при успехе, -1 при ошибке.
static int ssl_send_all(SSL *ssl, const void *buf, size_t len) {
    size_t sent = 0; // Сколько байт уже отправлено
    
    // Отправляем по частям, пока не передадим всё
    while (sent < len) {
        int n = SSL_write(ssl, (const char *)buf + sent, len - sent);
        // SSL_write возвращает <= 0 при ошибке или необходимости повтора (но у нас блокирующее соединение)
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }
    
    return 0; // Успешно отправлено
}

// Надёжный приём данных через SSL-соединение.
// Гарантирует чтение ровно `len` байт или возврат ошибки.
// Возвращает общее количество полученных байт (всегда == len при успехе) или -1 при ошибке.
static int ssl_recv_all(SSL *ssl, void *buffer, size_t len) {
    size_t total = 0;      // Сколько байт уже получено
    int bytes;             // Результат одного вызова SSL_read()

    // Читаем циклически, пока не наберём нужное количество байт
    while (total < len) {
        bytes = SSL_read(ssl, (char*)buffer + total, len - total);
        // Любое значение <= 0 означает ошибку (в т.ч. закрытие соединения или SSL-ошибка)
        if (bytes <= 0) return -1;
        total += bytes;
    }

    return (int)total; // Совпадает с `len` при успехе
}


// Шифрование данных с использованием AES-256-GCM.
// Принимает: открытый текст, длину, ключ (32 байта), IV (12 байт).
// Выводит: зашифрованный текст (той же длины, что и вход) и 16-байтный аутентификационный тег.
// Возвращает длину шифротекста (>0) или -1 при ошибке.
static int enhanced_aes_gcm_encrypt(const uint8_t *plaintext, int plaintext_len, const uint8_t *key, const uint8_t *iv, uint8_t *ciphertext, uint8_t *tag) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;

    // Создаём контекст шифрования OpenSSL
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        logger(LOG_ERROR, "Failed to allocate EVP_CIPHER_CTX");
        return -1;
    }

    // Инициализация для AES-256-GCM в два этапа:
    // 1. Указываем алгоритм без ключа/IV (требуется OpenSSL API)
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "EVP_EncryptInit_ex (cipher setup) failed");
        return -1;
    }

    // 2. Устанавливаем ключ и IV
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "EVP_EncryptInit_ex (key/IV setup) failed");
        return -1;
    }

    // Шифруем основную часть данных
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "EVP_EncryptUpdate failed");
        return -1;
    }
    ciphertext_len = len;

    // Завершаем шифрование (в GCM обычно добавляет 0 байт, но вызов обязателен)
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "EVP_EncryptFinal_ex failed");
        return -1;
    }
    ciphertext_len += len;

    // Извлекаем 16-байтный GCM-тег для последующей проверки целостности при расшифровке
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "Failed to retrieve GCM tag");
        return -1;
    }

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len; // Должно совпадать с plaintext_len
}


// Расшифровка данных, зашифрованных AES-256-GCM.
// Принимает: шифротекст, его длину, ключ, IV, 16-байтный тег.
// Выводит: восстановленный открытый текст.
// Возвращает длину расшифрованного текста (>0) или -1 при ошибке (включая несоответствие тега).
static int enhanced_aes_gcm_decrypt(const uint8_t *ciphertext, int ciphertext_len, const uint8_t *key, const uint8_t *iv, const uint8_t *tag, uint8_t *plaintext) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;

    if (!(ctx = EVP_CIPHER_CTX_new())) {
        logger(LOG_ERROR, "Failed to allocate EVP_CIPHER_CTX for decryption");
        return -1;
    }

    // Инициализация для AES-256-GCM (аналогично шифрованию)
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "EVP_DecryptInit_ex (cipher setup) failed");
        return -1;
    }

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "EVP_DecryptInit_ex (key/IV setup) failed");
        return -1;
    }

    // Расшифровываем основную часть
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "EVP_DecryptUpdate failed");
        return -1;
    }
    plaintext_len = len;

    // Устанавливаем ожидаемый GCM-тег ДО вызова Final — критически важно!
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "Failed to set GCM tag for verification");
        return -1;
    }

    // Final проверяет тег: если не совпадает — вернёт ошибку
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        logger(LOG_ERROR, "GCM authentication failed (tag mismatch)");
        return -1;
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len; // Должно совпадать с ciphertext_len
}


// Обработка команды UPLOAD: приём, проверка, шифрование и сохранение файла от клиента.
// Предполагается, что SSL-соединение уже установлено и аутентифицировано.
void handle_upload_request(SSL *ssl, RequestHeader *req, const char *client_fingerprint) {
    // Убедимся, что глобальный криптографический контекст инициализирован
    if (!g_file_crypto.initialized) {
        logger(LOG_ERROR, "Crypto context not initialized — upload aborted");
        ResponseHeader resp = { .status = RESP_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }

    // Защита от path traversal: запрещаем ".." и любые подкаталоги в имени файла
    if (strstr(req->filename, "..") || strchr(req->filename, '/')) {
        logger(LOG_WARNING, "Path traversal attempt blocked for filename: %s", req->filename);
        ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }

    // Если указан получатель — проверяем, что это корректный SHA-256 отпечаток (64 hex-символа)
    if (req->recipient[0] != '\0') {
        if (strlen(req->recipient) != FINGERPRINT_LEN - 1) {
            logger(LOG_WARNING, "Invalid recipient fingerprint length for: %s", req->filename);
            ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
            ssl_send_all(ssl, &resp, sizeof(resp));
            return;
        }
        for (int i = 0; i < 64; i++) {
            char c = req->recipient[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                logger(LOG_WARNING, "Invalid character in recipient fingerprint: %c (pos %d)", c, i);
                ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
                ssl_send_all(ssl, &resp, sizeof(resp));
                return;
            }
        }
        // recipient прошёл валидацию — разрешено
    }

    // Формируем полный путь до файла в безопасной директории хранения
    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", STORAGE_DIR, req->filename);

    // Отправляем клиенту подтверждение, что можно начинать передачу
    ResponseHeader resp = { .status = RESP_SUCCESS };
    if (ssl_send_all(ssl, &resp, sizeof(resp)) != 0) {
        logger(LOG_ERROR, "Failed to send upload permission for: %s", req->filename);
        return;
    }

    // Выделяем буфер под весь файл в памяти (ограничение архитектуры; в реальных системах — потоковая обработка)
    uint8_t *plaintext = malloc(req->filesize);
    if (!plaintext) {
        logger(LOG_ERROR, "Memory allocation failed for file of size %zu: %s", req->filesize, req->filename);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }

    // Приём файла по частям через SSL
    long long remaining = req->filesize;
    uint8_t *ptr = plaintext;

    while (remaining > 0) {
        size_t to_read = (remaining < BUFFER_SIZE) ? (size_t)remaining : BUFFER_SIZE;

        if (ssl_recv_all(ssl, ptr, to_read) != (ssize_t)to_read) {
            logger(LOG_ERROR, "Incomplete file reception for: %s", req->filename);
            free(plaintext);
            return;
        }

        ptr += to_read;
        remaining -= to_read;
    }

    // Проверяем целостность полученного файла через BLAKE3-хеш, присланный клиентом
    uint8_t computed_hash[BLAKE3_HASH_LEN];
    compute_buffer_blake3(plaintext, req->filesize, computed_hash);

    if (memcmp(computed_hash, req->file_hash, BLAKE3_HASH_LEN) != 0) {
        logger(LOG_ERROR, "BLAKE3 integrity check failed for: %s", req->filename);
        ResponseHeader resp = { .status = RESP_INTEGRITY_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        free(plaintext);
        return;
    }

    // Подготавливаем буферы для шифрования
    uint8_t *ciphertext = malloc(req->filesize + 16); // +16 байт — место для тега (но тег отдельно)
    uint8_t iv[12];                                   // Стандартный размер IV для AES-GCM (96 бит)
    uint8_t tag[16];                                  // GCM-тег фиксированной длины

    // Генерируем криптографически безопасный IV
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        logger(LOG_ERROR, "Failed to generate secure IV using RAND_bytes for: %s", req->filename);
        free(plaintext);
        free(ciphertext);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }

    // Выполняем шифрование с использованием глобального ключа
    int ct_len = enhanced_aes_gcm_encrypt(plaintext, req->filesize, g_file_crypto.key, iv, ciphertext, tag);

    // Освобождаем память под открытый текст сразу после шифрования
    free(plaintext);

    if (ct_len < 0) {
        logger(LOG_ERROR, "Encryption pipeline failed for: %s", req->filename);
        free(ciphertext);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }

    // Сохраняем зашифрованный файл на диск
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        logger(LOG_ERROR, "fopen() failed for writing encrypted file: %s (errno=%d)", filepath, errno);
        free(ciphertext);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }

    size_t written = fwrite(ciphertext, 1, ct_len, fp);
    fclose(fp);
    free(ciphertext);

    if (written != (size_t)ct_len) {
        logger(LOG_ERROR, "Short write to disk: expected %d, wrote %zu bytes for %s", ct_len, written, filepath);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }

    // Сохраняем метаданные в MongoDB
    bson_t *doc = bson_new();
    BSON_APPEND_UTF8(doc, "filename", req->filename);
    BSON_APPEND_INT64(doc, "size", req->filesize);
    BSON_APPEND_BOOL(doc, "encrypted", true);
    BSON_APPEND_BINARY(doc, "iv", BSON_SUBTYPE_BINARY, iv, sizeof(iv));
    BSON_APPEND_BINARY(doc, "tag", BSON_SUBTYPE_BINARY, tag, sizeof(tag));
    BSON_APPEND_BOOL(doc, "deleted", false);
    BSON_APPEND_UTF8(doc, "owner_fingerprint", client_fingerprint);

    // Указываем, публичный файл или предназначенный конкретному получателю
    if (req->recipient[0] != '\0') {
        BSON_APPEND_UTF8(doc, "recipient_fingerprint", req->recipient);
        BSON_APPEND_BOOL(doc, "public", false);
    } else {
        BSON_APPEND_BOOL(doc, "public", true);
    }

    // Временная метка загрузки (в миллисекундах с эпохи)
    BSON_APPEND_DATE_TIME(doc, "uploaded_at", bson_get_monotonic_time() / 1000);

    bson_error_t error;
    bool success = mongoc_collection_insert_one(g_collection, doc, NULL, NULL, &error);

    if (!success) {
        logger(LOG_ERROR, "MongoDB metadata insertion failed for %s: [code=%d] %s", req->filename, error.code, error.message);
        resp.status = RESP_ERROR;
    } else {
        logger(LOG_INFO, "File upload completed successfully: %s (size=%zu)", req->filename, req->filesize);
        resp.status = RESP_SUCCESS;

        // Записываем событие обработки в историю (для аудита и отслеживания)
        if (!append_proc_event(filepath, "upload", "success")) {
            logger(LOG_WARNING, "Failed to log upload event in proc map for: %s", filepath);
        }
    }

    // Отправляем финальный статус клиенту
    ssl_send_all(ssl, &resp, sizeof(resp));

    // Освобождаем BSON-документ в любом случае
    bson_destroy(doc);
}


// Обработка команды LIST
void handle_list_request(SSL *ssl, const char *client_fingerprint) {
    bson_t *query = bson_new();
    bson_t *opts = BCON_NEW(
        "projection", "{",
            "filename", BCON_INT32(1),
            "size", BCON_INT32(1),
            "uploaded_at", BCON_INT32(1),
            "public", BCON_INT32(1),
            "owner_fingerprint", BCON_INT32(1),
        "}"
    );
    
    // Показываем публичные файлы и файлы пользователя
    bson_t *or_query = bson_new();
        // Показываем:
    // - файлы, загруженные мной (owner)
    // - файлы, где я — получатель
    // - публичные файлы (если будут)
    bson_t *or_array = bson_new();

    // 1. Я — владелец
    bson_t owner_doc;
    bson_init(&owner_doc);
    BSON_APPEND_UTF8(&owner_doc, "owner_fingerprint", client_fingerprint);
    BSON_APPEND_ARRAY_BEGIN(or_array, "$or", &owner_doc);
    bson_append_document_end(or_array, &owner_doc);

    // 2. Я — получатель
    bson_t recipient_doc;
    bson_init(&recipient_doc);
    BSON_APPEND_UTF8(&recipient_doc, "recipient_fingerprint", client_fingerprint);
    BSON_APPEND_ARRAY_BEGIN(or_array, "$or", &recipient_doc);
    bson_append_document_end(or_array, &recipient_doc);

    // 3. Публичные (опционально)
    bson_t public_doc;
    bson_init(&public_doc);
    BSON_APPEND_BOOL(&public_doc, "public", true);
    BSON_APPEND_ARRAY_BEGIN(or_array, "$or", &public_doc);
    bson_append_document_end(or_array, &public_doc);

    bson_append_document(query, "$or", -1, or_array);
    bson_destroy(or_array);
    
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(g_collection, or_query, opts, NULL);
    
    bson_error_t error;
    const bson_t *doc;
    char *json_str;
    size_t total_len = 0;
    char *full_list = NULL;
    
    while (mongoc_cursor_next(cursor, &doc)) {
        json_str = bson_as_canonical_extended_json(doc, NULL);
        if (!json_str) continue;
        
        size_t len = strlen(json_str);
        
        if (total_len == 0) {
            full_list = malloc(len + 3);
            if (!full_list) {
                bson_free(json_str);
                continue;
            }
            strcpy(full_list, "[");
            total_len = 1;
        } else {
            char *new_list = realloc(full_list, total_len + len + 2);
            if (!new_list) {
                bson_free(json_str);
                continue;
            }
            full_list = new_list;
            full_list[total_len++] = ',';
        }
        
        memcpy(full_list + total_len, json_str, len);
        total_len += len;
        bson_free(json_str);
    }
    
    if (mongoc_cursor_error(cursor, &error)) {
        logger(LOG_ERROR, "Cursor error in list request: %s", error.message);
    }
    
    if (total_len > 0) {
        full_list[total_len++] = ']';
        full_list[total_len] = '\0';
    } else {
        full_list = strdup("[]");
        total_len = strlen(full_list);
    }
    
    ResponseHeader resp = { .status = RESP_SUCCESS, .filesize = total_len };
    ssl_send_all(ssl, &resp, sizeof(resp));
    
    if (full_list) {
        ssl_send_all(ssl, full_list, total_len);
        free(full_list);
    }
    
    mongoc_cursor_destroy(cursor);
    bson_destroy(query);
    bson_destroy(opts);
    bson_destroy(or_query);
    
    logger(LOG_INFO, "Sent file list to client");
}

// Обработка команды DOWNLOAD
void handle_download_request(SSL *ssl, RequestHeader *req, const char *client_fingerprint) {
    if (strstr(req->filename, "..") || strchr(req->filename, '/')) {
        ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }
    
    bson_t *query = bson_new();
    BSON_APPEND_UTF8(query, "filename", req->filename);
    BSON_APPEND_BOOL(query, "deleted", false);
    
    bson_error_t error;
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(g_collection, query, NULL, NULL);
    
    const bson_t *doc;
    bool found = mongoc_cursor_next(cursor, &doc);
    
    if (!found) {
        ResponseHeader resp = { .status = RESP_FILE_NOT_FOUND };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    // Проверка прав доступа
    bson_iter_t iter;
    const char *owner_fp = NULL;
    bool is_public = false;
    
    if (bson_iter_init_find(&iter, doc, "owner_fingerprint") && BSON_ITER_HOLDS_UTF8(&iter)) {
        owner_fp = bson_iter_utf8(&iter, NULL);
    }
    
    if (bson_iter_init_find(&iter, doc, "public") && BSON_ITER_HOLDS_BOOL(&iter)) {
        is_public = bson_iter_bool(&iter);
    }
    
    if (!is_public && (!owner_fp || strcmp(owner_fp, client_fingerprint) != 0)) {
        ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", STORAGE_DIR, req->filename);
    
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ResponseHeader resp = { .status = RESP_FILE_NOT_FOUND };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    long long filesize = st.st_size;
    
    if (req->offset < 0 || req->offset > filesize) {
        ResponseHeader resp = { .status = RESP_INVALID_OFFSET };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        ResponseHeader resp = { .status = RESP_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    // Чтение и расшифровка файла
    uint8_t *ciphertext = malloc(filesize);
    if (!ciphertext) {
        fclose(fp);
        ResponseHeader resp = { .status = RESP_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    if (fread(ciphertext, 1, filesize, fp) != (size_t)filesize) {
        fclose(fp);
        free(ciphertext);
        ResponseHeader resp = { .status = RESP_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    fclose(fp);
    
    // Получение IV и тега из MongoDB
    const uint8_t *iv = NULL;
    const uint8_t *tag = NULL;
    uint32_t iv_len = 0, tag_len = 0;
    
    if (bson_iter_init_find(&iter, doc, "iv") && BSON_ITER_HOLDS_BINARY(&iter)) {
        bson_iter_binary(&iter, NULL, &iv_len, &iv);
    }
    
    if (bson_iter_init_find(&iter, doc, "tag") && BSON_ITER_HOLDS_BINARY(&iter)) {
        bson_iter_binary(&iter, NULL, &tag_len, &tag);
    }
    
    if (!iv || !tag || iv_len != 12 || tag_len != 16) {
        free(ciphertext);
        ResponseHeader resp = { .status = RESP_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    // Расшифровка
    uint8_t *plaintext = malloc(filesize);
    if (!plaintext) {
        free(ciphertext);
        ResponseHeader resp = { .status = RESP_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    int pt_len = enhanced_aes_gcm_decrypt(ciphertext, filesize, g_file_crypto.key, iv, tag, plaintext);
    free(ciphertext);
    
    if (pt_len < 0) {
        free(plaintext);
        ResponseHeader resp = { .status = RESP_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        goto cleanup;
    }
    
    // Отправка файла
    ResponseHeader resp = { .status = RESP_SUCCESS, .filesize = pt_len };
    ssl_send_all(ssl, &resp, sizeof(resp));
    
    long long bytes_to_send = pt_len - req->offset;
    if (bytes_to_send < 0) bytes_to_send = 0;
    
    if (bytes_to_send > 0) {
        ssl_send_all(ssl, plaintext + req->offset, bytes_to_send);
    }
    
    free(plaintext);
    
    // Добавляем событие в proc map
    if (!append_proc_event(filepath, "download", "success")) {
        logger(LOG_WARNING, "Failed to add proc event for download: %s", filepath);
    }
    
    logger(LOG_INFO, "Sent %lld bytes of '%s' to client", bytes_to_send, req->filename);
    
cleanup:
    if (cursor) mongoc_cursor_destroy(cursor);
    if (query) bson_destroy(query);
}

// Обработка клиентского соединения
void *handle_client(void *arg) {
    client_info_t *info = (client_info_t *)arg;
    int client_fd = info->client_socket;
    
    // Создаем SSL объект
    SSL *ssl = SSL_new(g_ssl_ctx);
    if (!ssl) {
        logger(LOG_ERROR, "Failed to create SSL object");
        close(client_fd);
        free(info);
        return NULL;
    }
    
    SSL_set_fd(ssl, client_fd);
    
    // SSL handshake
    if (SSL_accept(ssl) <= 0) {
        logger(LOG_ERROR, "SSL handshake failed");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(client_fd);
        free(info);
        return NULL;
    }
    
    // Получение клиентского сертификата
    X509 *client_cert = SSL_get_peer_certificate(ssl);
    if (!client_cert) {
        logger(LOG_ERROR, "No client certificate provided");
        SSL_free(ssl);
        close(client_fd);
        free(info);
        return NULL;
    }
    
    // Вычисление отпечатка сертификата
    unsigned char cert_hash[SHA256_DIGEST_LENGTH];
    X509_digest(client_cert, EVP_sha256(), cert_hash, NULL);
    
    char client_fingerprint[65];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(&client_fingerprint[i*2], "%02x", cert_hash[i]);
    }
    client_fingerprint[64] = '\0';
    
    X509_free(client_cert);
    
    logger(LOG_INFO, "Client connected: %s:%d (fingerprint: %s)", 
           inet_ntoa(info->client_addr.sin_addr), 
           ntohs(info->client_addr.sin_port),
           client_fingerprint);
    
    // Обработка запросов
    RequestHeader req;
    while (SSL_read(ssl, &req, sizeof(RequestHeader)) == sizeof(RequestHeader)) {
        logger(LOG_DEBUG, "Received command: %d for file: %s", req.command, req.filename);
        
        switch(req.command) {
            case CMD_UPLOAD:
                logger(LOG_INFO, "Upload request for: %s (size: %lld)", req.filename, req.filesize);
                handle_upload_request(ssl, &req, client_fingerprint);
                break;
                
            case CMD_LIST:
                logger(LOG_INFO, "List request");
                handle_list_request(ssl, client_fingerprint);
                break;
                
            case CMD_DOWNLOAD:
                logger(LOG_INFO, "Download request for: %s (offset: %lld)", req.filename, req.offset);
                handle_download_request(ssl, &req, client_fingerprint);
                break;
                
            default:
                logger(LOG_WARNING, "Unknown command: %d", req.command);
                ResponseHeader resp = { .status = RESP_UNKNOWN_COMMAND };
                ssl_send_all(ssl, &resp, sizeof(resp));
                break;
        }
    }
    
    // Завершение соединения
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_fd);
    free(info);
    
    logger(LOG_INFO, "Client disconnected: %s", client_fingerprint);
    return NULL;
}

// Инициализация SSL
static bool init_ssl(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    
    const SSL_METHOD *method = TLS_server_method();
    g_ssl_ctx = SSL_CTX_new(method);
    
    if (!g_ssl_ctx) {
        logger(LOG_ERROR, "Failed to create SSL context");
        return false;
    }
    
    // Загрузка сертификатов — пробуем несколько путей для удобства разработки/запуска
    const char *cert_candidates[] = {"src/server-cert.pem", "../server-cert.pem", "server/server-cert.pem", "src/server/server-cert.pem", NULL};
    const char *key_candidates[] = {"src/server-key.pem", "../server-key.pem", "server/server-key.pem", "src/server/server-key.pem", NULL};
    const char *ca_candidates[]  = {"src/ca.pem", "../ca.pem", "server/ca.pem", "src/server/ca.pem", NULL};

    const char *cert_file = NULL;
    const char *key_file = NULL;
    const char *ca_file = NULL;

    for (const char **p = cert_candidates; *p; ++p) {
        if (access(*p, R_OK) == 0) { cert_file = *p; break; }
    }
    for (const char **p = key_candidates; *p; ++p) {
        if (access(*p, R_OK) == 0) { key_file = *p; break; }
    }
    for (const char **p = ca_candidates; *p; ++p) {
        if (access(*p, R_OK) == 0) { ca_file = *p; break; }
    }

    if (!cert_file) {
        logger(LOG_ERROR, "Failed to find server certificate (tried multiple locations)");
        return false;
    }
    if (!key_file) {
        logger(LOG_ERROR, "Failed to find server private key (tried multiple locations)");
        return false;
    }
    if (!ca_file) {
        logger(LOG_WARNING, "CA certificate not found; continuing without explicit CA file (peer verification may fail)");
    }

    if (SSL_CTX_use_certificate_file(g_ssl_ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        unsigned long e = ERR_get_error();
        const char *err = e ? ERR_error_string(e, NULL) : "unknown";
        logger(LOG_ERROR, "Failed to load server certificate from %s: %s", cert_file, err);
        return false;
    } else {
        logger(LOG_INFO, "Loaded server certificate from %s", cert_file);
    }

    if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        unsigned long e = ERR_get_error();
        const char *err = e ? ERR_error_string(e, NULL) : "unknown";
        logger(LOG_ERROR, "Failed to load server private key from %s: %s", key_file, err);
        return false;
    } else {
        logger(LOG_INFO, "Loaded server private key from %s", key_file);
    }

    if (!SSL_CTX_check_private_key(g_ssl_ctx)) {
        logger(LOG_ERROR, "Server certificate and private key do not match (%s / %s)", cert_file, key_file);
        return false;
    }

    if (ca_file) {
        if (SSL_CTX_load_verify_locations(g_ssl_ctx, ca_file, NULL) <= 0) {
            unsigned long e = ERR_get_error();
            const char *err = e ? ERR_error_string(e, NULL) : "unknown";
            logger(LOG_ERROR, "Failed to load CA certificate from %s: %s", ca_file, err);
            return false;
        } else {
            logger(LOG_INFO, "Loaded CA certificate from %s", ca_file);
        }
    }
    
    SSL_CTX_set_verify(g_ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    SSL_CTX_set_verify_depth(g_ssl_ctx, 1);
    
    logger(LOG_INFO, "SSL initialization completed successfully");
    return true;
}

// Инициализация MongoDB
static bool init_mongodb(void) {
    mongoc_init();
    
    g_mongo_client = mongoc_client_new(MONGODB_URI);
    if (!g_mongo_client) {
        logger(LOG_ERROR, "Failed to connect to MongoDB");
        return false;
    }
    
    // Проверка подключения
    bson_error_t error;
    bool ping_success = mongoc_client_command_simple(
        g_mongo_client, "admin", BCON_NEW("ping", BCON_INT32(1)), NULL, NULL, &error);
    
    if (!ping_success) {
        logger(LOG_ERROR, "MongoDB ping failed: %s", error.message);
        mongoc_client_destroy(g_mongo_client);
        g_mongo_client = NULL;
        return false;
    }
    
    g_collection = mongoc_client_get_collection(g_mongo_client, DATABASE_NAME, COLLECTION_NAME);
    if (!g_collection) {
        logger(LOG_ERROR, "Failed to get collection");
        mongoc_client_destroy(g_mongo_client);
        g_mongo_client = NULL;
        return false;
    }
    
    logger(LOG_INFO, "MongoDB initialization completed successfully");
    return true;
}

// Инициализация криптографии
static bool init_cryptography(void) {
    if (!RAND_bytes(g_file_crypto.key, sizeof(g_file_crypto.key))) {
        logger(LOG_ERROR, "Failed to generate encryption key");
        return false;
    }
    
    g_file_crypto.initialized = 1;
    logger(LOG_INFO, "Cryptography initialization completed successfully");
    return true;
}

// Инициализация логирования
static bool init_logging(void) {
    g_log_file = fopen(LOG_FILE, "a");
    if (!g_log_file) {
        g_log_file = stderr;
        fprintf(stderr, "Failed to open log file, using stderr\n");
    }
    
    logger(LOG_INFO, "File server starting up");
    return true;
}

// Создание директории для файлов
static bool create_storage_dir(void) {
    if (mkdir(STORAGE_DIR, 0755) != 0 && errno != EEXIST) {
        logger(LOG_ERROR, "Failed to create storage directory: %s", strerror(errno));
        return false;
    }
    
    logger(LOG_INFO, "Storage directory ready: %s", STORAGE_DIR);
    return true;
}

// Очистка ресурсов
static void cleanup_resources(void) {
    logger(LOG_INFO, "Cleaning up resources");
    
    if (g_ssl_ctx) {
        SSL_CTX_free(g_ssl_ctx);
        g_ssl_ctx = NULL;
    }
    
    if (g_collection) {
        mongoc_collection_destroy(g_collection);
        g_collection = NULL;
    }
    
    if (g_mongo_client) {
        mongoc_client_destroy(g_mongo_client);
        g_mongo_client = NULL;
    }
    
    mongoc_cleanup();
    
    if (g_file_crypto.initialized) {
        explicit_bzero(g_file_crypto.key, sizeof(g_file_crypto.key));
        g_file_crypto.initialized = 0;
    }
    
    if (g_log_file && g_log_file != stderr) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    EVP_cleanup();
    ERR_free_strings();
}

// Обработчик сигналов
static void signal_handler(int sig) {
    logger(LOG_INFO, "Received signal %d, shutting down", sig);
    g_shutdown = 1;
}

// Настройка обработчиков сигналов
static bool setup_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        logger(LOG_ERROR, "Failed to setup signal handlers: %s", strerror(errno));
        return false;
    }
    
    signal(SIGPIPE, SIG_IGN);
    return true;
}

int main() {
    // Инициализация компонентов
    if (!init_logging()) {
        return EXIT_FAILURE;
    }
    // Print animated startup logo to terminal
    print_startup_logo();
    // Показать прогресс загрузки модулей (визуальный эффект)
    const char *modules[] = {"OpenSSL", "MongoDB", "Crypto", "Storage"};
    print_module_loading(modules, sizeof(modules)/sizeof(modules[0]));
    
    if (!setup_signal_handlers()) {
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    if (!init_ssl()) {
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    if (!init_mongodb()) {
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    if (!init_cryptography()) {
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    if (!create_storage_dir()) {
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    // Создание сокета
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        logger(LOG_ERROR, "Failed to create socket: %s", strerror(errno));
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    // Настройка сокета
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger(LOG_ERROR, "Failed to set socket options: %s", strerror(errno));
        close(server_fd);
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Привязка и прослушивание
    if (bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        logger(LOG_ERROR, "Bind failed: %s", strerror(errno));
        close(server_fd);
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    if (listen(server_fd, MAX_USERS_LISTEN) < 0) {
        logger(LOG_ERROR, "Listen failed: %s", strerror(errno));
        close(server_fd);
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    logger(LOG_INFO, "Server listening on port %d", PORT);
    
    // Основной цикл
    while (!g_shutdown) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno != EINTR) {
                logger(LOG_ERROR, "Accept failed: %s", strerror(errno));
            }
            continue;
        }
        
        client_info_t *info = malloc(sizeof(client_info_t));
        if (!info) {
            logger(LOG_ERROR, "Memory allocation failed for client info");
            close(client_fd);
            continue;
        }
        
        info->client_socket = client_fd;
        info->client_addr = client_addr;
        info->ssl = NULL;
        memset(info->fingerprint, 0, sizeof(info->fingerprint));
        
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, info) != 0) {
            logger(LOG_ERROR, "Failed to create client thread");
            free(info);
            close(client_fd);
            continue;
        }
        
        pthread_detach(tid);
    }
    
    // Завершение работы
    logger(LOG_INFO, "Server shutting down");
    close(server_fd);
    cleanup_resources();
    
    return EXIT_SUCCESS;
}
