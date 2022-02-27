#ifndef HTTP_H
#define HTTP_H

#include <esp_err.h>

#define TAG_HTTP_SERVER "http: server"

// Configure and start the HTTP server.
esp_err_t http_server_init(void);

#endif
