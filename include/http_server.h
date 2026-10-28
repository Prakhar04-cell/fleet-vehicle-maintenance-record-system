#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "common.h"

#define DEFAULT_HTTP_PORT 8080

StatusCode http_server_start(int port);
void http_server_stop(void);
bool http_server_is_running(void);

#endif /* HTTP_SERVER_H */
