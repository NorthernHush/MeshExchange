#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/evp.h>

//* заголовочные файлы
#include "aes_gcm.h"

int crypto_encrypt_aes_gcm(const uint8_t *pt, size_t pt_len, const uint8_t *key, uint8_t *ct, uint8_t *iv, uint8_t *tag) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) goto err;

    // Установка IV length
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1) goto err;

    // Генерация случайного IV (предполагается, что iv имеет размер 12 байт)
    RAND_bytes(iv, 12);

    // Инициализация шифрования с ключом и IV
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1) goto err;


    int len, ct_len = 0;
    if (EVP_EncryptUpdate(ctx, ct, &len, pt, pt_len) != 1) goto err;

    ct_len = len;

    if (EVP_EncryptFinal_ex(ctx, ct + len, &len) != 1) goto err;

    ct_len += len;


    // Получение Tag (предполагается, что tag имеет размер 16 байт)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) goto err;

    EVP_CIPHER_CTX_free(ctx);

    return ct_len;

err:
    EVP_CIPHER_CTX_free(ctx);
    return -1;
}
