#ifndef NET_H
#define NET_H

#include <esp_err.h>

#define TAG_ETH "net: eth"
#define TAG_IP "net: ip"

// Configure and start the ethernet interface.
esp_err_t net_eth_start(void);

#endif
