/**
 * Admin Panel Header
 * External interface for admin panel functionality
 */

#ifndef ADMIN_PANEL_H
#define ADMIN_PANEL_H

// Function declarations
int run_admin_panel(void);
int admin_is_client_banned(const char *session_key);
const char *admin_get_ban_message(const char *session_key);

#endif // ADMIN_PANEL_H
