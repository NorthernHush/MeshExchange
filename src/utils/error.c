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
    }

}
/*
int main() {
    status(SUCCESS);
}
    */

