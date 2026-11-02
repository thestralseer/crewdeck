#ifndef ATTENDANCE_H
#define ATTENDANCE_H

#include "common.h"
#include "auth.h"

void handle_attendance_list(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_attendance_checkin(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_attendance_checkout(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user);
void handle_attendance_heatmap(struct mg_connection *c, struct mg_http_message *hm, const auth_user_t *user, int employee_id);

#endif /* ATTENDANCE_H */
