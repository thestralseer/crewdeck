#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

typedef struct {
    int port;
    char db_path[256];
    char public_dir[256];
    char admin_email[128];
    char admin_password[128];
    time_t start_time;
} app_config_t;

extern app_config_t g_config;

void config_init(void);

#endif /* CONFIG_H */
