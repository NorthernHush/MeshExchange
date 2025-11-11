#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "log_crypto.h"

// Simple crypto logger - in production this would integrate with your main logging system
static void crypto_logger(log_crypto_level_t level, crypto_op_code_t code, const char *ctx) {
    const char *level_str[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
    const char *op_str[] = {"", "ENCRYPT", "DECRYPT", "KEY_GEN", "TAG_VERIFY"};

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    // Only log errors and warnings to avoid leaking sensitive info in debug logs
    if (level >= LOG_CRYPTO_WARNING) {
        fprintf(stderr, "[%s] [CRYPTO-%s] %s: %s\n", timestamp, level_str[level], op_str[code], ctx ? ctx : "N/A");
    }
}

void log_crypto_event(log_crypto_level_t level, crypto_op_code_t code, const char *ctx) {
    crypto_logger(level, code, ctx);
}
