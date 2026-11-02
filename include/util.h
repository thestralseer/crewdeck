#ifndef UTIL_H
#define UTIL_H

#include "common.h"

void util_json_error(struct mg_connection *c, int status_code, const char *code, const char *message);
void util_json_response(struct mg_connection *c, int status_code, cJSON *json);
void util_json_string(struct mg_connection *c, int status_code, const char *json_str);
cJSON *util_parse_json(struct mg_http_message *hm);

bool util_get_cookie(struct mg_http_message *hm, const char *name, char *dest, size_t dest_len);
bool util_get_query_param(struct mg_http_message *hm, const char *name, char *dest, size_t dest_len);
bool util_get_header(struct mg_http_message *hm, const char *name, char *dest, size_t dest_len);
void util_get_client_ip(struct mg_connection *c, struct mg_http_message *hm, char *dest, size_t dest_len);

void util_now_iso8601(char *dest, size_t dest_len);
void util_today_date(char *dest, size_t dest_len);
void util_now_time(char *dest, size_t dest_len);
void util_generate_token(char *dest, size_t dest_len);
size_t util_url_decode(const char *src, size_t src_len, char *dst, size_t dst_len);
bool util_get_path_id(const char *uri, const char *prefix, int *out_id);
bool util_is_valid_email(const char *email);
bool util_is_valid_date(const char *date_str);

#endif /* UTIL_H */
