// test_path_validation.c
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "../src/server/server.h" 

bool is_safe_filename(const char *filename) {
    if (!filename || strchr(filename, '/') || strstr(filename, "..")) {
        return false;
    }
    return strlen(filename) > 0 && strlen(filename) < FILENAME_MAX_LEN;
}

void test_safe_filename_valid() {
    assert(is_safe_filename("report.pdf") == true);
    assert(is_safe_filename("file_123.txt") == true);
}

void test_safe_filename_invalid() {
    assert(is_safe_filename("../etc/passwd") == false);
    assert(is_safe_filename("/etc/passwd") == false);
    assert(is_safe_filename("file/evil.txt") == false);
    assert(is_safe_filename("") == false);
    assert(is_safe_filename(NULL) == false);
}

void test_safe_filename_edge() {
    char long_name[FILENAME_MAX_LEN + 10] = {0};
    memset(long_name, 'A', FILENAME_MAX_LEN + 5);
    assert(is_safe_filename(long_name) == false); // слишком длинное
}