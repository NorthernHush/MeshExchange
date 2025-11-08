
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

#include <errno.h>
#include <signal.h>
#include <linux/limits.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h> 
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <openssl/ssl.h>

#define BLAKE3_IMPLEMENTATION
#include "blake3.h"

// Подмодули
#include "../db/mongo_ops_server.h"
#include "../../include/protocol.h"
#include "../crypto/aes_gcm.h"

// Конфигурация
#define PORT 4141
#define BUFFER_SIZE 4096
#define MAX_KEY_LENGTH 32
#define LOG_FILE "/tmp/file-server.log"
#define MONGODB_URI "mongodb://localhost:27017"
#define DATABASE_NAME "file_exchange"
#define COLLECTION_NAME "file_groups"
#define STORAGE_DIR "../../filetrade"

// Hello world 
// Уровни логирования
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

// Глобальные переменные
static volatile sig_atomic_t g_shutdown = 0;
mongoc_client_t *g_mongo_client = NULL;
mongoc_collection_t *g_collection = NULL;
static SSL_CTX *g_ssl_ctx = NULL;
static FILE *g_log_file = NULL;

// Контекст шифрования
typedef struct {
    uint8_t key[32];
    int initialized;
} file_crypto_ctx_t;

static file_crypto_ctx_t g_file_crypto = {0};

// Информация о клиенте
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
    SSL *ssl;
    char fingerprint[65];
} client_info_t;

// Логирование (в файл и в терминал)
static void logger(log_level_t level, const char *format, ...) {
    const char *level_str[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
    const char *color[] = {"\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m"};
    const char *color_reset = "\x1b[0m";

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    // Compose the message into a buffer
    char msgbuf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(msgbuf, sizeof(msgbuf), format, args);
    va_end(args);

    // Log to file if available
    if (g_log_file) {
        fprintf(g_log_file, "[%s] [%s] %s\n", timestamp, level_str[level], msgbuf);
        fflush(g_log_file);
    }

    // Also print to stderr (terminal) with colorized level
    fprintf(stderr, "%s[%s] [%s] %s%s\n", color[level], timestamp, level_str[level], msgbuf, color_reset);
}

// Animated ASCII logo for startup
static void print_startup_logo(void) {
    const char *frames[] = {
        "\n  __  __          _    _____  _                _\n |  \/  |   /\   | |  |  __ \| |              | |\n | \  / |  /  \  | |  | |__) | |__   ___  __ _| |_\n | |\/| | / /\ \ | |  |  ___/| '_ \ / _ \/ _` | __|\n | |  | |/ ____ \| |  | |    | | | |  __/ (_| | |_\n |_|  |_/_/    \_\_|  |_|    |_| |_|\___|\__,_|\__|\n\n",
        "\n  __  __   __  __  _____  _                _\n |  \/  | |  \/  |/ ____|| |              | |\n | \  / | | \  / | |     | |__   ___  __ _| |_\n | |\/| | | |\/| | |     | '_ \ / _ \/ _` | __|\n | |  | | | |  | | |____ | | | |  __/ (_| | |_\n |_|  |_| |_|  |_|\_____||_| |_|\___|\__,_|\__|\n\n",
        "\n  __  __  _   _ _______  _                _\n |  \/  |(_) | |__   __|| |              | |\n | \  / | _  | |  | |   | |__   ___  __ _| |_\n | |\/| || | | |  | |   | '_ \ / _ \/ _` | __|\n | |  | || | | |  | |   | | | |  __/ (_| | |_\n |_|  |_|/ |_|_|  |_|   |_| |_|\___|\__,_|\__|\n\n",
        NULL
    };

    for (int i = 0; frames[i]; ++i) {
        fprintf(stderr, "%s", frames[i]);
        fflush(stderr);
        usleep(120000); // 120 ms
    }
    // Title line
    fprintf(stderr, "\x1b[1mMeshExchange\x1b[0m - starting up...\n\n");
    fflush(stderr);
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

// Вычисление хеша BLAKE3
static void compute_buffer_blake3(const uint8_t *data, size_t len, uint8_t out_hash[BLAKE3_HASH_LEN]) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data, len);
    blake3_hasher_finalize(&hasher, out_hash, BLAKE3_HASH_LEN);
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
        if (error.code != 11000) {
            logger(LOG_ERROR, "Failed to create base document for %s: %s", fullpath, error.message);
        } else {
            logger(LOG_DEBUG, "Base document already exists for: %s", fullpath);
            success = true;
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

// Отправка всех данных через SSL
static int ssl_send_all(SSL *ssl, const void *buf, size_t len) {
    size_t sent = 0;
    
    while (sent < len) {
        int n = SSL_write(ssl, (const char *)buf + sent, len - sent);
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }
    
    return 0;
}

// Получение всех данных через SSL
static int ssl_recv_all(SSL *ssl, void *buffer, size_t len) {
    size_t total = 0;
    int bytes;
    
    while (total < len) {
        bytes = SSL_read(ssl, (char*)buffer + total, len - total);
        if (bytes <= 0) return -1;
        total += bytes;
    }
    
    return total;
}

// шифрование AES-256-GCM
static int enhanced_aes_gcm_encrypt(const uint8_t *plaintext, int plaintext_len,
                                   const uint8_t *key, const uint8_t *iv,
                                   uint8_t *ciphertext, uint8_t *tag) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;
    
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        return -1;
    }
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len += len;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

// дешифрование AES-256-GCM
static int enhanced_aes_gcm_decrypt(const uint8_t *ciphertext, int ciphertext_len,
                                   const uint8_t *key, const uint8_t *iv,
                                   const uint8_t *tag, uint8_t *plaintext) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        return -1;
    }
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len = len;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

// Обработка команды UPLOAD
void handle_upload_request(SSL *ssl, RequestHeader *req, const char *client_fingerprint) {
    if (!g_file_crypto.initialized) {
        logger(LOG_ERROR, "Crypto context not initialized");
        ResponseHeader resp = { .status = RESP_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }
    
    // Проверка пути
    if (strstr(req->filename, "..") || strchr(req->filename, '/')) {
        ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }

    if(req->recipient[0] != '\0') {
        if(strlen(req->recipient) != FINGERPRINT_LEN - 1) {
            ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
            ssl_send_all(ssl, &resp, sizeof(resp));
            return;
        }
        for(int i = 0; i < 64; i++) {
            if(!((req->recipient[i] >= '0' && req->recipient[i] <= '9') ||
                 (req->recipient[i] >= 'a' && req->recipient[i] <= 'f'))) {
                ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
                ssl_send_all(ssl, &resp, sizeof(resp));
                return;
            }
        }
    }
    
    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", STORAGE_DIR, req->filename);
    
    ResponseHeader resp = { .status = RESP_SUCCESS };
    if (ssl_send_all(ssl, &resp, sizeof(resp)) != 0) {
        logger(LOG_ERROR, "Failed to send success response for upload");
        return;
    }
    
    // Получение файла
    uint8_t *plaintext = malloc(req->filesize);
    if (!plaintext) {
        logger(LOG_ERROR, "Memory allocation failed for plaintext");
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }
    
    long long remaining = req->filesize;
    uint8_t *ptr = plaintext;
    
    while (remaining > 0) {
        size_t to_read = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
        
        if (ssl_recv_all(ssl, ptr, to_read) != (ssize_t)to_read) {
            logger(LOG_ERROR, "Failed to receive file data for: %s", req->filename);
            free(plaintext);
            return;
        }
        
        ptr += to_read;
        remaining -= to_read;
    }
    
    // Проверка целостности BLAKE3
    uint8_t computed_hash[BLAKE3_HASH_LEN];
    compute_buffer_blake3(plaintext, req->filesize, computed_hash);
    
    if (memcmp(computed_hash, req->file_hash, BLAKE3_HASH_LEN) != 0) {
        logger(LOG_ERROR, "Integrity check failed for: %s", req->filename);
        ResponseHeader resp = { .status = RESP_INTEGRITY_ERROR };
        ssl_send_all(ssl, &resp, sizeof(resp));
        free(plaintext);
        return;
    }
    
    // Шифрование AES-256-GCM
    uint8_t *ciphertext = malloc(req->filesize + 16);
    uint8_t iv[12];
    uint8_t tag[16];
    
    // Генерация случайного IV
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        logger(LOG_ERROR, "Failed to generate IV for: %s", req->filename);
        free(plaintext);
        free(ciphertext);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }
    
    int ct_len = enhanced_aes_gcm_encrypt(plaintext, req->filesize, 
                                         g_file_crypto.key, iv, 
                                         ciphertext, tag);
    
    free(plaintext);
    
    if (ct_len < 0) {
        logger(LOG_ERROR, "Encryption failed for: %s", req->filename);
        free(ciphertext);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }
    
    // Сохранение зашифрованного файла
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        logger(LOG_ERROR, "Failed to open file for writing: %s", filepath);
        free(ciphertext);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }
    
    size_t written = fwrite(ciphertext, 1, ct_len, fp);
    fclose(fp);
    free(ciphertext);
    
    if (written != (size_t)ct_len) {
        logger(LOG_ERROR, "Failed to write complete file: %s", filepath);
        resp.status = RESP_ERROR;
        ssl_send_all(ssl, &resp, sizeof(resp));
        return;
    }


    
    // Сохранение метаданных в MongoDB
    bson_t *doc = bson_new();
    BSON_APPEND_UTF8(doc, "filename", req->filename);
    BSON_APPEND_INT64(doc, "size", req->filesize);
    BSON_APPEND_BOOL(doc, "encrypted", true);
    BSON_APPEND_BINARY(doc, "iv", BSON_SUBTYPE_BINARY, iv, sizeof(iv));
    BSON_APPEND_BINARY(doc, "tag", BSON_SUBTYPE_BINARY, tag, sizeof(tag));
    BSON_APPEND_BOOL(doc, "deleted", false);
    BSON_APPEND_UTF8(doc, "owner_fingerprint", client_fingerprint);
    

    if(req->recipient[0] != '\0') {
        BSON_APPEND_UTF8(doc, "recipient_fingerprint", req->recipient);
        BSON_APPEND_BOOL(doc, "public", false);
    } else {
        BSON_APPEND_BOOL(doc, "public", true);
    }
    
    BSON_APPEND_DATE_TIME(doc, "uploaded_at", bson_get_monotonic_time() / 1000);
    
    bson_error_t error;
    bool success = mongoc_collection_insert_one(g_collection, doc, NULL, NULL, &error);

    if (!success) {
        logger(LOG_ERROR, "MongoDB insert failed for %s: %s", req->filename, error.message);
        resp.status = RESP_ERROR;
    } else {
        logger(LOG_INFO, "File uploaded successfully: %s", req->filename);
        resp.status = RESP_SUCCESS;
        
        // Добавляем событие в proc map
        if (!append_proc_event(filepath, "upload", "success")) {
            logger(LOG_WARNING, "Failed to add proc event for: %s", filepath);
        }
    }
    
    ssl_send_all(ssl, &resp, sizeof(resp));
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
    
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        g_collection, or_query, opts, NULL);
    
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
    
    int pt_len = enhanced_aes_gcm_decrypt(ciphertext, filesize, 
                                         g_file_crypto.key, iv, 
                                         tag, plaintext);
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
    const char *cert_candidates[] = {"./server-cert.pem", "../server-cert.pem", "server/server-cert.pem", "src/server/server-cert.pem", NULL};
    const char *key_candidates[] = {"./server-key.pem", "../server-key.pem", "server/server-key.pem", "src/server/server-key.pem", NULL};
    const char *ca_candidates[]  = {"./ca.pem", "../ca.pem", "server/ca.pem", "src/server/ca.pem", NULL};

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
    
    if (listen(server_fd, 10) < 0) {
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
