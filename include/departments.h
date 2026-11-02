#ifndef DEPARTMENTS_H
#define DEPARTMENTS_H

#include "common.h"
#include "auth.h"

void handle_departments_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_department_create(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_department_get(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id);
void handle_department_update(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id);
void handle_department_delete(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id);

#endif /* DEPARTMENTS_H */
