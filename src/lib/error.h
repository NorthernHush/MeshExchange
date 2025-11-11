#ifndef ERROR_H
#define ERROR_H

typedef enum {
    // Existing errors
    ERROR_MEMORY,
    ERROR_SERVER_REQUEST,
    ERROR_CLIENT_CONNECTED,
    SUCCESS,

    // New crypto-specific errors
    MR_SUCCESS = SUCCESS,  // Alias for compatibility
    MR_ERROR_CRYPTO,       // General crypto error
    MR_ERROR_MEMORY,       // Memory allocation failure
    MR_ERROR_INVALID_PARAM, // Invalid input parameters
    MR_ERROR_INTEGRITY,    // Integrity check failed (e.g., tag mismatch)

} error_status_t;

#endif // ERROR_H
