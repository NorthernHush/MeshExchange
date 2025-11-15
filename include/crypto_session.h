#ifndef CRYPTO_SESSION_H
#define CRYPTO_SESSION_H

#include <stdint.h>
#include <sodium.h>
#include "protocol.h"

// Session context for ECDH key exchange and encryption
typedef struct {
    uint8_t private_key[32];
    uint8_t public_key[32];
    uint8_t peer_public_key[32];
    uint8_t session_key[32];
    uint8_t shared_secret[32];
    int ecdh_completed;
    int session_established;
} crypto_session_t;

// Function declarations
int crypto_session_init(crypto_session_t *session);
int crypto_session_generate_keys(crypto_session_t *session);
int crypto_session_compute_shared_secret(crypto_session_t *session);
int crypto_session_derive_session_key(crypto_session_t *session);
int crypto_session_encrypt_metadata(crypto_session_t *session,
                                   const char *filename, long long filesize,
                                   const char *recipient, EncryptedMetadata *encrypted);
int crypto_session_decrypt_metadata(crypto_session_t *session,
                                   const EncryptedMetadata *encrypted,
                                   char *filename, long long *filesize, char *recipient);
void crypto_session_cleanup(crypto_session_t *session);

#endif
