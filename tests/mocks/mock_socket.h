#ifndef MOCK_SOCKET_H
#define MOCK_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

// Mock socket structure
typedef struct {
    int socket_fd;
    char *send_buffer;
    size_t send_buffer_size;
    char *recv_buffer;
    size_t recv_buffer_size;
    size_t recv_offset;
    bool partial_writes;
    bool partial_reads;
    int error_code;
    bool connected;
} mock_socket_t;

// Create mock socket
int create_mock_socket(void);

// Destroy mock socket
void destroy_mock_socket(int sock);

// Set data to be received on next read
void set_mock_socket_recv_data(int sock, const char *data, size_t len);

// Get data that was sent
char *get_mock_socket_data(int sock);

// Set mock to simulate partial writes
void set_mock_socket_partial_writes(int sock, bool partial);

// Set mock to simulate partial reads
void set_mock_socket_partial_reads(int sock, bool partial);

// Set mock to return error on next operation
void set_mock_socket_error(int sock, int error_code);

// Set mock to simulate recv error
void set_mock_socket_recv_error(int sock, int error_code);

// Check if socket is connected
bool is_mock_socket_connected(int sock);

// Send all data (mock implementation)
ssize_t send_all(int sockfd, const void *buf, size_t len);

// Receive all data (mock implementation)
ssize_t recv_all(int sockfd, void *buf, size_t len);

#endif // MOCK_SOCKET_H
