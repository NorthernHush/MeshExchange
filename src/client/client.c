/*
    Улыбнись
*/

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>

#include "../../include/protocol.h"

#define BLAKE3_IMPLEMENTATION

#include "blake3.h"

#define DEFAULT_PORT     5151
#define CLEAR "clear"
#define BUFFER_SIZE      4096
#define FILENAME_MAX_LEN 256
#define BAR_LENGTH       20 

int compute_file_blake3(const char *filepath, uint8_t out_hash[BLAKE3_HASH_LEN]);
void display_progress(float progress);

/*
 * Initialize OpenSSL client context for mTLS.
 * Loads CA, client certificate, and private key.
 * Exits on failure to ensure secure operation.
 */
static SSL_CTX *init_client_ssl_ctx(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* Load CA certificate to verify server identity */
    if (SSL_CTX_load_verify_locations(ctx, "src/ca.pem", NULL) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* Load client certificate and private key for mTLS */
    if (SSL_CTX_use_certificate_file(ctx, "src/client-cert.pem", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "src/client-key.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* Enforce server certificate verification */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    return ctx;
}

/*
 * Reliable SSL write: ensures all bytes are sent.
 */
static int ssl_send_all(SSL *ssl, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = SSL_write(ssl, (const char *)buf + sent, len - sent);
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }
    return 0;
}

/*
* ANSCI anim
*/

static void micro_sleep(long mircoseconds) {

    struct timespec ts;

    ts.tv_sec = mircoseconds / 1000000;
    ts.tv_nsec = (mircoseconds % 1000000) * 1000;
    nanosleep(&ts, &ts);

}

static void draw(int position) {

    printf("\033[2J\033[H");

    for (int i = 0; i < position; i++) {

        printf(" ");

    }

    printf("o\n");

    fflush(stdout);

}

static void show_progress(long long sent, long long total) {
    static const char spinner[] = "|/-\\";

    static int frame = 0;

    fprintf(stderr, "\r\x1b[Uploading: [%3lld%%] %c  (%lld/%lld bytes)",
            (total > 0 ? (sent * 100 / total) : 0),
            spinner[frame++ % 4],
            sent, total);

    fflush(stdout);

}


/*
 * Reliable SSL read: ensures all expected bytes are received.
 */
static int ssl_recv_all(SSL *ssl, void *buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        int n = SSL_read(ssl, (char *)buf + received, len - received);
        if (n <= 0) {
            return -1;
        }
        received += n;
    }
    return 0;
}

/*
 * Upload a file to the server over mTLS.
 * Uses the secure SSL channel for all communication.
 */
static int upload_file_ssl(SSL *ssl, const char *local_filepath, const char *remote_filename, const char *recipient) {
    FILE *fp = NULL;
    char buffer[BUFFER_SIZE];
    struct stat st;
    RequestHeader header;
    ResponseHeader response;

    if (stat(local_filepath, &st) == -1) {
        perror("stat");
        fprintf(stderr, "Error: Could not get file size for %s\n", local_filepath);
        return -1;
    }
    long long filesize = st.st_size;

    fp = fopen(local_filepath, "rb");
    if (!fp) {
        perror("fopen");
        fprintf(stderr, "Error: Could not open file %s for reading.\n", local_filepath);
        return -1;
    }

    /* Send upload request header */
    header.command = CMD_UPLOAD;
    strncpy(header.filename, remote_filename, FILENAME_MAX_LEN - 1);
    header.filename[FILENAME_MAX_LEN - 1] = '\0';
    header.filesize = filesize;
    uint8_t file_hash[BLAKE3_HASH_LEN];

    memset(header.recipient, 0, sizeof(header.recipient));
    if(recipient && strlen(recipient) > 0) {
        strncpy(header.recipient, recipient, FINGERPRINT_LEN - 1);
        header.recipient[FINGERPRINT_LEN - 1] = '\0';
    }

    if (compute_file_blake3(local_filepath, file_hash) != 0) {
        
        fprintf(stderr, "error: could not compute hash for %s\n", local_filepath);

        fclose(fp);
        return -1;

    }

    header.command = CMD_UPLOAD;
    strncpy(header.filename,remote_filename, FILENAME_MAX_LEN - 1);
    header.filename[FILENAME_MAX_LEN - 1] = '\0';
    header.filesize = filesize;
    memcpy(header.file_hash, file_hash, BLAKE3_HASH_LEN);

    printf("Uploading '%s' (%lld bytes) as '%s'...\n", local_filepath, filesize, remote_filename);
    if (ssl_send_all(ssl, &header, sizeof(RequestHeader)) == -1) {
        fclose(fp);
        return -1;
    }

    /* Wait for server readiness confirmation */
    if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) == -1) {
        fclose(fp);
        return -1;
    }

    if (response.status != RESP_SUCCESS) {
        fprintf(stderr, "Server rejected upload: Status %d\n", response.status);
        fclose(fp);
        return -1;
    }

    printf("Server ready for upload. Sending file data...\n");

    /* Send file content */
    long long total_sent = 0;
    ssize_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (ssl_send_all(ssl, buffer, bytes_read) == -1) {
            fprintf(stderr, "Failed to send file data.\n");
            fclose(fp);
            return -1;
        }
        total_sent += bytes_read;
        float progress = (float)total_sent / (float)filesize;
        display_progress(progress);
        usleep(10000);
        // show_progress(total_sent, filesize);
    }

    fprintf(stderr, "\nUpload completed!\n");

    if (ferror(fp)) {
        perror("fread");
        fprintf(stderr, "Error reading from local file %s.\n", local_filepath);
        fclose(fp);
        return -1;
    }

    printf("File data sent. Total: %lld bytes.\n", total_sent);

    /* Receive final upload status */
    if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) == -1) {
        fclose(fp);
        return -1;
    }

    if (response.status == RESP_SUCCESS) {
        printf("Upload completed successfully!\n");
    } else {
        fprintf(stderr, "Upload failed on server: Status %d\n", response.status);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

/*
* BLAKE3 compute file 
*/

int compute_file_blake3(const char *filepath, uint8_t out_hash[BLAKE3_HASH_LEN]) {

    FILE *fp = fopen(filepath, "rb");
    
    if (!fp) return -1;

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    unsigned char buf[4096];
    size_t bytes;

    while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0) {

        blake3_hasher_update(&hasher, buf, bytes);

    }

    if (ferror(fp)) { 

        fclose(fp);
        return -1;

    }

    blake3_hasher_finalize(&hasher, out_hash, BLAKE3_HASH_LEN);

    fclose(fp);
    return 0;

}

/*
 * Download a file from the server over mTLS.
 */
static int download_file_ssl(SSL *ssl, const char *remote_filename, const char *local_filepath) {
    FILE *fp = NULL;
    char buffer[BUFFER_SIZE];
    RequestHeader header;
    ResponseHeader response;

    fp = fopen(local_filepath, "ab");

    if (!fp) {

        perror("fopen");
        fprintf(stderr, "Error: Could not open file %s for writing.\n", local_filepath);
        return -1;
                
    }

    if (fseek(fp, 0, SEEK_END) != 0) {

        perror("fseek");
        fclose(fp);
        return -1;
    }

    long long offset = ftell(fp);
    if (offset == -1) {

        perror("ftell");
        fclose(fp);
        return -1;
    }

    /* Send download request */
    header.command = CMD_DOWNLOAD;
    strncpy(header.filename, remote_filename, FILENAME_MAX_LEN - 1);
    header.filename[FILENAME_MAX_LEN - 1] = '\0';
    header.filesize = 0;

    printf("Requesting download of '%s' to '%s'...\n", remote_filename, local_filepath);
    if (ssl_send_all(ssl, &header, sizeof(RequestHeader)) == -1) {
        return -1;
    }

    /* Receive file metadata */
    if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) == -1) {
        return -1;
    }

    if (response.status != RESP_SUCCESS) {
        fprintf(stderr, "Server rejected download request: Status %d\n", response.status);
        return -1;
    }

    long long filesize = response.filesize;
    if (filesize <= 0) {
        fprintf(stderr, "Server reported invalid file size (%lld) for download.\n", filesize);
        return -1;
    }

    printf("Server has file '%s' (%lld bytes). Starting download...\n", remote_filename, filesize);

    fp = fopen(local_filepath, "wb");
    if (!fp) {
        perror("fopen");
        fprintf(stderr, "Error: Could not open file %s for writing.\n", local_filepath);
        return -1;
    }

    /* Receive file content */
    long long total_received = offset;
    while (total_received < filesize) {
        size_t bytes_to_read = (filesize - total_received < BUFFER_SIZE) ? 
                               (size_t)(filesize - total_received) : BUFFER_SIZE;
        int bytes_received = SSL_read(ssl, buffer, bytes_to_read);
        if (bytes_received <= 0) {
            perror("SSL_read");
            fclose(fp);
            return -1;
        }

        if (fwrite(buffer, 1, bytes_received, fp) != (size_t)bytes_received) {
            perror("fwrite");
            fprintf(stderr, "Error writing to local file %s.\n", local_filepath);
            fclose(fp);
            return -1;
        }
        total_received += bytes_received;
        float progress = (float)(total_received - offset) / (float)(filesize - offset);
        display_progress(progress);
    }

    if (ferror(fp)) {
        perror("ferror");
        fprintf(stderr, "Error during file writing to %s.\n", local_filepath);
        fclose(fp);
        return -1;
    }

    printf("Download completed successfully! Saved to '%s'. Total: %lld bytes.\n", 
           local_filepath, total_received);
    fclose(fp);
    return 0;
}

void display_progress(float progress) {
//progress должна быть в диапазоне от 0.0 до 1.0
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    int filled_length = (int)(progress * BAR_LENGTH);
    char bar[BAR_LENGTH + 1]; // +1 для нуль-терминатора
    memset(bar, '#', filled_length);
    memset(bar + filled_length, ' ', BAR_LENGTH - filled_length);
    bar[BAR_LENGTH] = '\0';

    //Формируем строку для вывода
    char output[100]; //Достаточно большого размера, чтобы вместить все
    sprintf(output, "[%s] %.1f%%\n", bar, progress * 100.0);

    system(CLEAR); // Очистка экрана
    printf("%s", output); // Вывод прогресс-бара и процента
}


/*
 * Request file list from server over mTLS.
 */
static int list_files_ssl(SSL *ssl) {
    RequestHeader header;
    ResponseHeader response;

    /* Send list request */
    header.command = CMD_LIST;
    header.filename[0] = '\0';
    header.filesize = 0;

    printf("Requesting file list from server...\n");
    if (ssl_send_all(ssl, &header, sizeof(RequestHeader)) == -1) {
        return -1;
    }

    /* Receive list metadata */
    if (ssl_recv_all(ssl, &response, sizeof(ResponseHeader)) == -1) {
        return -1;
    }

    if (response.status != RESP_SUCCESS) {
        fprintf(stderr, "Server rejected list request: Status %d\n", response.status);
        return -1;
    }

    long long list_len = response.filesize;
    if (list_len <= 0) {
        printf("No files found on server.\n");
        return 0;
    }

    printf("File list from server (%lld bytes):\n", list_len);

    /* Receive and display file list */
    long long total_received = 0;
    char buffer[BUFFER_SIZE];
    while (total_received < list_len) {
        size_t bytes_to_read = (list_len - total_received < BUFFER_SIZE) ? 
                               (size_t)(list_len - total_received) : BUFFER_SIZE;
        int bytes_received = SSL_read(ssl, buffer, bytes_to_read);
        if (bytes_received <= 0) {
            perror("SSL_read");
            return -1;
        }

        fwrite(buffer, 1, bytes_received, stdout);
        total_received += bytes_received;
    }

    printf("\n");
    return 0;
}

void print_startup_logo(void) {
    printf("\n");
    printf(" /$$      /$$                     /$$       /$$$$$$$$                     /$$                                              \n");
    printf("| $$$    /$$$                    | $$      | $$_____/                    | $$                                              \n");
    printf("| $$$$  /$$$$  /$$$$$$   /$$$$$$$| $$$$$$$ | $$       /$$   /$$  /$$$$$$$| $$$$$$$   /$$$$$$  /$$$$$$$   /$$$$$$   /$$$$$$ \n");
    printf("| $$ $$/$$ $$ /$$__  $$ /$$_____/| $$__  $$| $$$$$   |  $$ /$$/ /$$_____/| $$__  $$ |____  $$| $$__  $$ /$$__  $$ /$$__  $$\n");
    printf("| $$  $$$| $$| $$$$$$$$|  $$$$$$ | $$  \\ $$| $$__/    \\  $$$$/ | $$      | $$  \\ $$  /$$$$$$$| $$  \\ $$| $$  \\ $$| $$$$$$$$\n");
    printf("| $$\\  $ | $$| $$_____/ \\____  $$| $$  | $$| $$        >$$  $$ | $$      | $$  | $$ /$$__  $$| $$  | $$| $$  | $$| $$_____/\n");
    printf("| $$ \\/  | $$|  $$$$$$$ /$$$$$$$/| $$  | $$| $$$$$$$$ /$$/\\  $$|  $$$$$$$| $$  | $$|  $$$$$$$| $$  | $$|  $$$$$$$|  $$$$$$$\n");
    printf("|__/     |__/ \\_______/|_______/ |__/  |__/|________/|__/  \\__/ \\_______/|__/  |__/ \\_______/|__/  |__/ \\____  $$ \\_______/\n");
    printf("                                                                                                        /$$  \\ $$          \n");
    printf("                                                                                                       |  $$$$$$/          \n");
    printf("                                                                                                        \\______/           \n");
    printf("\n");
                                                                                                  


}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    char *server_ip = "127.0.0.1";
    int port = DEFAULT_PORT;

    if (argc < 2) {
        print_startup_logo();
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  %s upload <local_filepath> <remote_filename>\n", argv[0]);
        fprintf(stderr, "  %s download <remote_filename> <local_filepath>\n", argv[0]);
        fprintf(stderr, "  %s list\n", argv[0]);
        fprintf(stderr, "Optional: --ip <ip> --port <port>\n");
        return EXIT_FAILURE;
    }

    /* Parse command-line arguments */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    /* Create TCP socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return EXIT_FAILURE;
    }

    print_startup_logo();

    /* Connect to server */
    printf("Connecting to %s:%d...\n", server_ip, port);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        return EXIT_FAILURE;
    }

    /* Initialize mTLS */
    SSL_CTX *ctx = init_client_ssl_ctx();
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        close(sock);
        return EXIT_FAILURE;
    }

    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sock);
        return EXIT_FAILURE;
    }

    printf("mTLS handshake successful.\n");

    /* Execute requested command */
    char *cmd_str = argv[1];
    int result = EXIT_FAILURE;

    if (strcmp(cmd_str, "upload") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s upload <local_filepath> <remote_filename> [recipient_fingerprint]\n", argv[0]);
            return EXIT_FAILURE;
        }

    const char *recipient = (argc >= 5) ? argv[4] : "";
    result = upload_file_ssl(ssl, argv[2], argv[3], recipient) ? EXIT_FAILURE : EXIT_SUCCESS;
    } else if (strcmp(cmd_str, "download") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s download <remote_filename> <local_filepath>\n", argv[0]);
        } else {
            result = download_file_ssl(ssl, argv[2], argv[3]) ? EXIT_FAILURE : EXIT_SUCCESS;
        }
    } else if (strcmp(cmd_str, "list") == 0) {
        result = list_files_ssl(ssl) ? EXIT_FAILURE : EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd_str);
    }

    /* Cleanup */
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);
    return result;
}
