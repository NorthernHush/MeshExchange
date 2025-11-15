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
    CMD_UNKNOWN,
    CMD_CONNECT = 99,  // клиент хочет подключиться и ждать
    CMD_CHECK = 100,   // Команда для администратора: проверить отпечаток
    CMD_APPROVE = 101  // Команда для администратора: подтвердить подключение
} CommandType;

// Опции для выбора на стороне сервера
typedef enum {
    OPEN_SERVER,
    OFF_USERS,
    CHECK_CLIENTS,
} OptionUserServer;

// статусы
#define FINGERPRINT_LEN 65

typedef enum {
    RESP_SUCCESS,
    RESP_FAILURE,
    RESP_FILE_NOT_FOUND,
    RESP_PERMISSION_DENIED,
    RESP_ERROR,
    RESP_INVALID_OFFSET,
    RESP_INTEGRITY_ERROR,
    RESP_UNKNOWN_COMMAND,
    RESP_WAITING_APPROVAL = 100, 
    RESP_APPROVED = 101,         // Подключение подтверждено
    RESP_REJECTED = 102          // Подключение отклонено
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


// Объявления функций

int send_all(int sockfd, const void *buffer, size_t len);
int recv_all(int sockfd, void *buffer, size_t len);

#endif