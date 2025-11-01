#include <mongoc/mongoc.h>

#include <stdio.h>
#include <stdlib.h>

#define DATABASE_NAME "file_exchange"
#define DATABASE_COLL "file_groups"
#define DATABASE_CONNECTION "localhost:"

void find_files_by_filename(mongoc_client_t *client, const char *filename) {

    mongoc_database_t *db;

    mongoc_collection_t *collection;

    bson_t *query;

    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_error_t error;
    char *str;

    db = mongoc_client_get_database(client, DATABASE_NAME);

    collection = mongoc_database_get_collection(db, DATABASE_COLL);

    query = BCON_NEW("filename", BCON_UTF8(filename));

    cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL, &error);

    while ()

}