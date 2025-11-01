# # gcc -o server server.c \
# #     -I/usr/include/libmongoc-1.0 \
# #     -I/usr/include/libbson-1.0 \
# #     -L/usr/lib \
# #     -lmongoc-1.0 -lbson-1.0 -lssl -lcrypto -lsasl2 -lz
# # gcc -o server server.c ../db/mongo_ops.c $(pkg-config --cflags --libs libmongoc-1.0) -lpthread

# # Компилируем все .c файлы в .o файлы
# gcc -c server.c -o server.o -Wall -Wextra $(pkg-config --cflags libmongoc-1.0)
# gcc -c ../db/mongo_ops_server.c -o mongo_ops_server.o -Wall -Wextra $(pkg-config --cflags libmongoc-1.0)
# gcc -c ../utils/utils.c -o utils.o -Wall -Wextra
# gcc -c ../crypto/aes_gcm.c -o aes_gcm.o
# gcc -c ../../deps/c/blake3.c

# # Линкуем все .o файлы и библиотеки
# gcc -o server server.o mongo_ops_server.o utils.o aes_gcm.o $(pkg-config --libs libmongoc-1.0) -lssl -lcrypto
#!/bin/bash
set -e

# Общие объекты (без SIMD)
gcc -c server.c -o server.o -Iinclude -Ideps/blake3 -Wall -Wextra $(pkg-config --cflags libmongoc-1.0)
gcc -c ../db/mongo_ops_server.c -o mongo_ops_server.o -Iinclude -Ideps/blake3 -Wall -Wextra $(pkg-config --cflags libmongoc-1.0)
gcc -c ../utils/utils.c -o utils.o -Iinclude -Ideps/blake3 -Wall -Wextra
gcc -c ../crypto/aes_gcm.c -o aes_gcm.o -Iinclude -Ideps/blake3 -Wall -Wextra

BLAKE3_DIR=../../deps/blake3
# Компилируем BLAKE3 без AVX512
gcc -c $BLAKE3_DIR/blake3.c -o blake3.o -I$BLAKE3_DIR -Wall -Wextra
gcc -c $BLAKE3_DIR/blake3_dispatch.c -o blake3_dispatch.o -I$BLAKE3_DIR -Wall -Wextra
gcc -c $BLAKE3_DIR/blake3_portable.c -o blake3_portable.o -I$BLAKE3_DIR -Wall -Wextra
gcc -c $BLAKE3_DIR/blake3_sse2.c -o blake3_sse2.o -I$BLAKE3_DIR -Wall -Wextra -msse2
gcc -c $BLAKE3_DIR/blake3_sse41.c -o blake3_sse41.o -I$BLAKE3_DIR -Wall -Wextra -mssse3 -msse4.1
gcc -c $BLAKE3_DIR/blake3_avx2.c -o blake3_avx2.o -I$BLAKE3_DIR -Wall -Wextra -mavx2

# НЕ компилируем blake3_avx512.c
gcc -o server server.o mongo_ops_server.o utils.o aes_gcm.o \
    blake3.o blake3_dispatch.o blake3_portable.o \
    blake3_sse2.o blake3_sse41.o blake3_avx2.o blake3_avx512.o \
    $(pkg-config --libs libmongoc-1.0) -lssl -lcrypto -lpthread