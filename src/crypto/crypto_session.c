#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>  // For explicit_bzero
#include <sys/prctl.h>  // For prctl
#include "crypto.h"

// Fallback for explicit_bzero if not available
#ifndef explicit_bzero
#define explicit_bzero(b, len) memset(b, 0, len)
#endif

// Create a secure key with locked memory
secure_key_t* secure_key_create(void) {
    // Allocate memory with MAP_LOCKED to prevent swapping
    secure_key_t *key = mmap(NULL, sizeof(secure_key_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);

    if (key == MAP_FAILED) {
        // Fallback to regular malloc if mlock fails
        key = calloc(1, sizeof(secure_key_t));
        if (!key) return NULL;

        // Try to lock the memory
        if (mlock(key, sizeof(secure_key_t)) != 0) {
            // If mlock fails, continue anyway - better than failing completely
        }
    }

    // Prevent core dumps from including this memory
    if (prctl(PR_SET_DUMPABLE, 0) != 0) {
        // prctl failed, but continue
    }

    key->initialized = 0;
    return key;
}

// Destroy a secure key
void secure_key_destroy(secure_key_t *key) {
    if (!key) return;

    // Zero out the key
    explicit_bzero((void*)key->key, sizeof(key->key));
    key->initialized = 0;

    // If it was mmap'd, munmap it
    if (munmap(key, sizeof(secure_key_t)) == 0) {
        return;
    }

    // Otherwise it was malloc'd, free it
    free(key);
}

// Set the key data
crypto_status_t secure_key_set(secure_key_t *key, const uint8_t *data, size_t len) {
    if (!key || !data || len != 32) {
        return CRYPTO_ERR_INVALID_INPUT;
    }

    memcpy((void*)key->key, data, len);
    key->initialized = 1;
    return CRYPTO_OK;
}

// Create a crypto session
crypto_session_t* crypto_session_create(secure_key_t *key) {
    if (!key || !key->initialized) {
        return NULL;
    }

    crypto_session_t *session = calloc(1, sizeof(crypto_session_t));
    if (!session) return NULL;

    session->key = key;
    return session;
}

// Destroy a crypto session
void crypto_session_destroy(crypto_session_t *session) {
    if (!session) return;
    // Note: we don't destroy the key here, as it might be shared
    free(session);
}
