#ifndef CRYPTO_SESSION_H
#define CRYPTO_SESSION_H

#include "crypto.h"

// Secure key management functions
secure_key_t* secure_key_create(void);
void secure_key_destroy(secure_key_t *key);
crypto_status_t secure_key_set(secure_key_t *key, const uint8_t *data, size_t len);

// Session management
crypto_session_t* crypto_session_create(secure_key_t *key);
void crypto_session_destroy(crypto_session_t *session);

#endif // CRYPTO_SESSION_H
