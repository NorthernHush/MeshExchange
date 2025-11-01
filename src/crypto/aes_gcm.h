#ifndef AES_GCM_H
#define AES_GCM_H

#include <stddef.h>
#include <stdint.h>
#define AES_KEY_SIZE    32  //* 256 bit
#define AES_BLOCK_SIZE  16 //* block size AES

//* структура для хранения ключа и вектора инициализации
//* iv
typedef struct {
    unsigned char key[AES_KEY_SIZE];
    unsigned char iv[AES_BLOCK_SIZE];
} AES_CONTEXT;

int crypto_encrypt_aes_gcm(const uint8_t *pt, size_t pt_len, const uint8_t *key, uint8_t *ct, uint8_t *iv, uint8_t *tag);




#endif