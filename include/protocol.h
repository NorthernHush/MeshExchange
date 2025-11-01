#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define FILENAME_MAX_LEN 256
#define BUFFER_SIZE      4096
#define BLAKE3_HASH_LEN  32 // 32 байта


// Типы команд, согласованные с сервером
typedef enum {
    CMD_UPLOAD,
    CMD_DOWNLOAD,
    CMD_LIST,
    CMD_UNKNOWN
} CommandType;

// статусы
#define RESP_INTEGRITY_ERROR 3
#define FINGERPRINT_LEN 65

// Статусы ответа от сервера
typedef enum {
    RESP_SUCCESS,
    RESP_FAILURE,
    RESP_FILE_NOT_FOUND,
    RESP_PERMISSION_DENIED,
    RESP_ERROR,
    RESP_INVALID_OFFSET,
    RESP_UNKNOWN_COMMAND = 4
} ResponseStatus;

// Заголовок запроса от клиента к серверу
typedef struct {
    CommandType command;
    char filename[FILENAME_MAX_LEN];
    long long filesize; // Используем long long для размера файла

    int64_t offset;

    uint8_t flags; // bit 0 = public

    uint8_t file_hash[BLAKE3_HASH_LEN]; // для download/list
    char recipient[FINGERPRINT_LEN]; // для upload
} RequestHeader;

// Заголовок ответа от сервера к клиенту
typedef struct {
    ResponseStatus status;
    long long filesize; // Для передачи размера файла при скачивании
} ResponseHeader;


// Объявления функциц

int send_all(int sockfd, const void *buffer, size_t len);
int recv_all(int sockfd, void *buffer, size_t len);

#endif