/**
 * Secure File Exchange Server - Admin Panel
 * Features: ncurses UI, fingerprint verification, client control, ban/unban functionality
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
#include <mongoc/mongoc.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define BLAKE3_IMPLEMENTATION
#include "blake3.h"

#include "../../include/protocol.h"
#include "../crypto/crypto_session.h"
#include "../db/mongo_ops_server.h"

// Admin panel configuration
#define ADMIN_PASSWORD "admin123" // In production, use proper authentication
#define MAX_BANNED_CLIENTS 1000
#define ADMIN_LOG_FILE "/tmp/admin_panel.log"

// Admin data structures
typedef struct {
    char session_key[65]; // Hex-encoded session key
    char fingerprint[FINGERPRINT_LEN];
    char ip_address[INET_ADDRSTRLEN];
    time_t banned_at;
    char reason[256];
} banned_client_t;

typedef struct {
    char fingerprint[FINGERPRINT_LEN];
    int approved;
    time_t last_seen;
    char permissions[256]; // JSON-like string of permissions
} client_info_t;

// Global admin state
static banned_client_t g_banned_clients[MAX_BANNED_CLIENTS];
static int g_banned_count = 0;
static GHashTable *g_client_permissions = NULL;
static GHashTable *g_fingerprint_cache = NULL;
static volatile sig_atomic_t g_admin_shutdown = 0;

// UI elements
static WINDOW *admin_header_win = NULL;
static WINDOW *admin_main_win = NULL;
static WINDOW *admin_status_win = NULL;
static WINDOW *admin_footer_win = NULL;
static PANEL *admin_header_panel = NULL;
static PANEL *admin_main_panel = NULL;
static PANEL *admin_status_panel = NULL;
static PANEL *admin_footer_panel = NULL;

// UI Colors for admin panel
#define ADMIN_COLOR_BG_DEFAULT  COLOR_BLACK
#define ADMIN_COLOR_FG_DEFAULT  COLOR_WHITE
#define ADMIN_COLOR_BG_HEADER   COLOR_BLUE
#define ADMIN_COLOR_FG_HEADER   COLOR_WHITE
#define ADMIN_COLOR_BG_MENU     COLOR_CYAN
#define ADMIN_COLOR_FG_MENU     COLOR_BLACK
#define ADMIN_COLOR_BG_ERROR    COLOR_RED
#define ADMIN_COLOR_FG_ERROR    COLOR_WHITE
#define ADMIN_COLOR_BG_SUCCESS  COLOR_GREEN
#define ADMIN_COLOR_FG_SUCCESS  COLOR_BLACK

// UI dimensions
#define ADMIN_HEADER_HEIGHT 4
#define ADMIN_FOOTER_HEIGHT 3
#define ADMIN_STATUS_HEIGHT 5
#define ADMIN_MAIN_HEIGHT (LINES - ADMIN_HEADER_HEIGHT - ADMIN_FOOTER_HEIGHT - ADMIN_STATUS_HEIGHT)

// Admin logging
static void admin_log(const char *level, const char *format, ...) {
    time_t now = time(NULL);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    FILE *log_fp = fopen(ADMIN_LOG_FILE, "a");
    if (log_fp) {
        va_list args;
        va_start(args, format);
        fprintf(log_fp, "[%s] [%s] ", timestamp, level);
        vfprintf(log_fp, format, args);
        fprintf(log_fp, "\n");
        va_end(args);
        fclose(log_fp);
    }
}

// Initialize admin UI
static void init_admin_ui(void) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    // Initialize colors
    start_color();
    init_pair(1, ADMIN_COLOR_FG_DEFAULT, ADMIN_COLOR_BG_DEFAULT);
    init_pair(2, ADMIN_COLOR_FG_HEADER, ADMIN_COLOR_BG_HEADER);
    init_pair(3, ADMIN_COLOR_FG_MENU, ADMIN_COLOR_BG_MENU);
    init_pair(4, ADMIN_COLOR_FG_ERROR, ADMIN_COLOR_BG_ERROR);
    init_pair(5, ADMIN_COLOR_FG_SUCCESS, ADMIN_COLOR_BG_SUCCESS);

    // Create windows
    admin_header_win = newwin(ADMIN_HEADER_HEIGHT, COLS, 0, 0);
    admin_main_win = newwin(ADMIN_MAIN_HEIGHT, COLS, ADMIN_HEADER_HEIGHT, 0);
    admin_status_win = newwin(ADMIN_STATUS_HEIGHT, COLS, ADMIN_HEADER_HEIGHT + ADMIN_MAIN_HEIGHT, 0);
    admin_footer_win = newwin(ADMIN_FOOTER_HEIGHT, COLS, LINES - ADMIN_FOOTER_HEIGHT, 0);

    // Create panels
    admin_header_panel = new_panel(admin_header_win);
    admin_main_panel = new_panel(admin_main_win);
    admin_status_panel = new_panel(admin_status_win);
    admin_footer_panel = new_panel(admin_footer_win);

    // Set up windows
    wbkgd(admin_header_win, COLOR_PAIR(2));
    wbkgd(admin_main_win, COLOR_PAIR(1));
    wbkgd(admin_status_win, COLOR_PAIR(1));
    wbkgd(admin_footer_win, COLOR_PAIR(1));

    update_panels();
    doupdate();
}

static void cleanup_admin_ui(void) {
    if (admin_header_panel) del_panel(admin_header_panel);
    if (admin_main_panel) del_panel(admin_main_panel);
    if (admin_status_panel) del_panel(admin_status_panel);
    if (admin_footer_panel) del_panel(admin_footer_panel);

    if (admin_header_win) delwin(admin_header_win);
    if (admin_main_win) delwin(admin_main_win);
    if (admin_status_win) delwin(admin_status_win);
    if (admin_footer_win) delwin(admin_footer_win);

    endwin();
}

static void draw_admin_header(void) {
    werase(admin_header_win);
    box(admin_header_win, 0, 0);

    mvwprintw(admin_header_win, 1, 2, "🔐 Secure File Exchange - Admin Control Panel");
    mvwprintw(admin_header_win, 2, 2, "Server Status: ACTIVE | Banned Clients: %d | Connected: %d",
              g_banned_count, g_hash_table_size(g_client_permissions));

    wnoutrefresh(admin_header_win);
}

static void draw_admin_status(const char *message) {
    werase(admin_status_win);
    box(admin_status_win, 0, 0);

    mvwprintw(admin_status_win, 1, 2, "Status: %s", message ? message : "Ready");

    // Show recent activity
    mvwprintw(admin_status_win, 2, 2, "Recent Activity:");
    mvwprintw(admin_status_win, 3, 4, "- Server running on port 5162");
    mvwprintw(admin_status_win, 4, 4, "- SSL/TLS encryption enabled");

    wnoutrefresh(admin_status_win);
}

static void draw_admin_footer(void) {
    werase(admin_footer_win);
    mvwprintw(admin_footer_win, 1, 2, "Commands: (v)iew clients | (b)an client | (u)nban client | (p)ermissions | (f)ingerprints | (q)uit");
    wnoutrefresh(admin_footer_win);
}

static void draw_main_menu(void) {
    werase(admin_main_win);
    box(admin_main_win, 0, 0);

    int y = 2;
    mvwprintw(admin_main_win, y++, 2, "=== ADMIN CONTROL PANEL ===");
    mvwprintw(admin_main_win, y++, 2, "");
    mvwprintw(admin_main_win, y++, 2, "1. View Connected Clients");
    mvwprintw(admin_main_win, y++, 2, "2. Ban Client by Session Key");
    mvwprintw(admin_main_win, y++, 2, "3. Unban Client");
    mvwprintw(admin_main_win, y++, 2, "4. Set Client Permissions");
    mvwprintw(admin_main_win, y++, 2, "5. View Fingerprints");
    mvwprintw(admin_main_win, y++, 2, "6. View Banned Clients");
    mvwprintw(admin_main_win, y++, 2, "7. Server Statistics");
    mvwprintw(admin_main_win, y++, 2, "");
    mvwprintw(admin_main_win, y++, 2, "Press number or key to select option...");

    wnoutrefresh(admin_main_win);
}

// Admin functions
static int is_client_banned(const char *session_key) {
    for (int i = 0; i < g_banned_count; i++) {
        if (strcmp(g_banned_clients[i].session_key, session_key) == 0) {
            return 1;
        }
    }
    return 0;
}

static int ban_client(const char *session_key, const char *reason) {
    if (g_banned_count >= MAX_BANNED_CLIENTS) {
        return -1; // Max banned clients reached
    }

    if (is_client_banned(session_key)) {
        return -2; // Already banned
    }

    // Add to banned list
    strcpy(g_banned_clients[g_banned_count].session_key, session_key);
    strcpy(g_banned_clients[g_banned_count].reason, reason);
    g_banned_clients[g_banned_count].banned_at = time(NULL);

    g_banned_count++;

    admin_log("INFO", "Client banned: %s (reason: %s)", session_key, reason);
    return 0;
}

static int unban_client(const char *session_key) {
    for (int i = 0; i < g_banned_count; i++) {
        if (strcmp(g_banned_clients[i].session_key, session_key) == 0) {
            // Remove by shifting array
            for (int j = i; j < g_banned_count - 1; j++) {
                g_banned_clients[j] = g_banned_clients[j + 1];
            }
            g_banned_count--;
            admin_log("INFO", "Client unbanned: %s", session_key);
            return 0;
        }
    }
    return -1; // Not found
}

static void show_banned_clients(void) {
    werase(admin_main_win);
    box(admin_main_win, 0, 0);

    int y = 2;
    mvwprintw(admin_main_win, y++, 2, "=== BANNED CLIENTS ===");

    if (g_banned_count == 0) {
        mvwprintw(admin_main_win, y++, 2, "No banned clients.");
    } else {
        for (int i = 0; i < g_banned_count; i++) {
            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",
                    localtime(&g_banned_clients[i].banned_at));

            mvwprintw(admin_main_win, y++, 2, "%d. Key: %.16s... | Banned: %s | Reason: %s",
                     i + 1, g_banned_clients[i].session_key, time_str,
                     g_banned_clients[i].reason);
        }
    }

    mvwprintw(admin_main_win, y + 2, 2, "Press any key to return to main menu...");
    wnoutrefresh(admin_main_win);
    doupdate();
    getch();
}

static void show_connected_clients(void) {
    werase(admin_main_win);
    box(admin_main_win, 0, 0);

    int y = 2;
    mvwprintw(admin_main_win, y++, 2, "=== CONNECTED CLIENTS ===");

    // This would need to be populated from the main server
    // For now, show placeholder
    mvwprintw(admin_main_win, y++, 2, "Connected clients will be shown here...");
    mvwprintw(admin_main_win, y++, 2, "(Integration with main server needed)");

    mvwprintw(admin_main_win, y + 2, 2, "Press any key to return to main menu...");
    wnoutrefresh(admin_main_win);
    doupdate();
    getch();
}

static void ban_client_menu(void) {
    werase(admin_main_win);
    box(admin_main_win, 0, 0);

    int y = 2;
    mvwprintw(admin_main_win, y++, 2, "=== BAN CLIENT ===");
    mvwprintw(admin_main_win, y++, 2, "");
    mvwprintw(admin_main_win, y++, 2, "Enter session key to ban:");
    mvwprintw(admin_main_win, y++, 2, "Format: 64-character hex string");
    mvwprintw(admin_main_win, y++, 2, "");
    mvwprintw(admin_main_win, y++, 2, "Session Key: ______________________________");
    mvwprintw(admin_main_win, y++, 2, "Reason: __________________________________");

    wnoutrefresh(admin_main_win);
    doupdate();

    // Simple input handling (in real implementation, use forms)
    mvwprintw(admin_main_win, y - 2, 14, "");
    echo();
    curs_set(1);

    char session_key[65];
    char reason[256];

    wgetnstr(admin_main_win, session_key, 64);
    mvwprintw(admin_main_win, y - 1, 9, "");
    wgetnstr(admin_main_win, reason, 255);

    noecho();
    curs_set(0);

    if (strlen(session_key) == 64) {
        if (ban_client(session_key, reason) == 0) {
            draw_admin_status("Client banned successfully!");
        } else {
            draw_admin_status("Failed to ban client!");
        }
    } else {
        draw_admin_status("Invalid session key format!");
    }
}

static void unban_client_menu(void) {
    werase(admin_main_win);
    box(admin_main_win, 0, 0);

    int y = 2;
    mvwprintw(admin_main_win, y++, 2, "=== UNBAN CLIENT ===");
    mvwprintw(admin_main_win, y++, 2, "");
    mvwprintw(admin_main_win, y++, 2, "Enter session key to unban:");
    mvwprintw(admin_main_win, y++, 2, "");
    mvwprintw(admin_main_win, y++, 2, "Session Key: ______________________________");

    wnoutrefresh(admin_main_win);
    doupdate();

    echo();
    curs_set(1);
    mvwprintw(admin_main_win, y - 1, 14, "");

    char session_key[65];
    wgetnstr(admin_main_win, session_key, 64);

    noecho();
    curs_set(0);

    if (unban_client(session_key) == 0) {
        draw_admin_status("Client unbanned successfully!");
    } else {
        draw_admin_status("Client not found in banned list!");
    }
}

// Main admin loop
static void admin_main_loop(void) {
    int ch;

    while (!g_admin_shutdown) {
        draw_admin_header();
        draw_main_menu();
        draw_admin_status(NULL);
        draw_admin_footer();

        update_panels();
        doupdate();

        timeout(1000); // 1 second timeout
        ch = getch();

        if (ch == 'q' || ch == 'Q') {
            g_admin_shutdown = 1;
        } else if (ch == '1' || ch == 'v' || ch == 'V') {
            show_connected_clients();
        } else if (ch == '2' || ch == 'b' || ch == 'B') {
            ban_client_menu();
        } else if (ch == '3' || ch == 'u' || ch == 'U') {
            unban_client_menu();
        } else if (ch == '6') {
            show_banned_clients();
        }
        // Add more menu options as needed
    }
}

// Admin panel initialization
static int init_admin_panel(void) {
    // Initialize hash tables
    g_client_permissions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_fingerprint_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    // Load banned clients from file (if exists)
    FILE *fp = fopen("/tmp/banned_clients.dat", "rb");
    if (fp) {
        fread(&g_banned_count, sizeof(int), 1, fp);
        fread(g_banned_clients, sizeof(banned_client_t), g_banned_count, fp);
        fclose(fp);
    }

    admin_log("INFO", "Admin panel initialized");
    return 0;
}

// Save banned clients to file
static void save_banned_clients(void) {
    FILE *fp = fopen("/tmp/banned_clients.dat", "wb");
    if (fp) {
        fwrite(&g_banned_count, sizeof(int), 1, fp);
        fwrite(g_banned_clients, sizeof(banned_client_t), g_banned_count, fp);
        fclose(fp);
    }
}

// Cleanup admin panel
static void cleanup_admin_panel(void) {
    save_banned_clients();

    if (g_client_permissions) {
        g_hash_table_destroy(g_client_permissions);
    }
    if (g_fingerprint_cache) {
        g_hash_table_destroy(g_fingerprint_cache);
    }

    admin_log("INFO", "Admin panel shutdown");
}

// Signal handler for admin panel
static void admin_signal_handler(int sig) {
    g_admin_shutdown = 1;
}

// Main admin panel function
int run_admin_panel(void) {
    // Initialize libsodium if not already done
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return EXIT_FAILURE;
    }

    // Initialize admin panel
    if (init_admin_panel() != 0) {
        return EXIT_FAILURE;
    }

    // Set up signal handling
    signal(SIGINT, admin_signal_handler);
    signal(SIGTERM, admin_signal_handler);

    // Initialize UI
    init_admin_ui();

    // Main loop
    admin_main_loop();

    // Cleanup
    cleanup_admin_ui();
    cleanup_admin_panel();

    return EXIT_SUCCESS;
}

// Function to check if client is banned (called from main server)
int admin_is_client_banned(const char *session_key) {
    return is_client_banned(session_key);
}

// Function to get ban message for client
const char *admin_get_ban_message(const char *session_key) {
    for (int i = 0; i < g_banned_count; i++) {
        if (strcmp(g_banned_clients[i].session_key, session_key) == 0) {
            return "ИДИ НАХУЙ - You are banned from this server!";
        }
    }
    return NULL;
}
