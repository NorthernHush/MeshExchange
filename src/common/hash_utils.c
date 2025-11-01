#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blake3.h"

#include "hash_utils.h"

int compute_file_blake3(const char *filepath, uint8_t out_hash[HASH_SIZE]) {

    blake3_hasher hasher;

    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data, len);

    blake3_hasher_finalize(&hasher, out_hash, HASH_SIZE);

}

// todo: compute_file_blake3(...)

int compute_file_blake3(const char *filepath, uint8_t out_hash[HASH_SIZE]) {
    FILE *fp = fopen(filepath, "rb");

    if (!fp) return -1;

}