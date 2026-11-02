#ifndef LEAVE_H
#define LEAVE_H

#include "common.h"
#include "auth.h"

void handle_leave_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_leave_create(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_leave_review(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int request_id);
void handle_leave_balance(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int employee_id);

#endif /* LEAVE_H */
