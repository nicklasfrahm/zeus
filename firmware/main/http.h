#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>

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

/**
 * Check whether an HTTP status code indicates a redirect.
 *
 * @param status An HTTP status code.
 *
 * @return true if the status indicates a redirect.
 */
bool http_is_redirect(int32_t status);

#endif
