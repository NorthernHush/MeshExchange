#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#include <cstddef>
#include <stdint.h>

#define HASH_SIZE 32

// Вычисляет BLAKE3-хеш файла по пути
int compute_file_blake3(const char *filepath, uint8_t out_hash[HASH_SIZE]);

// Вычисляет BLAKE3-хеш буфера в памяти
void compute_buffer_blake3(const uint8_t *data, size_t len, uint8_t out_hash[HASH_SIZE]);

#endif