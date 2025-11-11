
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // For explicit_bzero
#include <sys/mman.h>  // For mlock
#include <sys/prctl.h>  // For prctl
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <stdbool.h>

#include "aes_gcm.h"  // For constants and legacy function
#include "crypto.h"  // For crypto_status_t and secure_key_t
#include "crypto_session.h"  // For session management
#include "../lib/error.h"  // For error_status_t compatibility
#include "../utils/log_crypto.h"  // For crypto logging

// Legacy function for backward compatibility
error_status_t crypto_decrypt_aes_gcm_legacy(const uint8_t *ct, size_t ct_len,
                                             const uint8_t *key, const uint8_t *iv,
                                             const uint8_t *tag, uint8_t *pt,
                                             size_t *pt_len, bool use_chacha) {
    // Input validation
    if (!ct || !key || !iv || !tag || !pt || !pt_len || ct_len == 0) {
        return MR_ERROR_INVALID_PARAM;
    }

    // Validate buffer sizes
    if (ct_len > SIZE_MAX - GCM_TAG_SIZE) {
        return MR_ERROR_INVALID_PARAM;
    }

    EVP_CIPHER_CTX *ctx = NULL;
    error_status_t result = MR_ERROR_CRYPTO;
    int len = 0;
    *pt_len = 0;

    // Lock memory to prevent swapping
    if (mlock(pt, ct_len) != 0) {
        // Log but continue - not fatal
        log_crypto_event(LOG_CRYPTO_WARNING, CRYPTO_OP_DECRYPT, "Failed to lock memory for decryption");
    }

    // Set memory protection
    if (prctl(PR_SET_SECCOMP, 1, 0, 0, 0) != 0) {
        // Log but continue
        log_crypto_event(LOG_CRYPTO_WARNING, CRYPTO_OP_DECRYPT, "Failed to set memory protection");
    }

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to create EVP context");
        goto cleanup;
    }

    // Initialize decryption context
    const EVP_CIPHER *cipher = use_chacha ? EVP_chacha20_poly1305() : EVP_aes_256_gcm();
    if (EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to initialize decryption");
        goto cleanup;
    }

    // Set IV length for GCM (ChaCha20 uses fixed 12-byte nonce)
    if (!use_chacha) {
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_SIZE, NULL) != 1) {
            char err_buf[256];
            ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
            log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to set IV length");
            goto cleanup;
        }
    }

    // Initialize with key and IV
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to set key and IV");
        goto cleanup;
    }

    // Decrypt ciphertext
    if (EVP_DecryptUpdate(ctx, pt, &len, ct, ct_len) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to decrypt data");
        goto cleanup;
    }
    *pt_len = len;

    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, (void*)tag) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to set authentication tag");
        goto cleanup;
    }

    // Finalize decryption and verify tag
    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx, pt + len, &final_len) != 1) {
        // Authentication failed - do not log details to prevent timing attacks
        result = MR_ERROR_INTEGRITY;
        goto cleanup;
    }
    *pt_len += final_len;

    result = MR_SUCCESS;

cleanup:
    // Zero sensitive data on failure
    if (result != MR_SUCCESS) {
        explicit_bzero(pt, ct_len);
        *pt_len = 0;
    }

    // Clean up context
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }

    // Unlock memory
    munlock(pt, ct_len);

    return result;
}

// New secure decryption function with [IV][Tag][Ciphertext] input format
crypto_status_t crypto_decrypt_aes_gcm(const uint8_t *encrypted_data, size_t data_len,
                                       const secure_key_t *key, uint8_t *plaintext,
                                       size_t *plaintext_len) {
    // Input validation
    if (!encrypted_data || !key || !plaintext || !plaintext_len) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    // Check minimum data length: IV(12) + Tag(16) + at least 1 byte ciphertext
    if (data_len < GCM_IV_SIZE + GCM_TAG_SIZE + 1) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    // Extract components from input: [IV(12)][Tag(16)][Ciphertext]
    const uint8_t *iv = encrypted_data;
    const uint8_t *tag = encrypted_data + GCM_IV_SIZE;
    const uint8_t *ciphertext = encrypted_data + GCM_IV_SIZE + GCM_TAG_SIZE;
    size_t ct_len = data_len - GCM_IV_SIZE - GCM_TAG_SIZE;

    // Validate key
    if (!key->initialized) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    EVP_CIPHER_CTX *ctx = NULL;
    crypto_status_t result = CRYPTO_ERR_OPENSSL;
    int len = 0;
    *plaintext_len = 0;

    // Lock memory to prevent swapping
    if (mlock(plaintext, ct_len) != 0) {
        log_crypto_event(LOG_CRYPTO_WARNING, CRYPTO_OP_DECRYPT, "Failed to lock memory for decryption");
    }

    // Set memory protection
    if (prctl(PR_SET_SECCOMP, 1, 0, 0, 0) != 0) {
        log_crypto_event(LOG_CRYPTO_WARNING, CRYPTO_OP_DECRYPT, "Failed to set memory protection");
    }

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to create EVP context");
        goto cleanup;
    }

    // Initialize decryption context (AES-GCM by default)
    const EVP_CIPHER *cipher = EVP_aes_256_gcm();
    if (EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to initialize decryption");
        goto cleanup;
    }

    // Set IV length for GCM
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_SIZE, NULL) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to set IV length");
        goto cleanup;
    }

    // Initialize with key and IV
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key->key, iv) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to set key and IV");
        goto cleanup;
    }

    // Decrypt ciphertext
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ct_len) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to decrypt data");
        goto cleanup;
    }
    *plaintext_len = len;

    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, (void*)tag) != 1) {
        char err_buf[256];
        ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
        log_crypto_event(LOG_CRYPTO_ERROR, CRYPTO_OP_DECRYPT, "Failed to set authentication tag");
        goto cleanup;
    }

    // Finalize decryption and verify tag using constant-time comparison
    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &final_len) == 1) {
        *plaintext_len += final_len;
        result = CRYPTO_OK;
    } else {
        // Authentication failed - use constant-time comparison
        // Note: EVP_DecryptFinal_ex already performs constant-time tag verification
        result = CRYPTO_ERR_AUTH_FAILED;
    }

cleanup:
    // Zero sensitive data on failure
    if (result != CRYPTO_OK) {
        explicit_bzero(plaintext, ct_len);
        *plaintext_len = 0;
    }

    // Clean up context
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }

    // Unlock memory
    munlock(plaintext, ct_len);

    return result;
}

// Example usage function
void example_decrypt_usage(void) {
    // Example: Decrypt data using legacy API
    uint8_t key[32] = {0}; // 32-byte key
    uint8_t iv[12] = {0};  // 12-byte IV
    uint8_t tag[16] = {0}; // 16-byte tag
    uint8_t ciphertext[1024] = {0};
    uint8_t plaintext[1024] = {0};
    size_t ct_len = 100; // Example ciphertext length
    size_t pt_len = 0;

    error_status_t result = crypto_decrypt_aes_gcm_legacy(
        ciphertext, ct_len, key, iv, tag, plaintext, &pt_len, false // false = AES-GCM
    );

    if (result == MR_SUCCESS) {
        // Decryption successful, plaintext contains decrypted data
        printf("Decryption successful, plaintext length: %zu\n", pt_len);
    } else {
        // Handle error
        printf("Decryption failed with error: %d\n", result);
    }
}
