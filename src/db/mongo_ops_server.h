// db/mongo_ops.h
#ifndef MONGO_OPS_H
#define MONGO_OPS_H

#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include <stdint.h>

typedef struct {
    char filename[256];
    char extension[32];
    int64_t initial_size;
    int64_t actual_size;
    bson_t *changes;
} file_record_t;

extern mongoc_collection_t *g_collection;
extern mongoc_client_t *g_mongo_client;

bson_t* change_info_to_bson(const char *type, int64_t size_after);
bson_t* file_overseer_to_bson(const file_record_t *file);

// bool mongo_insert(mongoc_collection_t *coll, const char *filename, uint64_t size, const char *mime);

bool mongo_update_or_insert(const char *filename, uint64_t size, const char *mime);
bool mongo_insert(const char *filename, uint64_t size, const char *mime_type);
#endif