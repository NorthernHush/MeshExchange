#ifndef MOCK_MONGO_H
#define MOCK_MONGO_H

#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <stdbool.h>

// Mock collection structure
typedef struct {
    mongoc_collection_t *real_collection;
    bson_t *inserted_documents;
    int insert_count;
    bool should_fail;
} mock_mongo_collection_t;

// Mock client structure
typedef struct {
    mongoc_client_t *real_client;
    mock_mongo_collection_t *mock_collection;
} mock_mongo_client_t;

// Create mock collection
mongoc_collection_t *create_mock_collection(void);

// Destroy mock collection
void destroy_mock_collection(mongoc_collection_t *collection);

// Set mock to fail operations
void set_mock_collection_failure(mongoc_collection_t *collection, bool fail);

// Get inserted documents for verification
bson_t *get_mock_inserted_documents(mongoc_collection_t *collection, int *count);

// Create mock client
mongoc_client_t *create_mock_client(void);

// Destroy mock client
void destroy_mock_client(mongoc_client_t *client);

// Get collection from mock client
mongoc_collection_t *mock_client_get_collection(mongoc_client_t *client, const char *db, const char *collection);

#endif // MOCK_MONGO_H
