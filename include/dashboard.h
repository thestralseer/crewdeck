#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "common.h"
#include "auth.h"

void handle_dashboard_metrics(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_org_constellation(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);

#endif /* DASHBOARD_H */
