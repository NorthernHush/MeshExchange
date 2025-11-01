gcc -o mongo_client mongo_client.c \
    -I/usr/include/libmongoc-1.0 \
    -I/usr/include/libbson-1.0 \
    -L/usr/lib \
    -lmongoc-1.0 -lbson-1.0 -lssl -lcrypto -lsasl2 -lz
