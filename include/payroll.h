#ifndef PAYROLL_H
#define PAYROLL_H

#include "common.h"
#include "auth.h"

void handle_payroll_summary(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_payroll_export(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);

#endif /* PAYROLL_H */
