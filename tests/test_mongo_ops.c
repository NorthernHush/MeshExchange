#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <criterion/criterion.h>

#include <mongoc/mongoc.h>
#include <bson/bson.h>

#include "../src/db/mongo_ops.h"
#include "../tests/mocks/mock_mongo.h"

// Test change_info_to_bson function
Test(mongo_ops, change_info_to_bson_basic) {
    const char *change_type = "modified";
    int64_t size_after = 1024;

    bson_t *doc = change_info_to_bson(change_type, size_after);
    cr_assert_not_null(doc);

    // Verify the document contains expected fields
    bson_iter_t iter;
    bson_iter_init(&iter, doc);

    cr_assert(bson_iter_find(&iter, "type_of_changes"));
    cr_assert_str_eq(bson_iter_utf8(&iter, NULL), change_type);

    cr_assert(bson_iter_find(&iter, "size_after"));
    cr_assert_eq(bson_iter_int64(&iter), size_after);

    bson_destroy(doc);
}

// Test file_overseer_to_bson with all fields
Test(mongo_ops, file_overseer_to_bson_complete) {
    file_record_t file = {0};
    strcpy(file.filename, "test.txt");
    strcpy(file.extension, "txt");
    file.initial_size = 512;
    file.actual_size = 1024;
    file.changes = change_info_to_bson("created", 1024);

    bson_t *doc = file_overseer_to_bson(&file);
    cr_assert_not_null(doc);

    bson_iter_t iter;
    bson_iter_init(&iter, doc);

    cr_assert(bson_iter_find(&iter, "filename"));
    cr_assert_str_eq(bson_iter_utf8(&iter, NULL), "test.txt");

    cr_assert(bson_iter_find(&iter, "extension"));
    cr_assert_str_eq(bson_iter_utf8(&iter, NULL), "txt");

    cr_assert(bson_iter_find(&iter, "initial_size"));
    cr_assert_eq(bson_iter_int64(&iter), 512);

    cr_assert(bson_iter_find(&iter, "actual_size"));
    cr_assert_eq(bson_iter_int64(&iter), 1024);

    cr_assert(bson_iter_find(&iter, "changes"));
    // Changes is a subdocument, we could verify its contents too

    bson_destroy(file.changes);
    bson_destroy(doc);
}

// Test file_overseer_to_bson without changes
Test(mongo_ops, file_overseer_to_bson_no_changes) {
    file_record_t file = {0};
    strcpy(file.filename, "no_changes.txt");
    strcpy(file.extension, "txt");
    file.initial_size = 256;
    file.actual_size = 256;
    file.changes = NULL;

    bson_t *doc = file_overseer_to_bson(&file);
    cr_assert_not_null(doc);

    bson_iter_t iter;
    bson_iter_init(&iter, doc);

    cr_assert(bson_iter_find(&iter, "filename"));
    cr_assert_str_eq(bson_iter_utf8(&iter, NULL), "no_changes.txt");

    // Should not have changes field
    cr_assert(!bson_iter_find(&iter, "changes"));

    bson_destroy(doc);
}

// Test mongo_insert with mock collection
Test(mongo_ops, mongo_insert_success) {
    // Setup mock
    mongoc_collection_t *mock_coll = create_mock_collection();
    cr_assert_not_null(mock_coll);

    // Test insert
    bool result = mongo_insert(mock_coll, "test_file.txt", 1024, "text/plain");
    cr_assert(result, "Insert should succeed with mock collection");

    // Verify the document was "inserted"
    bson_t *expected_doc = BCON_NEW(
        "filename", BCON_UTF8("test_file.txt"),
        "size", BCON_INT64(1024),
        "mime_type", BCON_UTF8("text/plain"),
        "deleted", BCON_BOOL(false),
        "created_at", BCON_DATE_TIME(0)  // Mock time
    );

    // In a real test, we'd verify the mock received the correct document
    // For now, just check that the function didn't crash

    bson_destroy(expected_doc);
    destroy_mock_collection(mock_coll);
}

// Test mongo_insert with NULL collection
Test(mongo_ops, mongo_insert_null_collection) {
    bool result = mongo_insert(NULL, "test.txt", 100, "text/plain");
    cr_assert_not(result, "Insert should fail with NULL collection");
}

// Test mongo_insert with NULL filename
Test(mongo_ops, mongo_insert_null_filename) {
    mongoc_collection_t *mock_coll = create_mock_collection();
    bool result = mongo_insert(mock_coll, NULL, 100, "text/plain");
    cr_assert_not(result, "Insert should fail with NULL filename");

    destroy_mock_collection(mock_coll);
}

// Test mongo_update_or_insert (requires global collection to be set)
// This would need more complex mocking for the global g_collection
Test(mongo_ops, mongo_update_or_insert_basic) {
    // This test would require setting up g_collection
    // For now, skip or use dependency injection pattern
    cr_skip_test("Requires global collection setup");
}

// Test BSON creation with edge cases
Test(mongo_ops, change_info_to_bson_edge_cases) {
    // Empty change type
    bson_t *doc1 = change_info_to_bson("", 0);
    cr_assert_not_null(doc1);
    bson_destroy(doc1);

    // Large size
    bson_t *doc2 = change_info_to_bson("resized", INT64_MAX);
    cr_assert_not_null(doc2);
    bson_destroy(doc2);

    // Negative size (though unusual, should still work)
    bson_t *doc3 = change_info_to_bson("truncated", -100);
    cr_assert_not_null(doc3);
    bson_destroy(doc3);
}
