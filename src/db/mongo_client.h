#ifndef MONGO_CLIENT_H
#define MONGO_CLIENT_H

#include <mongoc/mongoc.h>
#include <stdint.h>


//* structure for change info
typedef struct {
    char type_of_changes[256];
    long long size_after;
} ChangeInfo;

//* structure representing a file overseer doc
typedef struct {
    char filename[256];
    char extension[32];
    int64_t initial_size;
    int64_t actual_size;

    bson_t *changes;
} file_record_t;

extern mongoc_client_t *g_mongo_client;
extern mongoc_collection_t *g_collection;

//* initialize function
bson_t* change_info_to_bson(const char *type, int64_t size_after);
bson_t* file_overseer_to_bson(const file_record_t *file);


#endif //* MONGO_CLIENT_H