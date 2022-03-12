#ifndef NET_H
#define NET_H

#include "esp_err.h"

#define TAG_ETH "net.eth"
#define TAG_IP "net.ip"

/**
 * Configure and bring up the ethernet interface.
 *
 * @return ESP_OK if the interface can be configured successfully.
 */
esp_err_t net_eth_init(void);

#endif
