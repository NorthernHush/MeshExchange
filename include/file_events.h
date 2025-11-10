#ifndef FILE_EVENTS_H
#define FILE_EVENTS_H

#include <mongoc/mongoc.h>
#include <stdbool.h>

extern mongoc_client_t *g_mongo_client; 

bool append_proc_event(const char *file_id, const char *change_type, const char *status);
char* get_next_proc_key(const char *file_id);
bool create_base_document(const char *fullpath);
char* get_filename_without_extension(const char* full_filename);
char* get_file_extension(const char *full_path);

#endif