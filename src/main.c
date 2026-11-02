#include "common.h"
#include "config.h"
#include "db.h"
#include "router.h"

static volatile int g_running = 1;

static void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        router_dispatch(c, hm);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize configuration */
    config_init();

    printf("\n");
    printf("  =======================================================\n");
    printf("   CREW / DECK - Production Employee Management System   \n");
    printf("   Version: %s | Platform: %s                    \n", APP_VERSION,
#ifdef _WIN32
           "Windows x64"
#else
           "Linux x86_64"
#endif
    );
    printf("  =======================================================\n\n");

    /* Initialize libsodium */
    if (sodium_init() < 0) {
        fprintf(stderr, "[FATAL] Failed to initialize libsodium cryptographic engine.\n");
        return 1;
    }
    printf("[INIT] Libsodium cryptographic engine initialized.\n");

    /* Initialize Database */
    printf("[INIT] Opening SQLite database at %s ...\n", g_config.db_path);
    if (!db_init(g_config.db_path)) {
        fprintf(stderr, "[FATAL] Failed to initialize SQLite database.\n");
        return 1;
    }
    printf("[INIT] SQLite database online and verified.\n");

    /* Initialize Router */
    router_init();

    /* Set signal handlers for graceful shutdown */
#ifndef _WIN32
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
#else
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
#endif

    /* Initialize Mongoose */
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    char listen_addr[64];
    snprintf(listen_addr, sizeof(listen_addr), "http://0.0.0.0:%d", g_config.port);

    struct mg_connection *c = mg_http_listen(&mgr, listen_addr, ev_handler, NULL);
    if (!c) {
        fprintf(stderr, "[FATAL] Failed to bind to %s. Port may already be in use.\n", listen_addr);
        db_close();
        mg_mgr_free(&mgr);
        return 1;
    }

    printf("[READY] CREW/DECK is listening on %s\n", listen_addr);
    printf("[INFO] Serving static files from: %s\n", g_config.public_dir);
    printf("[INFO] Health check available at: %s/healthz\n", listen_addr);
    printf("[INFO] Default admin: %s (password: %s)\n\n", g_config.admin_email, g_config.admin_password);

    /* Event Loop */
    while (g_running) {
        mg_mgr_poll(&mgr, 50);
    }

    printf("\n[SHUTDOWN] Gracefully terminating CREW/DECK server...\n");
    mg_mgr_free(&mgr);
    db_close();
    printf("[SHUTDOWN] Resources freed. Goodbye.\n");

    return 0;
}
