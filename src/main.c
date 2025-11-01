#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <bson/bson.h>
#include <mongoc/mongoc.h>

// Конфигурация
#define PID_FILE "/tmp/exchange-daemon.pid"
#define EXCHANGE_DIR "/home/just/mesh_proto/oxxyen_storage/file_dir/filetrade"
#define LOG_FILE "/tmp/exchange-daemon.log"
#define MONGODB_URI "mongodb://localhost:27017"
#define DATABASE_NAME "file_exchange"
#define COLLECTION_NAME "file_groups"

#define EVENT_BUFFER_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
#define MAX_KEY_LENGTH 32

// Глобальные переменные
static volatile sig_atomic_t g_shutdown = 0;
static mongoc_client_t *g_mongo_client = NULL;
static FILE *g_log_file = NULL;

// Уровни логирования
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

// Логирование
static void logger(log_level_t level, const char *format, ...) {
    if (!g_log_file) return;
    
    const char *level_str[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(g_log_file, "[%s] [%s] ", timestamp, level_str[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(g_log_file, format, args);
    va_end(args);
    
    fprintf(g_log_file, "\n");
    fflush(g_log_file);
}

// Получение расширения файла
static char* get_file_extension(const char *full_path) {
    if (!full_path) return NULL;
    
    const char *filename = strrchr(full_path, '/');
    if (!filename) filename = full_path;
    else filename++;
    
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return strdup("");
    }
    
    return strdup(dot);
}

// Получение имени файла без расширения
static char* get_filename_without_extension(const char* full_filename) {
    const char *filename = strrchr(full_filename, '/');
    if (!filename) filename = full_filename;
    else filename++;
    
    const char *dot = strrchr(filename, '.');
    size_t name_len = dot ? (size_t)(dot - filename) : strlen(filename);
    
    char *result = malloc(name_len + 1);
    if (!result) {
        logger(LOG_ERROR, "Memory allocation failed in get_filename_without_extension");
        return NULL;
    }
    
    strncpy(result, filename, name_len);
    result[name_len] = '\0';
    return result;
}

// Получение следующего ключа для proc map
static char* get_next_proc_key(const char *file_id) {
    mongoc_collection_t *coll = mongoc_client_get_collection(
        g_mongo_client, DATABASE_NAME, COLLECTION_NAME);
    if (!coll) {
        logger(LOG_ERROR, "Failed to get collection for file: %s", file_id);
        return NULL;
    }
    
    bson_t *query = BCON_NEW("_id", BCON_UTF8(file_id));
    bson_t *projection = BCON_NEW("proc", BCON_INT32(1));
    bson_error_t error;
    
    char *next_key = NULL;
    const bson_t *doc;
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        coll, query, NULL, NULL);
    
    int64_t max_key = 0;
    
    if (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, doc, "proc") && 
            BSON_ITER_HOLDS_DOCUMENT(&iter)) {
            
            bson_iter_t child;
            bson_iter_recurse(&iter, &child);
            
            while (bson_iter_next(&child)) {
                const char *key_str = bson_iter_key(&child);
                char *endptr;
                long num = strtol(key_str, &endptr, 10);
                
                if (errno == 0 && *endptr == '\0' && num > 0 && num > max_key) {
                    max_key = num;
                }
            }
        }
    } else {
        logger(LOG_DEBUG, "No existing document found for: %s, starting from key 1", file_id);
    }
    
    if (mongoc_cursor_error(cursor, &error)) {
        logger(LOG_ERROR, "Cursor error for file %s: %s", file_id, error.message);
    }
    
    mongoc_cursor_destroy(cursor);
    bson_destroy(query);
    bson_destroy(projection);
    mongoc_collection_destroy(coll);
    
    next_key = malloc(MAX_KEY_LENGTH);
    if (!next_key) {
        logger(LOG_ERROR, "Memory allocation failed for next_key");
        return NULL;
    }
    
    snprintf(next_key, MAX_KEY_LENGTH, "%" PRId64, max_key + 1);
    logger(LOG_DEBUG, "Next proc key for %s: %s", file_id, next_key);
    return next_key;
}

// Создание базового документа файла
static bool create_base_document(const char *fullpath) {
    mongoc_collection_t *coll = mongoc_client_get_collection(
        g_mongo_client, DATABASE_NAME, COLLECTION_NAME);
    if (!coll) {
        logger(LOG_ERROR, "Failed to get collection for base document: %s", fullpath);
        return false;
    }
    
    char *filename = get_filename_without_extension(fullpath);
    char *extension = get_file_extension(fullpath);
    
    if (!filename || !extension) {
        logger(LOG_ERROR, "Failed to parse filename/extension for: %s", fullpath);
        free(filename);
        free(extension);
        mongoc_collection_destroy(coll);
        return false;
    }
    
    bson_t *doc = bson_new();
    BSON_APPEND_UTF8(doc, "_id", fullpath);
    BSON_APPEND_UTF8(doc, "filename", filename);
    BSON_APPEND_UTF8(doc, "extension", extension);
    
    // Инициализируем пустой proc map
    bson_t proc;
    bson_init(&proc);
    BSON_APPEND_DOCUMENT(doc, "proc", &proc);
    bson_destroy(&proc);
    
    bson_error_t error;
    bool success = mongoc_collection_insert_one(coll, doc, NULL, NULL, &error);
    
    if (!success) {
        // Если документ уже существует, это не ошибка
        if (error.code != 11000) { // Duplicate key error
            logger(LOG_ERROR, "Failed to create base document for %s: %s", fullpath, error.message);
        } else {
            logger(LOG_DEBUG, "Base document already exists for: %s", fullpath);
            success = true; // Документ уже существует, продолжаем
        }
    } else {
        logger(LOG_INFO, "Created base document for: %s", fullpath);
    }
    
    bson_destroy(doc);
    free(filename);
    free(extension);
    mongoc_collection_destroy(coll);
    
    return success;
}

// Добавление события в proc map
static bool append_proc_event(const char *file_id, const char *change_type, const char *status) {
    // Сначала убедимся, что базовый документ существует
    if (!create_base_document(file_id)) {
        logger(LOG_ERROR, "Failed to ensure base document for: %s", file_id);
        return false;
    }
    
    mongoc_collection_t *coll = mongoc_client_get_collection(
        g_mongo_client, DATABASE_NAME, COLLECTION_NAME);
    if (!coll) {
        logger(LOG_ERROR, "Failed to get collection for event: %s", file_id);
        return false;
    }
    
    char *next_key = get_next_proc_key(file_id);
    if (!next_key) {
        logger(LOG_ERROR, "Failed to get next proc key for: %s", file_id);
        mongoc_collection_destroy(coll);
        return false;
    }
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t now_ms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    
    // Формируем путь для обновления: "proc.<next_key>"
    char set_path[64];
    snprintf(set_path, sizeof(set_path), "proc.%s", next_key);
    
    // Создаем документ события
    bson_t event_doc;
    bson_init(&event_doc);
    BSON_APPEND_DATE_TIME(&event_doc, "date", now_ms);
    
    bson_t info_doc;
    bson_init(&info_doc);
    BSON_APPEND_UTF8(&info_doc, "type_of_changes", change_type);
    BSON_APPEND_UTF8(&info_doc, "status", status);
    BSON_APPEND_DOCUMENT(&event_doc, "info", &info_doc);
    
    // Формируем операцию обновления
    bson_t *update = BCON_NEW("$set", "{", set_path, BCON_DOCUMENT(&event_doc), "}");
    bson_t *query = BCON_NEW("_id", BCON_UTF8(file_id));
    
    bson_error_t error;
    bool success = mongoc_collection_update_one(
        coll, query, update, NULL, NULL, &error);
    
    if (!success) {
        logger(LOG_ERROR, "Failed to append proc event for %s: %s", file_id, error.message);
    } else {
        logger(LOG_INFO, "Added event %s to %s: %s - %s", 
               next_key, file_id, change_type, status);
    }
    
    // Очистка
    bson_destroy(&info_doc);
    bson_destroy(&event_doc);
    bson_destroy(update);
    bson_destroy(query);
    free(next_key);
    mongoc_collection_destroy(coll);
    
    return success;
}

// Проверка, является ли путь обычным файлом
static bool is_regular_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        logger(LOG_WARNING, "Failed to stat file: %s, error: %s", path, strerror(errno));
        return false;
    }
    return S_ISREG(st.st_mode);
}

// Обработчик создания/модификации файла
static void handle_file_event(const char *fullpath, const char *event_type) {
    if (!is_regular_file(fullpath)) {
        logger(LOG_DEBUG, "Skipping non-regular file: %s", fullpath);
        return;
    }
    
    logger(LOG_INFO, "File %s: %s", event_type, fullpath);
    
    if (!append_proc_event(fullpath, event_type, "success")) {
        logger(LOG_ERROR, "Failed to log %s event for: %s", event_type, fullpath);
    }
}

// Обработчик удаления файла
static void handle_file_deleted(const char *fullpath) {
    logger(LOG_INFO, "File deleted: %s", fullpath);
    
    if (!append_proc_event(fullpath, "deleted", "n/a")) {
        logger(LOG_ERROR, "Failed to log deletion event for: %s", fullpath);
    }
}

// Обработчик сигналов
static void signal_handler(int sig) {
    logger(LOG_INFO, "Received signal %d, shutting down", sig);
    g_shutdown = 1;
}

// Проверка на уже запущенный демон
static bool is_daemon_running(void) {
    FILE *pidfp = fopen(PID_FILE, "r");
    if (!pidfp) {
        return false;
    }
    
    pid_t old_pid;
    if (fscanf(pidfp, "%d", &old_pid) == 1 && kill(old_pid, 0) == 0) {
        fclose(pidfp);
        logger(LOG_ERROR, "Daemon already running with PID %d", (int)old_pid);
        return true;
    }
    
    fclose(pidfp);
    return false;
}

// Запись PID файла
static bool write_pid_file(void) {
    FILE *pidfp = fopen(PID_FILE, "w");
    if (!pidfp) {
        logger(LOG_ERROR, "Failed to create PID file: %s", strerror(errno));
        return false;
    }
    
    fprintf(pidfp, "%d\n", (int)getpid());
    fclose(pidfp);
    return true;
}

// Инициализация MongoDB
static bool init_mongodb(void) {
    mongoc_init();
    
    g_mongo_client = mongoc_client_new(MONGODB_URI);
    if (!g_mongo_client) {
        logger(LOG_ERROR, "Failed to connect to MongoDB at %s", MONGODB_URI);
        return false;
    }
    
    // Проверяем подключение
    bson_error_t error;
    bool ping_success = mongoc_client_command_simple(
        g_mongo_client, "admin", BCON_NEW("ping", BCON_INT32(1)), NULL, NULL, &error);
    
    if (!ping_success) {
        logger(LOG_ERROR, "MongoDB ping failed: %s", error.message);
        mongoc_client_destroy(g_mongo_client);
        g_mongo_client = NULL;
        return false;
    }
    
    logger(LOG_INFO, "Successfully connected to MongoDB");
    return true;
}

// Очистка ресурсов
static void cleanup_resources(void) {
    logger(LOG_INFO, "Cleaning up resources");
    
    if (g_mongo_client) {
        mongoc_client_destroy(g_mongo_client);
        g_mongo_client = NULL;
    }
    
    mongoc_cleanup();
    
    if (g_log_file && g_log_file != stdout && g_log_file != stderr) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    unlink(PID_FILE);
}

// Инициализация логирования
static bool init_logging(void) {
    g_log_file = fopen(LOG_FILE, "a");
    if (!g_log_file) {
        // Fallback to stderr
        g_log_file = stderr;
        fprintf(stderr, "Failed to open log file, using stderr\n");
    }
    
    logger(LOG_INFO, "Daemon started with PID %d", (int)getpid());
    return true;
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
    
    // Игнорируем SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    
    return true;
}

// Основная функция
int main(void) {
    // Проверяем, не запущен ли уже демон
    if (is_daemon_running()) {
        fprintf(stderr, "Daemon is already running\n");
        return EXIT_FAILURE;
    }
    
    // Демонизация
    if (daemon(1, 1) == -1) {
        perror("daemon");
        return EXIT_FAILURE;
    }
    
    // Инициализация логирования
    if (!init_logging()) {
        return EXIT_FAILURE;
    }
    
    // Запись PID файла
    if (!write_pid_file()) {
        return EXIT_FAILURE;
    }
    
    // Инициализация MongoDB
    if (!init_mongodb()) {
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    // Настройка обработчиков сигналов
    if (!setup_signal_handlers()) {
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    // Инициализация inotify
    int inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (inotify_fd == -1) {
        logger(LOG_ERROR, "inotify_init1 failed: %s", strerror(errno));
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    // Добавление наблюдения за директорией
    int wd = inotify_add_watch(inotify_fd, EXCHANGE_DIR,
                              IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM);
    if (wd == -1) {
        logger(LOG_ERROR, "inotify_add_watch failed for %s: %s", 
               EXCHANGE_DIR, strerror(errno));
        close(inotify_fd);
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    logger(LOG_INFO, "Started watching directory: %s", EXCHANGE_DIR);
    
    // Основной цикл
    char buffer[EVENT_BUFFER_SIZE * 128] __attribute__((aligned(__alignof__(struct inotify_event))));
    
    while (!g_shutdown) {
        ssize_t len = read(inotify_fd, buffer, sizeof(buffer));
        
        if (len == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN) {
                usleep(100000); // 100ms
                continue;
            } else {
                logger(LOG_ERROR, "inotify read error: %s", strerror(errno));
                break;
            }
        }
        
        // Обработка событий inotify
        for (char *ptr = buffer; ptr < buffer + len;) {
            struct inotify_event *event = (struct inotify_event *)ptr;
            
            if (event->len > 0) {
                char fullpath[PATH_MAX];
                int res = snprintf(fullpath, sizeof(fullpath), "%s/%s", 
                                  EXCHANGE_DIR, event->name);
                
                if (res < 0 || (size_t)res >= sizeof(fullpath)) {
                    logger(LOG_ERROR, "Path too long: %s/%s", EXCHANGE_DIR, event->name);
                    ptr += sizeof(struct inotify_event) + event->len;
                    continue;
                }
                
                if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
                    handle_file_event(fullpath, 
                                     (event->mask & IN_MOVED_TO) ? "moved_to" : "modified");
                } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                    handle_file_deleted(fullpath);
                }
            }
            
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    
    // Завершение работы
    logger(LOG_INFO, "Shutting down daemon");
    
    if (inotify_fd >= 0) {
        if (wd >= 0) {
            inotify_rm_watch(inotify_fd, wd);
        }
        close(inotify_fd);
    }
    
    cleanup_resources();
    return EXIT_SUCCESS;
}