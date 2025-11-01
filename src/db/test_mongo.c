//* gcc -o hello_mongoc hello_mongoc.c $(pkg-config --libs --cflags libmongoc-1.0)

#include <mongoc/mongoc.h>

int main(int argc, char *argv[]) {
    const char *uri_string = "mongodb://localhost:27017";
    mongoc_uri_t *uri;
    mongoc_client_t *client;

    mongoc_database_t *database;
    mongoc_collection_t *collection;
    bson_t *command, reply, insert;
    bson_error_t error;
    char *str;
    bool retval;

    /*
    * Required to initialize libmongoc's internals
    */

    mongoc_init();
    /*
    * Optionally get MongoDB URI from command line
    */

    if(argc > 1) {
        uri_string = argv[1];
    }

    uri = mongoc_uri_new_with_error(uri_string, &error);
    if(!uri) {
        fprintf(stderr, "failed to parse URI: %s\")
    }
}