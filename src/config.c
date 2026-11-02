#include "config.h"

app_config_t g_config;

void config_init(void) {
    memset(&g_config, 0, sizeof(g_config));
    g_config.start_time = time(NULL);

    const char *port_str = getenv("PORT");
    if (port_str && *port_str) {
        g_config.port = atoi(port_str);
        if (g_config.port <= 0 || g_config.port > 65535) {
            g_config.port = DEFAULT_PORT;
        }
    } else {
        g_config.port = DEFAULT_PORT;
    }

    const char *db_str = getenv("DB_PATH");
    if (db_str && *db_str) {
        snprintf(g_config.db_path, sizeof(g_config.db_path), "%s", db_str);
    } else {
        snprintf(g_config.db_path, sizeof(g_config.db_path), "%s", DEFAULT_DB_PATH);
    }

    const char *pub_str = getenv("PUBLIC_DIR");
    if (pub_str && *pub_str) {
        snprintf(g_config.public_dir, sizeof(g_config.public_dir), "%s", pub_str);
    } else {
        snprintf(g_config.public_dir, sizeof(g_config.public_dir), "%s", DEFAULT_PUBLIC_DIR);
    }

    const char *admin_email = getenv("ADMIN_EMAIL");
    if (admin_email && *admin_email) {
        snprintf(g_config.admin_email, sizeof(g_config.admin_email), "%s", admin_email);
    } else {
        snprintf(g_config.admin_email, sizeof(g_config.admin_email), "admin@crewdeck.io");
    }

    const char *admin_pass = getenv("ADMIN_PASSWORD");
    if (admin_pass && *admin_pass) {
        snprintf(g_config.admin_password, sizeof(g_config.admin_password), "%s", admin_pass);
    } else {
        snprintf(g_config.admin_password, sizeof(g_config.admin_password), "admin123");
    }
}
