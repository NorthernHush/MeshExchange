#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <criterion/criterion.h>

#include "../include/protocol.h"

// Test RequestHeader structure
Test(protocol, request_header_basic) {
    RequestHeader header = {0};

    // Test setting values
    header.command = CMD_UPLOAD;
    strcpy(header.filename, "test.txt");
    header.filesize = 1024;
    header.offset = 0;
    header.flags = FLAG_ENCRYPTED;
    // Set a mock hash
    memset(header.file_hash, 0xAA, BLAKE3_HASH_LEN);
    strcpy(header.recipient, "user@example.com");

    // Verify values
    cr_assert_eq(header.command, CMD_UPLOAD);
    cr_assert_str_eq(header.filename, "test.txt");
    cr_assert_eq(header.filesize, 1024);
    cr_assert_eq(header.offset, 0);
    cr_assert_eq(header.flags, FLAG_ENCRYPTED);
    cr_assert_str_eq(header.recipient, "user@example.com");

    // Verify hash
    for (int i = 0; i < BLAKE3_HASH_LEN; i++) {
        cr_assert_eq(header.file_hash[i], 0xAA);
    }
}

// Test ResponseHeader structure
Test(protocol, response_header_basic) {
    ResponseHeader header = {0};

    header.status = STATUS_OK;
    header.filesize = 2048;

    cr_assert_eq(header.status, STATUS_OK);
    cr_assert_eq(header.filesize, 2048);
}

// Test command constants
Test(protocol, command_constants) {
    cr_assert_eq(CMD_UPLOAD, 1);
    cr_assert_eq(CMD_DOWNLOAD, 2);
    cr_assert_eq(CMD_LIST, 3);
    cr_assert_eq(CMD_DELETE, 4);
}

// Test status constants
Test(protocol, status_constants) {
    cr_assert_eq(STATUS_OK, 0);
    cr_assert_eq(STATUS_ERROR, 1);
    cr_assert_eq(STATUS_NOT_FOUND, 2);
    cr_assert_eq(STATUS_ACCESS_DENIED, 3);
    cr_assert_eq(STATUS_INVALID_REQUEST, 4);
}

// Test flag constants
Test(protocol, flag_constants) {
    cr_assert_eq(FLAG_NONE, 0);
    cr_assert_eq(FLAG_ENCRYPTED, 1);
    cr_assert_eq(FLAG_COMPRESSED, 2);
    cr_assert_eq(FLAG_PRIVATE, 4);
}

// Test header sizes
Test(protocol, header_sizes) {
    // RequestHeader should be fixed size
    cr_assert_eq(sizeof(RequestHeader), 256 + 32 + 8 + 8 + 4 + 32 + 256);

    // ResponseHeader should be smaller
    cr_assert_eq(sizeof(ResponseHeader), 4 + 8);
}

// Test filename length limits
Test(protocol, filename_length_limits) {
    RequestHeader header = {0};

    // Test maximum filename length
    memset(header.filename, 'a', FILENAME_MAX_LEN - 1);
    header.filename[FILENAME_MAX_LEN - 1] = '\0';

    cr_assert_eq(strlen(header.filename), FILENAME_MAX_LEN - 1);

    // Test that we don't exceed buffer
    cr_assert_lt(strlen(header.filename), sizeof(header.filename));
}

// Test recipient field
Test(protocol, recipient_field) {
    RequestHeader header = {0};

    strcpy(header.recipient, "recipient@example.com");
    cr_assert_str_eq(header.recipient, "recipient@example.com");

    // Test empty recipient (public file)
    strcpy(header.recipient, "");
    cr_assert_str_eq(header.recipient, "");
}

// Test hash field size
Test(protocol, hash_field_size) {
    RequestHeader header = {0};

    cr_assert_eq(sizeof(header.file_hash), BLAKE3_HASH_LEN);

    // Test setting hash
    for (int i = 0; i < BLAKE3_HASH_LEN; i++) {
        header.file_hash[i] = (uint8_t)i;
    }

    for (int i = 0; i < BLAKE3_HASH_LEN; i++) {
        cr_assert_eq(header.file_hash[i], (uint8_t)i);
    }
}

// Test protocol version compatibility (if defined)
Test(protocol, protocol_version) {
    // If PROTOCOL_VERSION is defined, test it
#ifdef PROTOCOL_VERSION
    cr_assert_geq(PROTOCOL_VERSION, 1);
#endif
}

// Test flag combinations
Test(protocol, flag_combinations) {
    uint32_t flags = FLAG_ENCRYPTED | FLAG_PRIVATE;
    cr_assert_eq(flags, 5); // 1 + 4

    flags = FLAG_ENCRYPTED | FLAG_COMPRESSED | FLAG_PRIVATE;
    cr_assert_eq(flags, 7); // 1 + 2 + 4
}

// Test boundary values for filesize
Test(protocol, filesize_boundary_values) {
    RequestHeader req_header = {0};
    ResponseHeader resp_header = {0};

    // Test zero size
    req_header.filesize = 0;
    resp_header.filesize = 0;
    cr_assert_eq(req_header.filesize, 0);
    cr_assert_eq(resp_header.filesize, 0);

    // Test maximum size
    req_header.filesize = UINT64_MAX;
    resp_header.filesize = UINT64_MAX;
    cr_assert_eq(req_header.filesize, UINT64_MAX);
    cr_assert_eq(resp_header.filesize, UINT64_MAX);
}
