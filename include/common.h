#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <unistd.h>
  #include <signal.h>
  #include <sys/time.h>
#endif

#include "../vendor/mongoose/mongoose.h"
#include "../vendor/sqlite3/sqlite3.h"
#include "../vendor/cjson/cJSON.h"

#define SODIUM_STATIC 1
#include "../vendor/libsodium/include/sodium.h"

#define APP_VERSION "1.0.0"
#define DEFAULT_PORT 8080
#define DEFAULT_DB_PATH "./data/crewdeck.db"
#define DEFAULT_PUBLIC_DIR "./public"
#define SESSION_COOKIE_NAME "crew_session"
#define SESSION_TTL_SECONDS 86400  /* 24 hours */
#define MAX_LOGIN_ATTEMPTS 5
#define LOGIN_WINDOW_SECONDS 60

#endif /* COMMON_H */
