#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <criterion/criterion.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../tests/mocks/mock_ssl.h"
#include "../tests/mocks/mock_socket.h"

// Test SSL initialization (mocked)
Test(ssl, ssl_initialization) {
    // Test that we can create mock SSL context
    SSL_CTX *mock_ctx = create_mock_ssl_ctx();
    cr_assert_not_null(mock_ctx);

    // Test that we can create mock SSL connection
    SSL *mock_ssl = create_mock_ssl(mock_ctx);
    cr_assert_not_null(mock_ssl);

    // Cleanup
    destroy_mock_ssl(mock_ssl);
    destroy_mock_ssl_ctx(mock_ctx);
}

// Test send_all function with mock socket
Test(ssl, send_all_complete_buffer) {
    // Create mock socket
    int mock_sock = create_mock_socket();
    cr_assert_geq(mock_sock, 0);

    const char *test_data = "Hello, World!";
    size_t data_len = strlen(test_data);

    // Send data
    ssize_t sent = send_all(mock_sock, test_data, data_len);
    cr_assert_eq(sent, data_len);

    // Verify data was "sent"
    char *received = get_mock_socket_data(mock_sock);
    cr_assert_not_null(received);
    cr_assert_str_eq(received, test_data);

    destroy_mock_socket(mock_sock);
}

// Test send_all with partial sends (mocked)
Test(ssl, send_all_partial_sends) {
    int mock_sock = create_mock_socket();
    set_mock_socket_partial_writes(mock_sock, 1); // Simulate partial writes

    const char *test_data = "Partial write test";
    size_t data_len = strlen(test_data);

    ssize_t sent = send_all(mock_sock, test_data, data_len);
    cr_assert_eq(sent, data_len);

    destroy_mock_socket(mock_sock);
}

// Test send_all with error
Test(ssl, send_all_error) {
    int mock_sock = create_mock_socket();
    set_mock_socket_error(mock_sock, -1); // Simulate write error

    const char *test_data = "This should fail";
    size_t data_len = strlen(test_data);

    ssize_t sent = send_all(mock_sock, test_data, data_len);
    cr_assert_eq(sent, -1);

    destroy_mock_socket(mock_sock);
}

// Test recv_all function
Test(ssl, recv_all_complete_buffer) {
    int mock_sock = create_mock_socket();

    const char *test_data = "Received data";
    size_t data_len = strlen(test_data);

    // Pre-populate mock socket with data
    set_mock_socket_recv_data(mock_sock, test_data, data_len);

    char buffer[256];
    ssize_t received = recv_all(mock_sock, buffer, data_len);
    cr_assert_eq(received, data_len);
    buffer[data_len] = '\0';
    cr_assert_str_eq(buffer, test_data);

    destroy_mock_socket(mock_sock);
}

// Test recv_all with partial receives
Test(ssl, recv_all_partial_receives) {
    int mock_sock = create_mock_socket();
    set_mock_socket_partial_reads(mock_sock, 1);

    const char *test_data = "Partial receive test";
    size_t data_len = strlen(test_data);

    set_mock_socket_recv_data(mock_sock, test_data, data_len);

    char buffer[256];
    ssize_t received = recv_all(mock_sock, buffer, data_len);
    cr_assert_eq(received, data_len);
    buffer[data_len] = '\0';
    cr_assert_str_eq(buffer, test_data);

    destroy_mock_socket(mock_sock);
}

// Test recv_all with error
Test(ssl, recv_all_error) {
    int mock_sock = create_mock_socket();
    set_mock_socket_recv_error(mock_sock, -1);

    char buffer[256];
    ssize_t received = recv_all(mock_sock, buffer, 100);
    cr_assert_eq(received, -1);

    destroy_mock_socket(mock_sock);
}

// Test SSL_write wrapper (mocked)
Test(ssl, ssl_write_wrapper) {
    SSL_CTX *mock_ctx = create_mock_ssl_ctx();
    SSL *mock_ssl = create_mock_ssl(mock_ctx);

    const char *test_data = "SSL write test";
    size_t data_len = strlen(test_data);

    int written = SSL_write(mock_ssl, test_data, data_len);
    cr_assert_eq(written, data_len);

    // Verify data
    char *ssl_data = get_mock_ssl_data(mock_ssl);
    cr_assert_not_null(ssl_data);
    cr_assert_str_eq(ssl_data, test_data);

    destroy_mock_ssl(mock_ssl);
    destroy_mock_ssl_ctx(mock_ctx);
}

// Test SSL_read wrapper (mocked)
Test(ssl, ssl_read_wrapper) {
    SSL_CTX *mock_ctx = create_mock_ssl_ctx();
    SSL *mock_ssl = create_mock_ssl(mock_ctx);

    const char *test_data = "SSL read test";
    size_t data_len = strlen(test_data);

    set_mock_ssl_recv_data(mock_ssl, test_data, data_len);

    char buffer[256];
    int read = SSL_read(mock_ssl, buffer, data_len);
    cr_assert_eq(read, data_len);
    buffer[data_len] = '\0';
    cr_assert_str_eq(buffer, test_data);

    destroy_mock_ssl(mock_ssl);
    destroy_mock_ssl_ctx(mock_ctx);
}

// Test SSL connection establishment (mocked)
Test(ssl, ssl_connect_success) {
    SSL_CTX *mock_ctx = create_mock_ssl_ctx();
    SSL *mock_ssl = create_mock_ssl(mock_ctx);

    int result = SSL_connect(mock_ssl);
    cr_assert_eq(result, 1); // SSL_SUCCESS

    destroy_mock_ssl(mock_ssl);
    destroy_mock_ssl_ctx(mock_ctx);
}

// Test SSL handshake failure (mocked)
Test(ssl, ssl_connect_failure) {
    SSL_CTX *mock_ctx = create_mock_ssl_ctx();
    SSL *mock_ssl = create_mock_ssl(mock_ctx);

    set_mock_ssl_connect_error(mock_ssl, 0); // Simulate handshake failure

    int result = SSL_connect(mock_ssl);
    cr_assert_neq(result, 1); // Not SSL_SUCCESS

    destroy_mock_ssl(mock_ssl);
    destroy_mock_ssl_ctx(mock_ctx);
}

// Test certificate verification (mocked)
Test(ssl, certificate_verification) {
    SSL_CTX *mock_ctx = create_mock_ssl_ctx();
    SSL *mock_ssl = create_mock_ssl(mock_ctx);

    // Test successful verification
    int verify_result = SSL_get_verify_result(mock_ssl);
    cr_assert_eq(verify_result, X509_V_OK);

    destroy_mock_ssl(mock_ssl);
    destroy_mock_ssl_ctx(mock_ctx);
}

// Test mTLS with client certificate (mocked)
Test(ssl, mutual_tls_client_cert) {
    SSL_CTX *mock_ctx = create_mock_ssl_ctx();

    // Load mock client certificate and key
    int cert_result = SSL_CTX_use_certificate_file(mock_ctx, "mock_client.crt", SSL_FILETYPE_PEM);
    cr_assert_eq(cert_result, 1);

    int key_result = SSL_CTX_use_PrivateKey_file(mock_ctx, "mock_client.key", SSL_FILETYPE_PEM);
    cr_assert_eq(key_result, 1);

    SSL *mock_ssl = create_mock_ssl(mock_ctx);

    // Verify certificate is loaded
    X509 *cert = SSL_get_certificate(mock_ssl);
    cr_assert_not_null(cert);

    destroy_mock_ssl(mock_ssl);
    destroy_mock_ssl_ctx(mock_ctx);
}
