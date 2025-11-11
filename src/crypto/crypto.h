#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// Error codes for crypto operations
typedef enum {
    CRYPTO_OK = 0,
    CRYPTO_ERR_INVALID_INPUT = 1,
    CRYPTO_ERR_AUTH_FAILED = 2,
    CRYPTO_ERR_MEMORY = 3,
    CRYPTO_ERR_OPENSSL = 4
} crypto_status_t;

// Secure key structure
typedef struct {
    volatile uint8_t key[32];  // 256-bit key, volatile to prevent optimization
    int initialized;
} secure_key_t;

// Context for crypto operations
typedef struct {
    secure_key_t *key;
    // Add other context fields as needed
} crypto_session_t;

// Function declarations
crypto_status_t crypto_decrypt_aes_gcm(
    const uint8_t *data, size_t data_len,
    const secure_key_t *key,
    uint8_t *plaintext, size_t *plaintext_len
);

#endif // CRYPTO_H
