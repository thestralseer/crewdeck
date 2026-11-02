#include "auth.h"
#include "db.h"
#include "util.h"
#include "audit.h"

bool auth_hash_password(const char *password, char *hash_out, size_t hash_out_len) {
    if (!password || !hash_out || hash_out_len < crypto_pwhash_STRBYTES) return false;
    if (crypto_pwhash_str(hash_out, password, strlen(password),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        return false;
    }
    return true;
}

bool auth_verify_password(const char *password, const char *hash) {
    if (!password || !hash) return false;
    if (crypto_pwhash_str_verify(hash, password, strlen(password)) == 0) {
        return true;
    }
    return false;
}

bool auth_is_rate_limited(const char *ip) {
    if (!ip) return false;
    sqlite3 *db = db_get_handle();
    if (!db) return false;

    time_t cutoff = time(NULL) - LOGIN_WINDOW_SECONDS;
    const char *sql = "SELECT COUNT(*) FROM login_attempts WHERE ip = ? AND attempt_time > ?;";
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ip, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)cutoff);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count >= MAX_LOGIN_ATTEMPTS;
}

void auth_record_login_attempt(const char *ip) {
    if (!ip) return;
    sqlite3 *db = db_get_handle();
    if (!db) return;

    time_t now = time(NULL);
    const char *sql = "INSERT INTO login_attempts (ip, attempt_time) VALUES (?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ip, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* Periodic cleanup of old attempts */
    time_t old_cutoff = now - 3600;
    const char *cleanup_sql = "DELETE FROM login_attempts WHERE attempt_time < ?;";
    if (sqlite3_prepare_v2(db, cleanup_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)old_cutoff);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void auth_clear_login_attempts(const char *ip) {
    if (!ip) return;
    sqlite3 *db = db_get_handle();
    if (!db) return;

    const char *sql = "DELETE FROM login_attempts WHERE ip = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ip, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

bool auth_create_session(int user_id, char *token_out, size_t token_out_len) {
    if (user_id <= 0 || !token_out || token_out_len < 65) return false;
    sqlite3 *db = db_get_handle();
    if (!db) return false;

    util_generate_token(token_out, token_out_len);
    time_t expires_at = time(NULL) + SESSION_TTL_SECONDS;
    char now_iso[32];
    util_now_iso8601(now_iso, sizeof(now_iso));

    const char *sql = "INSERT INTO sessions (token, user_id, expires_at, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, token_out, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, user_id);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)expires_at);
        sqlite3_bind_text(stmt, 4, now_iso, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = true;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

void auth_destroy_session(const char *token) {
    if (!token || !*token) return;
    sqlite3 *db = db_get_handle();
    if (!db) return;

    const char *sql = "DELETE FROM sessions WHERE token = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

bool auth_validate_session(struct mg_http_message *hm, auth_user_t *user_out) {
    if (!hm || !user_out) return false;
    memset(user_out, 0, sizeof(*user_out));

    char token[128];
    if (!util_get_cookie(hm, SESSION_COOKIE_NAME, token, sizeof(token))) {
        return false;
    }

    sqlite3 *db = db_get_handle();
    if (!db) return false;

    time_t now = time(NULL);
    const char *sql =
        "SELECT u.id, u.email, u.role, u.employee_id, "
        "       COALESCE(e.name, 'Admin'), COALESCE(e.avatar_color, '#FFB020')\n"
        "FROM sessions s\n"
        "JOIN users u ON s.user_id = u.id\n"
        "LEFT JOIN employees e ON u.employee_id = e.id\n"
        "WHERE s.token = ? AND s.expires_at > ?;";

    sqlite3_stmt *stmt = NULL;
    bool found = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)now);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user_out->id = sqlite3_column_int(stmt, 0);
            const char *email = (const char *)sqlite3_column_text(stmt, 1);
            const char *role = (const char *)sqlite3_column_text(stmt, 2);
            int emp_id = sqlite3_column_type(stmt, 3) != SQLITE_NULL ? sqlite3_column_int(stmt, 3) : 0;
            const char *name = (const char *)sqlite3_column_text(stmt, 4);
            const char *color = (const char *)sqlite3_column_text(stmt, 5);

            if (email) snprintf(user_out->email, sizeof(user_out->email), "%s", email);
            if (role) snprintf(user_out->role, sizeof(user_out->role), "%s", role);
            user_out->employee_id = emp_id;
            if (name) snprintf(user_out->name, sizeof(user_out->name), "%s", name);
            if (color) snprintf(user_out->avatar_color, sizeof(user_out->avatar_color), "%s", color);
            found = true;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

void handle_auth_login(struct mg_connection *c, struct mg_http_message *hm) {
    char ip[64];
    util_get_client_ip(c, hm, ip, sizeof(ip));

    if (auth_is_rate_limited(ip)) {
        util_json_error(c, 429, "RATE_LIMITED", "Too many failed login attempts. Please wait 60 seconds.");
        return;
    }

    cJSON *body = util_parse_json(hm);
    if (!body) {
        util_json_error(c, 400, "INVALID_JSON", "Request body must be valid JSON");
        return;
    }

    cJSON *email_item = cJSON_GetObjectItemCaseSensitive(body, "email");
    cJSON *pass_item = cJSON_GetObjectItemCaseSensitive(body, "password");

    if (!cJSON_IsString(email_item) || !cJSON_IsString(pass_item) ||
        !email_item->valuestring || !pass_item->valuestring) {
        cJSON_Delete(body);
        util_json_error(c, 400, "VALIDATION_ERROR", "Email and password are required");
        return;
    }

    const char *email = email_item->valuestring;
    const char *password = pass_item->valuestring;

    sqlite3 *db = db_get_handle();
    if (!db) {
        cJSON_Delete(body);
        util_json_error(c, 500, "DB_ERROR", "Database unavailable");
        return;
    }

    const char *sql =
        "SELECT u.id, u.email, u.password_hash, u.role, u.employee_id, "
        "       COALESCE(e.name, 'Admin'), COALESCE(e.avatar_color, '#FFB020')\n"
        "FROM users u\n"
        "LEFT JOIN employees e ON u.employee_id = e.id\n"
        "WHERE u.email = ? COLLATE NOCASE;";

    sqlite3_stmt *stmt = NULL;
    bool authed = false;
    int user_id = 0;
    char db_email[128] = {0};
    char db_role[32] = {0};
    int db_emp_id = 0;
    char db_name[128] = {0};
    char db_color[32] = {0};

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user_id = sqlite3_column_int(stmt, 0);
            const char *row_email = (const char *)sqlite3_column_text(stmt, 1);
            const char *row_hash = (const char *)sqlite3_column_text(stmt, 2);
            const char *row_role = (const char *)sqlite3_column_text(stmt, 3);
            int row_emp = sqlite3_column_type(stmt, 4) != SQLITE_NULL ? sqlite3_column_int(stmt, 4) : 0;
            const char *row_name = (const char *)sqlite3_column_text(stmt, 5);
            const char *row_color = (const char *)sqlite3_column_text(stmt, 6);

            if (row_hash && auth_verify_password(password, row_hash)) {
                authed = true;
                if (row_email) snprintf(db_email, sizeof(db_email), "%s", row_email);
                if (row_role) snprintf(db_role, sizeof(db_role), "%s", row_role);
                db_emp_id = row_emp;
                if (row_name) snprintf(db_name, sizeof(db_name), "%s", row_name);
                if (row_color) snprintf(db_color, sizeof(db_color), "%s", row_color);
            }
        }
        sqlite3_finalize(stmt);
    }

    cJSON_Delete(body);

    if (!authed) {
        auth_record_login_attempt(ip);
        util_json_error(c, 401, "INVALID_CREDENTIALS", "Invalid email or password");
        return;
    }

    /* Clear failed attempts on successful login */
    auth_clear_login_attempts(ip);

    char token[128];
    if (!auth_create_session(user_id, token, sizeof(token))) {
        util_json_error(c, 500, "SESSION_ERROR", "Failed to create user session");
        return;
    }

    audit_log_record(user_id, db_email, "LOGIN", "auth", user_id, "User authenticated successfully", ip);

    /* Construct JSON response and Set-Cookie */
    cJSON *res = cJSON_CreateObject();
    cJSON *u_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(u_obj, "id", user_id);
    cJSON_AddStringToObject(u_obj, "email", db_email);
    cJSON_AddStringToObject(u_obj, "role", db_role);
    cJSON_AddNumberToObject(u_obj, "employee_id", db_emp_id);
    cJSON_AddStringToObject(u_obj, "name", db_name);
    cJSON_AddStringToObject(u_obj, "avatar_color", db_color);
    cJSON_AddItemToObject(res, "user", u_obj);

    char *json_str = cJSON_PrintUnformatted(res);
    cJSON_Delete(res);

    char headers[512];
    snprintf(headers, sizeof(headers),
             "Content-Type: application/json\r\n"
             "Set-Cookie: %s=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=%d\r\n"
             "Cache-Control: no-store\r\n",
             SESSION_COOKIE_NAME, token, SESSION_TTL_SECONDS);

    mg_http_reply(c, 200, headers, "%s", json_str ? json_str : "{}");
    if (json_str) free(json_str);
}

void handle_auth_logout(struct mg_connection *c, struct mg_http_message *hm) {
    char token[128];
    auth_user_t user;
    if (auth_validate_session(hm, &user)) {
        char ip[64];
        util_get_client_ip(c, hm, ip, sizeof(ip));
        audit_log_record(user.id, user.email, "LOGOUT", "auth", user.id, "User logged out", ip);
    }
    if (util_get_cookie(hm, SESSION_COOKIE_NAME, token, sizeof(token))) {
        auth_destroy_session(token);
    }

    char headers[256];
    snprintf(headers, sizeof(headers),
             "Content-Type: application/json\r\n"
             "Set-Cookie: %s=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0\r\n"
             "Cache-Control: no-store\r\n",
             SESSION_COOKIE_NAME);

    mg_http_reply(c, 200, headers, "{\"message\":\"Logged out successfully\"}");
}

void handle_auth_me(struct mg_connection *c, struct mg_http_message *hm) {
    auth_user_t user;
    if (!auth_validate_session(hm, &user)) {
        util_json_error(c, 401, "UNAUTHORIZED", "Active session required");
        return;
    }

    cJSON *res = cJSON_CreateObject();
    cJSON *u_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(u_obj, "id", user.id);
    cJSON_AddStringToObject(u_obj, "email", user.email);
    cJSON_AddStringToObject(u_obj, "role", user.role);
    cJSON_AddNumberToObject(u_obj, "employee_id", user.employee_id);
    cJSON_AddStringToObject(u_obj, "name", user.name);
    cJSON_AddStringToObject(u_obj, "avatar_color", user.avatar_color);
    cJSON_AddItemToObject(res, "user", u_obj);

    util_json_response(c, 200, res);
}
