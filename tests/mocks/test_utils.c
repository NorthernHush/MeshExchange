#include <criterion/criterion.h>
#include <criterion/internal/test.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../../src/db/mongo_ops.h"

const char *extract_filename_from_path(const char *fullpath) {
    
    const char *last_slash = strrchr(fullpath, '/');
    return last_slash ? last_slash + 1 : fullpath;

}

const char* get_mime_type_from_text(const char* filename) {

    const char* dot = strrchr(filename, '.');

    if (!dot || dot == filename) return "application/octet-stream";

    const char *extension = dot + 1;

    char* lower_extension = strdup(extension);

    if (lower_extension == NULL) {

        perror("error in memory");
        return "application/octet-stream";
    }

    for (int i = 0; i < lower_extension[i]; i++) {

        lower_extension[i] = tolower((unsigned char)lower_extension[i]);

    }

    if (strcmp(lower_extension, "pdf") == 0) {

        return "application/pdf";

    } else if (strcmp(lower_extension, "txt") == 0) {
        return "text/plain";
    } else if (strcmp(lower_extension, "html") == 0 || strcmp(lower_extension, "htm") == 0) {
        return "text/html";
    } else if (strcmp(lower_extension, "jpg") == 0 || strcmp(lower_extension, "jpeg") == 0) {
        return "image/jpeg";

    } else if (strcmp(lower_extension, "png") == 0 ) {
        return "image/png";
    } else { 
        return "application/octet-stream";
    }
}

Test(utils, extract_filename) {
    const char *path1 = "/home/user/docks/report.pdf";
    const char *path2 = "simple.txt";
    const char *path3 = "/no/extension";

    cr_assert_str_eq(extract_filename_from_path(path1), "report.pdf");
    cr_assert_str_eq(extract_filename_from_path(path2), "simple.txt");
    cr_assert_str_eq(extract_filename_from_path(path3), "extension");

}

Test(utils, mime_type) {
    cr_assert_str_eq(get_mime_type_from_text("file.pdf"), "application/pdf");
    cr_assert_str_eq(get_mime_type_from_text("image.jpg"), "image/jpeg");
    cr_assert_str_eq(get_mime_type_from_text("unknown.xyz"), "application/octet-stream");
}