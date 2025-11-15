/**
 * Secure File Exchange Client - Modern CLI with ncurses
 * Features: ECDH, XChaCha20-Poly1305, Tor support, fancy UI
 */

#include <ncurses.h>
#include <menu.h>
#include <form.h>
#include <panel.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/buffer.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sodium.h>
#include <glib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BLAKE3_IMPLEMENTATION
#include "blake3.h"

#include "../../include/protocol.h"
#include "../crypto/crypto_session.h"

// UI Colors and themes
#define COLOR_BG_DEFAULT  COLOR_BLACK
#define COLOR_FG_DEFAULT  COLOR_WHITE
#define COLOR_BG_HEADER   COLOR_BLUE
#define COLOR_FG_HEADER   COLOR_WHITE
#define COLOR_BG_PROGRESS COLOR_GREEN
#define COLOR_FG_PROGRESS COLOR_BLACK
#define COLOR_BG_ERROR    COLOR_RED
#define COLOR_FG_ERROR    COLOR_WHITE

// UI dimensions
#define HEADER_HEIGHT 3
#define FOOTER_HEIGHT 2
#define PROGRESS_HEIGHT 3
#define MAIN_HEIGHT (LINES - HEADER_HEIGHT - FOOTER_HEIGHT - PROGRESS_HEIGHT)

// Client state
typedef enum {
    STATE_DISCONNECTED,
    STATE_CONNECTING,
    STATE_ECDH_HANDSHAKE,
    STATE_AUTHENTICATED,
    STATE_TRANSFERRING
} client_state_t;

// Transfer information
typedef struct {
    char filename[FILENAME_MAX_LEN];
    long long filesize;
    long long transferred;
    time_t start_time;
    int progress_percentage;
} transfer_info_t;

// Global UI elements
static WINDOW *header_win = NULL;
static WINDOW *main_win = NULL;
static WINDOW *progress_win = NULL;
static WINDOW *footer_win = NULL;
static PANEL *header_panel = NULL;
static PANEL *main_panel = NULL;
static PANEL *progress_panel = NULL;
static PANEL *footer_panel = NULL;

// Global state
static client_state_t g_client_state = STATE_DISCONNECTED;
static crypto_session_t g_crypto_session;
static transfer_info_t g_transfer_info;
static struct bufferevent *g_bev = NULL;
static SSL_CTX *g_ssl_ctx = NULL;
static volatile sig_atomic_t g_shutdown = 0;

// UI functions
static void init_ui(void) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    // Initialize colors
    start_color();
    init_pair(1, COLOR_FG_DEFAULT, COLOR_BG_DEFAULT);
    init_pair(2, COLOR_FG_HEADER, COLOR_BG_HEADER);
    init_pair(3, COLOR_FG_PROGRESS, COLOR_BG_PROGRESS);
    init_pair(4, COLOR_FG_ERROR, COLOR_BG_ERROR);

    // Create windows
    header_win = newwin(HEADER_HEIGHT, COLS, 0, 0);
    main_win = newwin(MAIN_HEIGHT, COLS, HEADER_HEIGHT, 0);
    progress_win = newwin(PROGRESS_HEIGHT, COLS, HEADER_HEIGHT + MAIN_HEIGHT, 0);
    footer_win = newwin(FOOTER_HEIGHT, COLS, LINES - FOOTER_HEIGHT, 0);

    // Create panels
    header_panel = new_panel(header_win);
    main_panel = new_panel(main_win);
    progress_panel = new_panel(progress_win);
    footer_panel = new_panel(footer_win);

    // Set up windows
    wbkgd(header_win, COLOR_PAIR(2));
    wbkgd(main_win, COLOR_PAIR(1));
    wbkgd(progress_win, COLOR_PAIR(1));
    wbkgd(footer_win, COLOR_PAIR(1));

    update_panels();
    doupdate();
}

static void cleanup_ui(void) {
    if (header_panel) del_panel(header_panel);
    if (main_panel) del_panel(main_panel);
    if (progress_panel) del_panel(progress_panel);
    if (footer_panel) del_panel(footer_panel);

    if (header_win) delwin(header_win);
    if (main_win) delwin(main_win);
    if (progress_win) delwin(progress_win);
    if (footer_win) delwin(footer_win);

    endwin();
}

static void draw_header(const char *title) {
    werase(header_win);
    box(header_win, 0, 0);
    mvwprintw(header_win, 1, (COLS - strlen(title)) / 2, "%s", title);
    wnoutrefresh(header_win);
}

static void draw_progress_bar(int percentage, const char *status) {
    werase(progress_win);
    box(progress_win, 0, 0);

    int bar_width = COLS - 4;
    int filled = (percentage * bar_width) / 100;

    mvwprintw(progress_win, 1, 1, "[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            wattron(progress_win, COLOR_PAIR(3));
            waddch(progress_win, '=');
            wattroff(progress_win, COLOR_PAIR(3));
        } else {
            waddch(progress_win, ' ');
        }
    }
    wprintw(progress_win, "] %d%%", percentage);

    if (status) {
        mvwprintw(progress_win, 2, 1, "%s", status);
    }

    wnoutrefresh(progress_win);
}

static void draw_footer(const char *message) {
    werase(footer_win);
    mvwprintw(footer_win, 0, 0, "%s", message);
    wnoutrefresh(footer_win);
}

static void update_status(const char *message) {
    draw_footer(message);
    update_panels();
    doupdate();
}

// Network functions
static SSL_CTX *init_ssl_context(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        return NULL;
    }

    // Load client certificate
    if (SSL_CTX_use_certificate_file(ctx, "src/client-cert.pem", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "src/client-key.pem", SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

static void perform_ecdh_exchange(void) {
    update_status("Performing ECDH key exchange...");

    // Initialize crypto session
    if (crypto_session_init(&g_crypto_session) != 0) {
        update_status("Failed to initialize crypto session");
        return;
    }

    // Send ECDH init packet
    ECDHInitPacket init_packet;
    memcpy(init_packet.public_key, g_crypto_session.public_key, ECDH_PUBLIC_KEY_LEN);
    randombytes_buf(init_packet.nonce, XCHACHA20_NONCE_LEN);

    bufferevent_write(g_bev, &init_packet, sizeof(init_packet));

    g_client_state = STATE_ECDH_HANDSHAKE;
    update_status("ECDH initiation sent, waiting for response...");
}

// Event callbacks
static void read_cb(struct bufferevent *bev, void *ctx) {
    struct evbuffer *input = bufferevent_get_input(bev);

    switch (g_client_state) {
        case STATE_ECDH_HANDSHAKE: {
            size_t len = evbuffer_get_length(input);
            if (len >= sizeof(ECDHResponsePacket)) {
                ECDHResponsePacket response;
                evbuffer_remove(input, &response, sizeof(response));

                // Store peer's public key
                memcpy(g_crypto_session.peer_public_key, response.public_key, ECDH_PUBLIC_KEY_LEN);

                // Compute shared secret
                if (crypto_session_compute_shared_secret(&g_crypto_session) != 0 ||
                    crypto_session_derive_session_key(&g_crypto_session) != 0) {
                    update_status("ECDH handshake failed");
                    return;
                }

                // Send session key
                SessionKeyPacket session_packet;
                memcpy(session_packet.session_key, g_crypto_session.session_key, SESSION_KEY_LEN);

                blake3_hasher hasher;
                blake3_hasher_init(&hasher);
                blake3_hasher_update(&hasher, session_packet.session_key, SESSION_KEY_LEN);
                blake3_hasher_finalize(&hasher, session_packet.key_hash, BLAKE3_HASH_LEN);

                bufferevent_write(g_bev, &session_packet, sizeof(session_packet));

                g_client_state = STATE_AUTHENTICATED;
                update_status("Session established successfully!");
            }
            break;
        }
        case STATE_AUTHENTICATED: {
            size_t len = evbuffer_get_length(input);
            if (len >= sizeof(ResponseHeader)) {
                ResponseHeader resp;
                evbuffer_remove(input, &resp, sizeof(resp));

                if (resp.status == RESP_SUCCESS) {
                    update_status("Command executed successfully");
                } else {
                    update_status("Command failed");
                }
            }
            break;
        }
        default:
            break;
    }
}

static void event_cb(struct bufferevent *bev, short events, void *ctx) {
    if (events & BEV_EVENT_CONNECTED) {
        update_status("Connected to server, starting ECDH handshake...");
        perform_ecdh_exchange();
    } else if (events & BEV_EVENT_ERROR) {
        update_status("Connection error");
        g_client_state = STATE_DISCONNECTED;
    }
}

// Command handling
static void upload_file(const char *local_path, const char *remote_name, const char *recipient) {
    if (g_client_state != STATE_AUTHENTICATED) {
        update_status("Not connected to server");
        return;
    }

    // Get file size
    struct stat st;
    if (stat(local_path, &st) != 0) {
        update_status("Failed to get file information");
        return;
    }

    long long filesize = st.st_size;

    // Encrypt metadata
    EncryptedMetadata encrypted;
    if (crypto_session_encrypt_metadata(&g_crypto_session, remote_name, filesize,
                                      recipient ? recipient : "", &encrypted) != 0) {
        update_status("Failed to encrypt metadata");
        return;
    }

    // Send upload request
    RequestHeader req = {
        .command = CMD_UPLOAD,
        .metadata = encrypted,
        .flags = 0,
        .offset = 0
    };

    randombytes_buf(req.packet_nonce, XCHACHA20_NONCE_LEN);

    bufferevent_write(g_bev, &req, sizeof(req));

    // Initialize transfer info
    strcpy(g_transfer_info.filename, remote_name);
    g_transfer_info.filesize = filesize;
    g_transfer_info.transferred = 0;
    g_transfer_info.start_time = time(NULL);
    g_transfer_info.progress_percentage = 0;

    g_client_state = STATE_TRANSFERRING;
    update_status("Starting file upload...");
}

// Main UI loop
static void ui_loop(void) {
    int ch;
    char *input;
    char prompt[256];

    while (!g_shutdown) {
        // Update progress if transferring
        if (g_client_state == STATE_TRANSFERRING) {
            g_transfer_info.progress_percentage =
                (int)((g_transfer_info.transferred * 100) / g_transfer_info.filesize);
            draw_progress_bar(g_transfer_info.progress_percentage, g_transfer_info.filename);
        }

        update_panels();
        doupdate();

        // Handle input
        timeout(100); // 100ms timeout
        ch = getch();

        if (ch == 'q' || ch == 'Q') {
            g_shutdown = 1;
        } else if (ch == 'c' || ch == 'C') {
            if (g_client_state == STATE_DISCONNECTED) {
                // Connect to server
                struct sockaddr_in sin;
                memset(&sin, 0, sizeof(sin));
                sin.sin_family = AF_INET;
                sin.sin_port = htons(DEFAULT_PORT);
                inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);

                g_bev = bufferevent_openssl_socket_new(
                    event_base_new(), -1, SSL_new(g_ssl_ctx),
                    BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE);

                bufferevent_setcb(g_bev, read_cb, NULL, event_cb, NULL);
                bufferevent_enable(g_bev, EV_READ | EV_WRITE);

                if (bufferevent_socket_connect(g_bev, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
                    update_status("Failed to connect");
                } else {
                    g_client_state = STATE_CONNECTING;
                    update_status("Connecting...");
                }
            }
        } else if (ch == 'u' || ch == 'U') {
            if (g_client_state == STATE_AUTHENTICATED) {
                // Simple upload command
                upload_file("test.txt", "uploaded_test.txt", NULL);
            }
        }
    }
}

// Signal handler
static void signal_handler(int sig) {
    g_shutdown = 1;
}

// Main function
int main(int argc, char *argv[]) {
    // Initialize libsodium
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return EXIT_FAILURE;
    }

    // Initialize SSL
    g_ssl_ctx = init_ssl_context();
    if (!g_ssl_ctx) {
        fprintf(stderr, "Failed to initialize SSL\n");
        return EXIT_FAILURE;
    }

    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize UI
    init_ui();
    draw_header("Secure File Exchange Client");
    update_status("Press 'c' to connect, 'u' to upload, 'q' to quit");

    // Main loop
    ui_loop();

    // Cleanup
    if (g_bev) bufferevent_free(g_bev);
    if (g_ssl_ctx) SSL_CTX_free(g_ssl_ctx);
    crypto_session_cleanup(&g_crypto_session);
    cleanup_ui();

    return EXIT_SUCCESS;
}
