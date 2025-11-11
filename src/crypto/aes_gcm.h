#ifndef AES_GCM_H
#define AES_GCM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../lib/error.h"  // For error_status_t

#define AES_KEY_SIZE    32  // 256 bit
#define AES_BLOCK_SIZE  16  // Block size AES
#define GCM_TAG_SIZE    16  // GCM authentication tag size
#define GCM_IV_SIZE     12  // GCM IV size (96 bits)

// Structure for storing key and initialization vector
typedef struct {
    unsigned char key[AES_KEY_SIZE];
    unsigned char iv[AES_BLOCK_SIZE];
} AES_CONTEXT;

// Encryption function (existing)
int crypto_encrypt_aes_gcm(const uint8_t *pt, size_t pt_len, const uint8_t *key, uint8_t *ct, uint8_t *iv, uint8_t *tag);

// Legacy decryption function (deprecated - use crypto_decrypt_aes_gcm from crypto.h)
// Decrypts AES-256-GCM or ChaCha20-Poly1305 encrypted data.
// Parameters:
// - ct: ciphertext buffer
// - ct_len: length of ciphertext
// - key: 32-byte key
// - iv: 12-byte IV for GCM, 12-byte nonce for ChaCha20
// - tag: 16-byte authentication tag
// - pt: output plaintext buffer (must be at least ct_len bytes)
// - pt_len: output length of decrypted plaintext
// - use_chacha: if true, use ChaCha20-Poly1305; else AES-256-GCM
// Returns: MR_SUCCESS on success, error code otherwise
error_status_t crypto_decrypt_aes_gcm_legacy(const uint8_t *ct, size_t ct_len,
                                             const uint8_t *key, const uint8_t *iv,
                                             const uint8_t *tag, uint8_t *pt,
                                             size_t *pt_len, bool use_chacha);

#endif
