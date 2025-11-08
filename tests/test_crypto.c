#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <criterion/criterion.h>

#include "../src/crypto/aes_gcm.h"

// Test AES-GCM encryption/decryption roundtrip
Test(crypto, aes_gcm_encrypt_decrypt_roundtrip) {
    // Test data
    const char *plaintext = "Hello, World! This is a test message for AES-GCM encryption.";
    size_t pt_len = strlen(plaintext);

    // Key (256-bit)
    uint8_t key[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };

    // Buffers for ciphertext and decrypted text
    uint8_t *ciphertext = malloc(pt_len + 16); // +16 for auth tag
    uint8_t *decrypted = malloc(pt_len + 1);
    uint8_t iv[12]; // 96-bit IV
    uint8_t tag[16]; // 128-bit auth tag

    cr_assert_not_null(ciphertext);
    cr_assert_not_null(decrypted);

    // Generate random IV
    RAND_bytes(iv, sizeof(iv));

    // Encrypt
    int encrypt_result = crypto_encrypt_aes_gcm(
        (const uint8_t *)plaintext, pt_len,
        key, iv, ciphertext, tag
    );
    cr_assert_eq(encrypt_result, 0, "Encryption should succeed");

    // Decrypt
    int decrypt_result = crypto_decrypt_aes_gcm(
        ciphertext, pt_len,
        key, iv, tag, decrypted
    );
    cr_assert_eq(decrypt_result, 0, "Decryption should succeed");

    // Verify plaintext matches
    decrypted[pt_len] = '\0';
    cr_assert_str_eq((char *)decrypted, plaintext, "Decrypted text should match original");

    free(ciphertext);
    free(decrypted);
}

// Test AES-GCM with empty plaintext
Test(crypto, aes_gcm_empty_plaintext) {
    uint8_t key[32] = {0};
    uint8_t iv[12] = {0};
    uint8_t ciphertext[16];
    uint8_t tag[16];
    uint8_t decrypted[1];

    int encrypt_result = crypto_encrypt_aes_gcm(NULL, 0, key, iv, ciphertext, tag);
    cr_assert_eq(encrypt_result, 0);

    int decrypt_result = crypto_decrypt_aes_gcm(ciphertext, 0, key, iv, tag, decrypted);
    cr_assert_eq(decrypt_result, 0);
}

// Test AES-GCM authentication failure (tampered ciphertext)
Test(crypto, aes_gcm_authentication_failure) {
    const char *plaintext = "Test message";
    size_t pt_len = strlen(plaintext);
    uint8_t key[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    uint8_t iv[12] = {0};
    uint8_t ciphertext[256];
    uint8_t tag[16];
    uint8_t decrypted[256];

    // Encrypt
    int encrypt_result = crypto_encrypt_aes_gcm(
        (const uint8_t *)plaintext, pt_len, key, iv, ciphertext, tag
    );
    cr_assert_eq(encrypt_result, 0);

    // Tamper with ciphertext
    ciphertext[0] ^= 0xFF;

    // Decrypt should fail due to authentication
    int decrypt_result = crypto_decrypt_aes_gcm(
        ciphertext, pt_len, key, iv, tag, decrypted
    );
    cr_assert_neq(decrypt_result, 0, "Decryption should fail with tampered data");
}

// Test AES-GCM with different IVs produce different ciphertexts
Test(crypto, aes_gcm_different_iv_different_ciphertext) {
    const char *plaintext = "Same message";
    size_t pt_len = strlen(plaintext);
    uint8_t key[32] = {0};
    uint8_t iv1[12] = {0};
    uint8_t iv2[12] = {1};
    uint8_t ct1[256], ct2[256];
    uint8_t tag1[16], tag2[16];

    int result1 = crypto_encrypt_aes_gcm((const uint8_t *)plaintext, pt_len, key, iv1, ct1, tag1);
    int result2 = crypto_encrypt_aes_gcm((const uint8_t *)plaintext, pt_len, key, iv2, ct2, tag2);

    cr_assert_eq(result1, 0);
    cr_assert_eq(result2, 0);

    // Ciphertexts should be different
    cr_assert_neq(memcmp(ct1, ct2, pt_len), 0, "Different IVs should produce different ciphertexts");
}
