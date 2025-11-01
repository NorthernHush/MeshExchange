//* gcc -o mongo_client mongo_client.c $(pkg-config --cflags --libs libmongoc-1.0)

#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//* headers

#include "mongo_client.h"

mongoc_collection_t *g_collection = NULL;

//* bson change info
bson_t* change_info_to_bson(const char *type, int64_t size_after) {
    bson_t* doc = bson_new();
    BSON_APPEND_UTF8(doc, "type_of_changes", type);
    BSON_APPEND_INT64(doc, "size_after", size_after);
    return doc;
}

bson_t* file_overseer_to_bson(const file_record_t *file)
{
    bson_t *doc = bson_new();
    BSON_APPEND_UTF8(doc, "filename", file->filename);
    BSON_APPEND_UTF8(doc, "extension", file->extension);
    BSON_APPEND_INT64(doc, "initial_size", file->initial_size);
    BSON_APPEND_INT64(doc, "actual_size", file->actual_size);
    if (file->changes) {
        BSON_APPEND_DOCUMENT(doc, "changes", file->changes);
    }
    return doc;
}

int main(void) {
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    bson_t *doc;
    bson_error_t error;

    mongoc_init();

    uri = mongoc_uri_new_with_error("mongodb://localhost:27017/?appname=file-overseer", &error);
    if (!uri) {
        fprintf(stderr, "Failed to parse URI: %s\n", error.message);
        mongoc_cleanup();
        return EXIT_FAILURE;
    }

    client = mongoc_client_new_from_uri(uri);
    if (!client) {
        fprintf(stderr, "Failed to create MongoDB client\n");
        mongoc_uri_destroy(uri);
        mongoc_cleanup();
        return EXIT_FAILURE;
    }

    collection = mongoc_client_get_collection(client, "mydatabase", "file_overseer");

    file_record_t my_file = {0};
    strncpy(my_file.filename, "example.txt", sizeof(my_file.filename) - 1);
    my_file.filename[sizeof(my_file.filename) - 1] = '\0';

    strncpy(my_file.extension, "txt", sizeof(my_file.extension) - 1);
    my_file.extension[sizeof(my_file.extension) - 1] = '\0';

    my_file.initial_size = 1024;
    my_file.actual_size = 2048;
    my_file.changes = change_info_to_bson("modified", 2048);

    doc = file_overseer_to_bson(&my_file);

    if(!doc) {
        fprintf(stderr, "Error memory! \n");
        exit(EXIT_FAILURE);
    }

    if (!mongoc_collection_insert_one(collection, doc, NULL, NULL, &error)) {
        fprintf(stderr, "Insert failed: %s\n", error.message);
    } else {
        printf("Document inserted successfully.\n");
    }

    bson_destroy(doc);
    bson_destroy(my_file.changes);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_uri_destroy(uri);
    mongoc_cleanup();

    return EXIT_SUCCESS;
}
