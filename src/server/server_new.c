/**
 * Secure File Exchange Server - Event-Driven Architecture
 * Features: libevent, XChaCha20-Poly1305, ECDH, Tor integration, DoS protection
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <glib.h>
#include <mongoc/mongoc.h>
#include <sodium.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define BLAKE3_IMPLEMENTATION
#include "blake3.h"

#include "../../include/protocol.h"
#include "../crypto/crypto_session.h"
#include "../db/mongo_ops_server.h"
#include "admin_panel.h"

// Server configuration
#define DEFAULT_PORT 1512
#define MAX_CONNECTIONS 10000
#define CONNECTION_TIMEOUT 300 // 5 minutes
#define STORAGE_DIR "filetrade"
#define LOG_FILE "/tmp/secure-file-server.log"
#define MONGODB_URI "mongodb://localhost:27017"
#define DATABASE_NAME "file_exchange"
#define COLLECTION_NAME "file_groups"
#define MAX_FILE_SIZE (1024LL * 1024LL * 1024LL) // 1GB

// Connection state
typedef enum {
    CONN_STATE_DISCONNECTED,
    CONN_STATE_ECDH_INIT,
    CONN_STATE_ECDH_RESPONSE,
    CONN_STATE_SESSION_KEY,
    CONN_STATE_AUTHENTICATED,
    CONN_STATE_TRANSFERRING
} connection_state_t;

// Connection context
typedef struct {
    struct bufferevent *bev;
    struct event_base *base;
    crypto_session_t crypto_session;
    char client_ip[INET_ADDRSTRLEN];
    char fingerprint[FINGERPRINT_LEN];
    char session_key_hex[65]; // Hex-encoded session key for admin panel
    time_t connected_at;
    connection_state_t state;
    GHashTable *pending_data; // For partial transfers
} connection_t;

// Rate limiting
typedef struct {
    uint32_t ip_address;
    uint32_t request_count;
    time_t window_start;
} rate_limit_t;

// Global state
static struct event_base *g_event_base = NULL;
static SSL_CTX *g_ssl_ctx = NULL;
static GHashTable *g_connections = NULL;
static GHashTable *g_rate_limits = NULL;
static volatile sig_atomic_t g_shutdown = 0;

// MongoDB globals are defined in mongo_ops_server.c
extern mongoc_client_t *g_mongo_client;
extern mongoc_collection_t *g_collection;

// Logging
static void secure_log(const char *level, const char *format, ...) {
    time_t now = time(NULL);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    va_list args;
    va_start(args, format);
    fprintf(stderr, "[%s] [%s] ", timestamp, level);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// Rate limiting functions
static int check_rate_limit(const char *ip) {
    uint32_t ip_addr = inet_addr(ip);
    rate_limit_t *limit = g_hash_table_lookup(g_rate_limits, &ip_addr);

    time_t now = time(NULL);

    if (!limit) {
        limit = calloc(1, sizeof(rate_limit_t));
        limit->ip_address = ip_addr;
        limit->window_start = now;
        g_hash_table_insert(g_rate_limits, &limit->ip_address, limit);
    }

    // Reset window if expired
    if (now - limit->window_start >= RATE_LIMIT_WINDOW_SEC) {
        limit->request_count = 0;
        limit->window_start = now;
    }

    if (limit->request_count >= MAX_REQUESTS_PER_WINDOW) {
        return 1; // Rate limited
    }

    limit->request_count++;
    return 0; // OK
}

// Connection count per IP
static int check_connection_limit(const char *ip) {
    uint32_t ip_addr = inet_addr(ip);
    int count = 0;

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, g_connections);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        connection_t *conn = value;
        if (inet_addr(conn->client_ip) == ip_addr) {
            count++;
        }
    }

    return count >= MAX_CONNECTIONS_PER_IP;
}

// SSL context initialization
static SSL_CTX *init_ssl_context(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        secure_log("ERROR", "Failed to create SSL context");
        return NULL;
    }

    // Load certificates
    if (SSL_CTX_use_certificate_file(ctx, "src/server-cert.pem", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "src/server-key.pem", SSL_FILETYPE_PEM) <= 0) {
        secure_log("ERROR", "Failed to load SSL certificates");
        SSL_CTX_free(ctx);
        return NULL;
    }

    // Load CA certificate for client verification
    if (SSL_CTX_load_verify_locations(ctx, "src/ca.pem", NULL) <= 0) {
        secure_log("ERROR", "Failed to load CA certificate");
        SSL_CTX_free(ctx);
        return NULL;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    return ctx;
}

// MongoDB initialization
static int init_mongodb(void) {
    mongoc_init();
    g_mongo_client = mongoc_client_new(MONGODB_URI);
    if (!g_mongo_client) {
        secure_log("ERROR", "Failed to connect to MongoDB");
        return -1;
    }

    bson_error_t error;
    if (!mongoc_client_command_simple(g_mongo_client, "admin",
                                    BCON_NEW("ping", BCON_INT32(1)), NULL, NULL, &error)) {
        secure_log("ERROR", "MongoDB ping failed: %s", error.message);
        return -1;
    }

    g_collection = mongoc_client_get_collection(g_mongo_client, DATABASE_NAME, COLLECTION_NAME);
    return 0;
}

// Handle ECDH key exchange initiation
static void handle_ecdh_init(connection_t *conn, const ECDHInitPacket *packet) {
    // Generate our ECDH keys
    if (crypto_session_init(&conn->crypto_session) != 0) {
        secure_log("ERROR", "Failed to initialize crypto session for %s", conn->client_ip);
        // Send error response
        return;
    }

    // Store peer's public key and nonce
    memcpy(conn->crypto_session.peer_public_key, packet->public_key, ECDH_PUBLIC_KEY_LEN);

    // Compute shared secret
    if (crypto_session_compute_shared_secret(&conn->crypto_session) != 0 ||
        crypto_session_derive_session_key(&conn->crypto_session) != 0) {
        secure_log("ERROR", "Failed ECDH computation for %s", conn->client_ip);
        return;
    }

    // Send ECDH response
    ECDHResponsePacket response;
    memcpy(response.public_key, conn->crypto_session.public_key, ECDH_PUBLIC_KEY_LEN);

    // Encrypt some initial metadata (empty for now)
    EncryptedMetadata empty_metadata = {0};
    memcpy(empty_metadata.nonce, packet->nonce, XCHACHA20_NONCE_LEN);

    if (crypto_session_encrypt_metadata(&conn->crypto_session, "", 0, "",
                                      &empty_metadata) != 0) {
        secure_log("ERROR", "Failed to encrypt metadata for %s", conn->client_ip);
        return;
    }

    memcpy(response.encrypted_metadata, &empty_metadata.encrypted_filename,
           ENCRYPTED_METADATA_MAX_LEN);
    memcpy(response.auth_tag, empty_metadata.filename_auth_tag, 16);

    // Send response
    bufferevent_write(conn->bev, &response, sizeof(response));

    // Move to next state
    conn->state = CONN_STATE_ECDH_RESPONSE;
}

// Handle session key establishment
static void handle_session_key(connection_t *conn, const SessionKeyPacket *packet) {
    // Verify key hash
    uint8_t computed_hash[BLAKE3_HASH_LEN];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, packet->session_key, SESSION_KEY_LEN);
    blake3_hasher_finalize(&hasher, computed_hash, BLAKE3_HASH_LEN);

    if (memcmp(computed_hash, packet->key_hash, BLAKE3_HASH_LEN) != 0) {
        secure_log("ERROR", "Session key verification failed for %s", conn->client_ip);
        return;
    }

    // Check if client is banned
    char session_key_hex[65];
    sodium_bin2hex(session_key_hex, sizeof(session_key_hex),
                   packet->session_key, SESSION_KEY_LEN);

    if (admin_is_client_banned(session_key_hex)) {
        secure_log("WARNING", "Banned client attempted connection: %s", conn->client_ip);

        // Send ban message
        const char *ban_message = admin_get_ban_message(session_key_hex);
        ResponseHeader resp = { .status = RESP_BANNED };
        bufferevent_write(conn->bev, &resp, sizeof(resp));

        // Send ban message if available
        if (ban_message) {
            bufferevent_write(conn->bev, ban_message, strlen(ban_message));
        }

        return;
    }

    // Store session key for admin panel
    strcpy(conn->session_key_hex, session_key_hex);

    // Session established
    conn->state = CONN_STATE_AUTHENTICATED;
    secure_log("INFO", "Session established for %s (key: %.16s...)", conn->client_ip, session_key_hex);

    // Send success response
    ResponseHeader resp = { .status = RESP_SUCCESS };
    bufferevent_write(conn->bev, &resp, sizeof(resp));
}

// Handle file upload
static void handle_upload(connection_t *conn, const RequestHeader *req) {
    if (conn->state != CONN_STATE_AUTHENTICATED) {
        ResponseHeader resp = { .status = RESP_AUTH_FAILED };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Decrypt metadata
    char filename[FILENAME_MAX_LEN];
    long long filesize;
    char recipient[FINGERPRINT_LEN];

    if (crypto_session_decrypt_metadata(&conn->crypto_session, &req->metadata,
                                       filename, &filesize, recipient) != 0) {
        secure_log("ERROR", "Failed to decrypt metadata for upload from %s", conn->client_ip);
        ResponseHeader resp = { .status = RESP_ENCRYPTION_ERROR };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Validate filename and size
    if (strstr(filename, "..") || strchr(filename, '/') || strlen(filename) == 0 ||
        filesize <= 0 || filesize > MAX_FILE_SIZE) {
        ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Create storage directory if needed
    if (mkdir(STORAGE_DIR, 0755) == -1 && errno != EEXIST) {
        secure_log("ERROR", "Failed to create storage directory");
        ResponseHeader resp = { .status = RESP_ERROR };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Create full path
    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", STORAGE_DIR, filename);

    // Check if file already exists
    if (access(filepath, F_OK) == 0) {
        ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Open file for writing
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        secure_log("ERROR", "Failed to open file %s for writing", filepath);
        ResponseHeader resp = { .status = RESP_ERROR };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Send OK to start transfer
    ResponseHeader resp = { .status = RESP_SUCCESS };
    bufferevent_write(conn->bev, &resp, sizeof(resp));

    // Set connection to transferring state
    conn->state = CONN_STATE_TRANSFERRING;

    // Store file info in pending data
    GHashTable *file_info = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(file_info, "fp", fp);
    g_hash_table_insert(file_info, "filesize", (gpointer)filesize);
    g_hash_table_insert(file_info, "received", (gpointer)0LL);
    g_hash_table_insert(file_info, "filename", g_strdup(filename));

    g_hash_table_insert(conn->pending_data, "upload", file_info);

    secure_log("INFO", "Upload initiated: %s (%lld bytes) from %s", filename, filesize, conn->client_ip);
}

// Handle file download
static void handle_download(connection_t *conn, const RequestHeader *req) {
    if (conn->state != CONN_STATE_AUTHENTICATED) {
        ResponseHeader resp = { .status = RESP_AUTH_FAILED };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Decrypt metadata to get filename
    char filename[FILENAME_MAX_LEN];
    long long dummy_filesize;
    char dummy_recipient[FINGERPRINT_LEN];

    if (crypto_session_decrypt_metadata(&conn->crypto_session, &req->metadata,
                                       filename, &dummy_filesize, dummy_recipient) != 0) {
        secure_log("ERROR", "Failed to decrypt metadata for download from %s", conn->client_ip);
        ResponseHeader resp = { .status = RESP_ENCRYPTION_ERROR };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Validate filename
    if (strstr(filename, "..") || strchr(filename, '/') || strlen(filename) == 0) {
        ResponseHeader resp = { .status = RESP_PERMISSION_DENIED };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Create full path
    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", STORAGE_DIR, filename);

    // Check if file exists and get size
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ResponseHeader resp = { .status = RESP_FILE_NOT_FOUND };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    long long filesize = st.st_size;

    // Open file for reading
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        secure_log("ERROR", "Failed to open file %s for reading", filepath);
        ResponseHeader resp = { .status = RESP_ERROR };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Send response with file size
    ResponseHeader resp = { .status = RESP_SUCCESS, .filesize = filesize };
    bufferevent_write(conn->bev, &resp, sizeof(resp));

    // Set connection to transferring state
    conn->state = CONN_STATE_TRANSFERRING;

    // Store file info in pending data
    GHashTable *file_info = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(file_info, "fp", fp);
    g_hash_table_insert(file_info, "filesize", (gpointer)filesize);
    g_hash_table_insert(file_info, "sent", (gpointer)0LL);
    g_hash_table_insert(file_info, "filename", g_strdup(filename));

    g_hash_table_insert(conn->pending_data, "download", file_info);

    secure_log("INFO", "Download initiated: %s (%lld bytes) to %s", filename, filesize, conn->client_ip);
}

// Handle file list
static void handle_list(connection_t *conn, const RequestHeader *req) {
    if (conn->state != CONN_STATE_AUTHENTICATED) {
        ResponseHeader resp = { .status = RESP_AUTH_FAILED };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Open storage directory
    DIR *dir = opendir(STORAGE_DIR);
    if (!dir) {
        secure_log("ERROR", "Failed to open storage directory");
        ResponseHeader resp = { .status = RESP_ERROR };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    // Collect file list
    GString *file_list = g_string_new("");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Regular files only
            struct stat st;
            char filepath[PATH_MAX];
            snprintf(filepath, sizeof(filepath), "%s/%s", STORAGE_DIR, entry->d_name);

            if (stat(filepath, &st) == 0) {
                g_string_append_printf(file_list, "%s\t%lld\n", entry->d_name, (long long)st.st_size);
            }
        }
    }
    closedir(dir);

    long long list_size = file_list->len;

    // Send response with list size
    ResponseHeader resp = { .status = RESP_SUCCESS, .filesize = list_size };
    bufferevent_write(conn->bev, &resp, sizeof(resp));

    // Send file list data
    if (list_size > 0) {
        bufferevent_write(conn->bev, file_list->str, list_size);
    }

    g_string_free(file_list, TRUE);
}

// Main request handler
static void handle_request(connection_t *conn, const RequestHeader *req) {
    // Rate limiting check
    if (check_rate_limit(conn->client_ip)) {
        ResponseHeader resp = { .status = RESP_RATE_LIMITED };
        bufferevent_write(conn->bev, &resp, sizeof(resp));
        return;
    }

    switch (req->command) {
        case CMD_ECDH_INIT:
            handle_ecdh_init(conn, (const ECDHInitPacket*)req);
            break;
        case CMD_SESSION_KEY:
            handle_session_key(conn, (const SessionKeyPacket*)req);
            break;
        case CMD_UPLOAD:
            handle_upload(conn, req);
            break;
        case CMD_DOWNLOAD:
            handle_download(conn, req);
            break;
        case CMD_LIST:
            handle_list(conn, req);
            break;
        case CMD_PING:
            // Keep-alive
            ResponseHeader resp = { .status = RESP_SUCCESS };
            bufferevent_write(conn->bev, &resp, sizeof(resp));
            break;
        default: {
            ResponseHeader resp = { .status = RESP_UNKNOWN_COMMAND };
            bufferevent_write(conn->bev, &resp, sizeof(resp));
            break;
        }
    }
}

// Buffer event callbacks
static void read_cb(struct bufferevent *bev, void *ctx) {
    connection_t *conn = ctx;
    struct evbuffer *input = bufferevent_get_input(bev);

    size_t len = evbuffer_get_length(input);

    // Handle different packet types based on state
    switch (conn->state) {
        case CONN_STATE_ECDH_INIT: {
            if (len < sizeof(ECDHInitPacket)) return;
            ECDHInitPacket packet;
            if (evbuffer_remove(input, &packet, sizeof(ECDHInitPacket)) != sizeof(ECDHInitPacket)) {
                secure_log("ERROR", "Failed to read ECDH init packet from %s", conn->client_ip);
                return;
            }
            handle_ecdh_init(conn, &packet);
            break;
        }
        case CONN_STATE_ECDH_RESPONSE: {
            if (len < sizeof(SessionKeyPacket)) return;
            SessionKeyPacket packet;
            if (evbuffer_remove(input, &packet, sizeof(SessionKeyPacket)) != sizeof(SessionKeyPacket)) {
                secure_log("ERROR", "Failed to read session key packet from %s", conn->client_ip);
                return;
            }
            handle_session_key(conn, &packet);
            break;
        }
        case CONN_STATE_AUTHENTICATED: {
            if (len < sizeof(RequestHeader)) return;
            RequestHeader req;
            if (evbuffer_remove(input, &req, sizeof(RequestHeader)) != sizeof(RequestHeader)) {
                secure_log("ERROR", "Failed to read request header from %s", conn->client_ip);
                return;
            }
            handle_request(conn, &req);
            break;
        }
        case CONN_STATE_TRANSFERRING: {
            // Check if this is upload or download
            GHashTable *upload_info = g_hash_table_lookup(conn->pending_data, "upload");
            GHashTable *download_info = g_hash_table_lookup(conn->pending_data, "download");

            if (upload_info) {
                // Handle upload data
                FILE *fp = g_hash_table_lookup(upload_info, "fp");
                long long filesize = (long long)g_hash_table_lookup(upload_info, "filesize");
                long long received = (long long)g_hash_table_lookup(upload_info, "received");

                if (!fp) {
                    secure_log("ERROR", "No file pointer for upload from %s", conn->client_ip);
                    evbuffer_drain(input, len);
                    return;
                }

                // Read available data
                size_t to_read = len;
                if (received + to_read > filesize) {
                    to_read = filesize - received;
                }

                if (to_read > 0) {
                    char buffer[BUFFER_SIZE];
                    size_t actually_read = evbuffer_remove(input, buffer, to_read);
                    size_t written = fwrite(buffer, 1, actually_read, fp);
                    if (written != actually_read) {
                        secure_log("ERROR", "Failed to write file data for %s", conn->client_ip);
                        fclose(fp);
                        g_hash_table_remove(conn->pending_data, "upload");
                        conn->state = CONN_STATE_AUTHENTICATED;
                        return;
                    }
                    received += written;
                    g_hash_table_insert(upload_info, "received", (gpointer)received);
                }

                // Check if upload complete
                if (received >= filesize) {
                    fclose(fp);
                    char *filename = g_hash_table_lookup(upload_info, "filename");
                    secure_log("INFO", "Upload completed: %s (%lld bytes) from %s", filename, filesize, conn->client_ip);

                    // Send completion response
                    ResponseHeader resp = { .status = RESP_SUCCESS };
                    bufferevent_write(conn->bev, &resp, sizeof(resp));

                    g_hash_table_remove(conn->pending_data, "upload");
                    conn->state = CONN_STATE_AUTHENTICATED;
                }
            } else if (download_info) {
                // Handle download - send file data
                FILE *fp = g_hash_table_lookup(download_info, "fp");
                long long filesize = (long long)g_hash_table_lookup(download_info, "filesize");
                long long sent = (long long)g_hash_table_lookup(download_info, "sent");

                if (!fp) {
                    secure_log("ERROR", "No file pointer for download to %s", conn->client_ip);
                    g_hash_table_remove(conn->pending_data, "download");
                    conn->state = CONN_STATE_AUTHENTICATED;
                    return;
                }

                // Send file data in chunks
                size_t to_send = BUFFER_SIZE;
                if (sent + to_send > filesize) {
                    to_send = filesize - sent;
                }

                if (to_send > 0) {
                    char buffer[BUFFER_SIZE];
                    size_t read_bytes = fread(buffer, 1, to_send, fp);
                    if (read_bytes > 0) {
                        bufferevent_write(conn->bev, buffer, read_bytes);
                        sent += read_bytes;
                        g_hash_table_insert(download_info, "sent", (gpointer)sent);
                    } else {
                        secure_log("ERROR", "Failed to read file data for %s", conn->client_ip);
                        fclose(fp);
                        g_hash_table_remove(conn->pending_data, "download");
                        conn->state = CONN_STATE_AUTHENTICATED;
                        return;
                    }
                }

                // Check if download complete
                if (sent >= filesize) {
                    fclose(fp);
                    char *filename = g_hash_table_lookup(download_info, "filename");
                    secure_log("INFO", "Download completed: %s (%lld bytes) to %s", filename, filesize, conn->client_ip);

                    g_hash_table_remove(conn->pending_data, "download");
                    conn->state = CONN_STATE_AUTHENTICATED;
                }
            } else {
                secure_log("ERROR", "No transfer info found for transferring connection %s", conn->client_ip);
                evbuffer_drain(input, len);
            }
            break;
        }
        default:
            // Drain buffer to prevent accumulation
            evbuffer_drain(input, len);
            break;
    }
}

static void event_cb(struct bufferevent *bev, short events, void *ctx) {
    connection_t *conn = ctx;

    if (events & BEV_EVENT_EOF) {
        secure_log("INFO", "Connection closed by %s", conn->client_ip);
    } else if (events & BEV_EVENT_ERROR) {
        secure_log("ERROR", "Connection error with %s: %s",
                  conn->client_ip, evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
    } else if (events & BEV_EVENT_TIMEOUT) {
        secure_log("INFO", "Connection timeout for %s", conn->client_ip);
    }

    // Cleanup connection
    crypto_session_cleanup(&conn->crypto_session);
    if (conn->pending_data) {
        g_hash_table_destroy(conn->pending_data);
    }
    bufferevent_free(bev);
    g_hash_table_remove(g_connections, conn);
    free(conn);
}

// Accept callback
static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                     struct sockaddr *sa, int socklen, void *ctx) {
    struct event_base *base = ctx;
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

    // Check connection limits
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));

    if (check_connection_limit(ip)) {
        close(fd);
        secure_log("WARNING", "Connection limit exceeded for %s", ip);
        return;
    }

    // Create SSL bufferevent
    struct bufferevent *bev = bufferevent_openssl_socket_new(
        base, fd, SSL_new(g_ssl_ctx), BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);

    if (!bev) {
        close(fd);
        secure_log("ERROR", "Failed to create bufferevent for %s", ip);
        return;
    }

    // Create connection context
    connection_t *conn = calloc(1, sizeof(connection_t));
    if (!conn) {
        bufferevent_free(bev);
        secure_log("ERROR", "Failed to allocate connection context for %s", ip);
        return;
    }

    conn->bev = bev;
    conn->base = base;
    strcpy(conn->client_ip, ip);
    conn->connected_at = time(NULL);
    conn->state = CONN_STATE_ECDH_INIT;
    conn->pending_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_destroy);

    // Set callbacks
    bufferevent_setcb(bev, read_cb, NULL, event_cb, conn);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    bufferevent_set_timeouts(bev, NULL, NULL); // No timeouts for now

    // Add to connections table
    g_hash_table_insert(g_connections, conn, conn);

    secure_log("INFO", "New connection from %s", ip);
}

// Signal handler
static void signal_cb(evutil_socket_t sig, short events, void *ctx) {
    struct event_base *base = ctx;
    secure_log("INFO", "Received signal %d, shutting down", sig);
    g_shutdown = 1;
    event_base_loopexit(base, NULL);
}

// Main function
int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;

    // Parse arguments with getopt
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Initialize libsodium
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return EXIT_FAILURE;
    }

    // Initialize event base
    g_event_base = event_base_new();
    if (!g_event_base) {
        fprintf(stderr, "Failed to create event base\n");
        return EXIT_FAILURE;
    }

    // Initialize SSL
    g_ssl_ctx = init_ssl_context();
    if (!g_ssl_ctx) {
        return EXIT_FAILURE;
    }

    // Initialize MongoDB
    if (init_mongodb() != 0) {
        return EXIT_FAILURE;
    }

    // Initialize hash tables
    g_connections = g_hash_table_new(g_direct_hash, g_direct_equal);
    g_rate_limits = g_hash_table_new(g_int_hash, g_int_equal);

    // Set up signal handling
    struct event *sig_int = evsignal_new(g_event_base, SIGINT, signal_cb, g_event_base);
    struct event *sig_term = evsignal_new(g_event_base, SIGTERM, signal_cb, g_event_base);
    evsignal_add(sig_int, NULL);
    evsignal_add(sig_term, NULL);

    // Set up listener
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);

    struct evconnlistener *listener = evconnlistener_new_bind(
        g_event_base, accept_cb, g_event_base,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
        (struct sockaddr*)&sin, sizeof(sin));

    if (!listener) {
        fprintf(stderr, "Failed to create listener\n");
        return EXIT_FAILURE;
    }

    secure_log("INFO", "Secure file server started on port %d", port);

    // Run event loop
    event_base_dispatch(g_event_base);

    // Cleanup
    evconnlistener_free(listener);
    event_free(sig_int);
    event_free(sig_term);

    g_hash_table_destroy(g_connections);
    g_hash_table_destroy(g_rate_limits);

    if (g_collection) mongoc_collection_destroy(g_collection);
    if (g_mongo_client) mongoc_client_destroy(g_mongo_client);
    mongoc_cleanup();

    if (g_ssl_ctx) SSL_CTX_free(g_ssl_ctx);
    EVP_cleanup();

    event_base_free(g_event_base);

    secure_log("INFO", "Server shutdown complete");
    return EXIT_SUCCESS;
}
