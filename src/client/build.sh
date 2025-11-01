# gcc -o client client.c ../db/mongo_ops.c ../utils/utils.c ../../deps/c/blake3.c -Wall -Wextra $(pkg-config --cflags --libs libmongoc-1.0) -lcrypto -lssl
# # client: src/client/client.c
# #	gcc -Iinclude -Ideps -o client src/client/client.c deps/blake3.c -lssl -lcrypto -lpthread

# #!/bin/bash
# set -e

# # Убедитесь, что client.c в src/client/
# gcc -c client.c -o client.o -I../../include -I../../deps/blake3 -Wall -Wextra

# # BLAKE3
# gcc -c ../../../deps/blake3/blake3.c -o blake3.o -I../../deps/blake3 -Wall -Wextra
# gcc -c ../../../deps/blake3/blake3_dispatch.c -o blake3_dispatch.o -I../../deps/blake3 -Wall -Wextra
# gcc -c ../../../deps/blake3/blake3_portable.c -o blake3_portable.o -I../../deps/blake3 -Wall -Wextra

# # Линковка
# gcc -o client client.o \
#     blake3.o blake3_dispatch.o blake3_portable.o \
#     -lssl -lcrypto -lpthread
#!/bin/bash
set -e

# Общие объекты (без SIMD)
gcc -c client.c -o client.o -Iinclude -Ideps/blake3 -Wall -Wextra $(pkg-config --cflags libmongoc-1.0)
gcc -c ../db/mongo_ops.c -o mongo_ops.o -Iinclude -Ideps/blake3 -Wall -Wextra $(pkg-config --cflags libmongoc-1.0)
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
gcc -o client client.o mongo_ops.o utils.o aes_gcm.o \
    blake3.o blake3_dispatch.o blake3_portable.o \
    blake3_sse2.o blake3_sse41.o blake3_avx2.o blake3_avx512.o \
    $(pkg-config --libs libmongoc-1.0) -lssl -lcrypto -lpthread