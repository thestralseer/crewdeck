#ifndef AUDIT_H
#define AUDIT_H

#include "common.h"
#include "auth.h"

void audit_log_record(int user_id, const char *user_email, const char *action,
                      const char *entity_type, int entity_id,
                      const char *details, const char *ip);

void handle_audit_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);

#endif /* AUDIT_H */
