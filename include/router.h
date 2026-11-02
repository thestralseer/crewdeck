#ifndef ROUTER_H
#define ROUTER_H

#include "common.h"

void router_init(void);
void router_dispatch(struct mg_connection *c, struct mg_http_message *hm);

#endif /* ROUTER_H */
