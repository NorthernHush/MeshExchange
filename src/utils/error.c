// error callback

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "../lib/error.h"

error_status_t status(error_status_t status) {

    switch(status) {

        case ERROR_MEMORY:
            printf("[ERROR]: error with memory");
            break;
        case ERROR_SERVER_REQUEST:
            printf("[ERROR]: error server request! :ccc");
            break;
        case ERROR_CLIENT_CONNECTED:
            printf("[ERROR]: client connected error :(");
            break;
        case SUCCESS:
            printf("[SUCCESS]: success:D");
            break;
        case MR_ERROR_CRYPTO:
            printf("[ERROR]: crypto operation failed");
            break;
        case MR_ERROR_MEMORY:
            printf("[ERROR]: memory allocation failed");
            break;
        case MR_ERROR_INVALID_PARAM:
            printf("[ERROR]: invalid parameters");
            break;
        case MR_ERROR_INTEGRITY:
            printf("[ERROR]: integrity check failed");
            break;
        default:
            printf("[ERROR]: unknown error");
            break;
    }

    return status;  // Return the status to fix -Werror=return-type
}
/*
int main() {
    status(SUCCESS);
}
    */

