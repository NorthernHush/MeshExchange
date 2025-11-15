#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

// Cryptographic constants
#define FILENAME_MAX_LEN 256
#define BUFFER_SIZE      4096
#define BLAKE3_HASH_LEN  32
#define XCHACHA20_KEY_LEN 32
#define XCHACHA20_NONCE_LEN 24
#define ECDH_PUBLIC_KEY_LEN 32
#define ECDH_PRIVATE_KEY_LEN 32
#define SESSION_KEY_LEN 32
#define ENCRYPTED_METADATA_MAX_LEN (FILENAME_MAX_LEN + 16 + 24) // filename + auth tag + nonce
#define DEFAULT_PORT 1512

// Anonymity and security constants
#define FINGERPRINT_LEN 65
#define TOR_PROXY_PORT 9050
#define MAX_CONNECTIONS_PER_IP 10
#define RATE_LIMIT_WINDOW_SEC 60
#define MAX_REQUESTS_PER_WINDOW 100

// Command types with new security features
typedef enum {
    CMD_UPLOAD,
    CMD_DOWNLOAD,
    CMD_LIST,
    CMD_UNKNOWN,
    CMD_CONNECT = 99,      // Initial connection handshake
    CMD_CHECK = 100,       // Admin: check fingerprint
    CMD_APPROVE = 101,     // Admin: approve connection
    CMD_ECDH_INIT = 102,   // ECDH key exchange initiation
    CMD_ECDH_RESP = 103,   // ECDH key exchange response
    CMD_SESSION_KEY = 104, // Session key establishment
    CMD_PING = 105,        // Keep-alive ping
    CMD_DISCONNECT = 106   // Graceful disconnect
} CommandType;

// Server options
typedef enum {
    OPEN_SERVER,
    OFF_USERS,
    CHECK_CLIENTS,
} OptionUserServer;

// Response statuses with enhanced security
typedef enum {
    RESP_SUCCESS,
    RESP_FAILURE,
    RESP_FILE_NOT_FOUND,
    RESP_PERMISSION_DENIED,
    RESP_ERROR,
    RESP_INVALID_OFFSET,
    RESP_INTEGRITY_ERROR,
    RESP_UNKNOWN_COMMAND,
    RESP_RATE_LIMITED = 50,     // DoS protection: rate limited
    RESP_CONNECTION_LIMIT = 51, // Too many connections from IP
    RESP_INVALID_KEY = 52,      // Invalid cryptographic key
    RESP_AUTH_FAILED = 53,      // Authentication failed
    RESP_ENCRYPTION_ERROR = 54, // Encryption/decryption error
    RESP_WAITING_APPROVAL = 100, // Waiting for admin approval
    RESP_APPROVED = 101,         // Connection approved
    RESP_REJECTED = 102,         // Connection rejected
    RESP_BANNED = 103            // Client is banned
} ResponseStatus;

// ECDH key exchange structures
typedef struct {
    uint8_t public_key[ECDH_PUBLIC_KEY_LEN];
    uint8_t nonce[XCHACHA20_NONCE_LEN]; // For encrypted metadata
} ECDHInitPacket;

typedef struct {
    uint8_t public_key[ECDH_PUBLIC_KEY_LEN];
    uint8_t encrypted_metadata[ENCRYPTED_METADATA_MAX_LEN];
    uint8_t auth_tag[16]; // Poly1305 auth tag
} ECDHResponsePacket;

// Session key establishment
typedef struct {
    uint8_t session_key[SESSION_KEY_LEN];
    uint8_t key_hash[BLAKE3_HASH_LEN]; // For integrity verification
} SessionKeyPacket;

// Encrypted metadata structure
typedef struct {
    uint8_t encrypted_filename[ENCRYPTED_METADATA_MAX_LEN];
    uint8_t filename_auth_tag[16];
    uint8_t encrypted_size[sizeof(long long) + 16]; // size + auth tag
    uint8_t size_auth_tag[16];
    uint8_t encrypted_recipient[FINGERPRINT_LEN + 16];
    uint8_t recipient_auth_tag[16];
    uint8_t nonce[XCHACHA20_NONCE_LEN];
} EncryptedMetadata;

// Enhanced request header with encryption
typedef struct {
    CommandType command;
    EncryptedMetadata metadata; // Encrypted filename, size, recipient
    int64_t offset;
    uint8_t flags; // bit 0 = public, bit 1 = anonymous
    uint8_t file_hash[BLAKE3_HASH_LEN]; // Integrity hash
    uint8_t packet_nonce[XCHACHA20_NONCE_LEN]; // Unique nonce per packet
    uint8_t auth_tag[16]; // Authentication tag for entire header
} RequestHeader;

// Enhanced response header
typedef struct {
    ResponseStatus status;
    long long filesize; // For download responses
    uint8_t response_nonce[XCHACHA20_NONCE_LEN];
    uint8_t auth_tag[16];
} ResponseHeader;

// Connection state for rate limiting
typedef struct {
    uint32_t ip_address;
    uint32_t request_count;
    time_t window_start;
} ConnectionState;

// Function declarations
int send_all(int sockfd, const void *buffer, size_t len);
int recv_all(int sockfd, void *buffer, size_t len);

// New cryptographic functions
int encrypt_metadata(const uint8_t *key, const uint8_t *nonce,
                    const char *filename, long long filesize, const char *recipient,
                    EncryptedMetadata *encrypted);

int decrypt_metadata(const uint8_t *key, const EncryptedMetadata *encrypted,
                    char *filename, long long *filesize, char *recipient);

#endif
