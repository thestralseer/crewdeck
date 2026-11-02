#ifndef EMPLOYEES_H
#define EMPLOYEES_H

#include "common.h"
#include "auth.h"

void handle_employees_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_employee_create(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_employee_get(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id);
void handle_employee_update(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id);
void handle_employee_delete(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int id);

#endif /* EMPLOYEES_H */
