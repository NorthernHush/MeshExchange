#ifndef MOCK_SSL_H
#define MOCK_SSL_H

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <stdbool.h>

// Mock SSL context structure
typedef struct {
    SSL_CTX *real_ctx;
    bool should_fail;
    char *cert_file;
    char *key_file;
} mock_ssl_ctx_t;

// Mock SSL connection structure
typedef struct {
    SSL *real_ssl;
    mock_ssl_ctx_t *mock_ctx;
    char *send_buffer;
    size_t send_buffer_size;
    char *recv_buffer;
    size_t recv_buffer_size;
    size_t recv_offset;
    int connect_result;
    int verify_result;
    X509 *mock_cert;
} mock_ssl_t;

// Create mock SSL context
SSL_CTX *create_mock_ssl_ctx(void);

// Destroy mock SSL context
void destroy_mock_ssl_ctx(SSL_CTX *ctx);

// Create mock SSL connection
SSL *create_mock_ssl(SSL_CTX *ctx);

// Destroy mock SSL connection
void destroy_mock_ssl(SSL *ssl);

// Set mock SSL to fail connect
void set_mock_ssl_connect_error(SSL *ssl, int result);

// Set data to be received on next SSL_read
void set_mock_ssl_recv_data(SSL *ssl, const char *data, size_t len);

// Get data that was sent via SSL_write
char *get_mock_ssl_data(SSL *ssl);

// Set mock certificate for SSL connection
void set_mock_ssl_certificate(SSL *ssl, const char *cert_pem);

// Load mock certificate file
int mock_ssl_ctx_use_certificate_file(SSL_CTX *ctx, const char *file, int type);

// Load mock private key file
int mock_ssl_ctx_use_private_key_file(SSL_CTX *ctx, const char *file, int type);

// Mock SSL_write function
int mock_ssl_write(SSL *ssl, const void *buf, int num);

// Mock SSL_read function
int mock_ssl_read(SSL *ssl, void *buf, int num);

// Mock SSL_connect function
int mock_ssl_connect(SSL *ssl);

// Mock SSL_get_verify_result function
long mock_ssl_get_verify_result(SSL *ssl);

// Mock SSL_get_certificate function
X509 *mock_ssl_get_certificate(SSL *ssl);

#endif // MOCK_SSL_H
