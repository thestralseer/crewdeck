#ifndef AUTH_H
#define AUTH_H

#include "common.h"

typedef struct {
    int id;
    char email[128];
    char role[32];
    int employee_id;
    char name[128];
    char avatar_color[32];
} auth_user_t;

bool auth_hash_password(const char *password, char *hash_out, size_t hash_out_len);
bool auth_verify_password(const char *password, const char *hash);

bool auth_is_rate_limited(const char *ip);
void auth_record_login_attempt(const char *ip);
void auth_clear_login_attempts(const char *ip);

bool auth_create_session(int user_id, char *token_out, size_t token_out_len);
void auth_destroy_session(const char *token);
bool auth_validate_session(struct mg_http_message *hm, auth_user_t *user_out);

void handle_auth_login(struct mg_connection *c, struct mg_http_message *hm);
void handle_auth_logout(struct mg_connection *c, struct mg_http_message *hm);
void handle_auth_me(struct mg_connection *c, struct mg_http_message *hm);

#endif /* AUTH_H */
