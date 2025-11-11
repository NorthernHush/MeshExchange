#ifndef LOG_CRYPTO_H
#define LOG_CRYPTO_H

#include <stdint.h>

// Log levels for crypto operations
typedef enum {
    LOG_CRYPTO_DEBUG = 0,
    LOG_CRYPTO_INFO = 1,
    LOG_CRYPTO_WARNING = 2,
    LOG_CRYPTO_ERROR = 3
} log_crypto_level_t;

// Crypto operation codes for logging
typedef enum {
    CRYPTO_OP_ENCRYPT = 1,
    CRYPTO_OP_DECRYPT = 2,
    CRYPTO_OP_KEY_GEN = 3,
    CRYPTO_OP_TAG_VERIFY = 4
} crypto_op_code_t;

// Log a crypto event
void log_crypto_event(log_crypto_level_t level, crypto_op_code_t code, const char *ctx);

#endif // LOG_CRYPTO_H
