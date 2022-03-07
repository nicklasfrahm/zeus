#ifndef HTTP_H
#define HTTP_H

#include "esp_err.h"

// Configure and start the HTTP server.
esp_err_t http_server_init(void);

/**
 * Get the user agent header for this application, including the name of the
 * application, its version and a link its Git repository.
 *
 * @return A heap-allocated string. WATCH OUT FOR MEMORY LEAKS!
 */
char* http_user_agent(void);

#endif
