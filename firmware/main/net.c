#include "net.h"

#include <math.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

// TODO: Abstract network interfaces.
// TODO: Signal online status.

static uint8_t netmask2prefix(const esp_ip4_addr_t *netmask) {
  return (uint8_t)round(log2(netmask->addr));
}

// Log ethernet status information.
static void net_eth_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data) {
  // Get the ethernet driver handle from the event data.
  esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
  uint8_t mac_addr[6] = {0};

  switch (event_id) {
    case ETHERNET_EVENT_CONNECTED: {
      esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
      ESP_LOGI(TAG_ETH, "Link up: %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0],
               mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
      break;
    }
    case ETHERNET_EVENT_DISCONNECTED: {
      ESP_LOGI(TAG_ETH, "Link down");
      break;
    }
    case ETHERNET_EVENT_START: {
      ESP_LOGI(TAG_ETH, "Started");
      break;
    }
    case ETHERNET_EVENT_STOP: {
      ESP_LOGI(TAG_ETH, "Stopped");
      break;
    }
    default: {
      break;
    }
  }
}

// Log IP status information.
static void net_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
  // Obtain information about the IP status.
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
  const esp_netif_ip_info_t *ip_info = &event->ip_info;

  // Convert netmask to CIDR prefix.
  uint8_t prefix = netmask2prefix(&ip_info->netmask);

  // Log information about the IP status.
  ESP_LOGI(TAG_IP, "Address: " IPSTR "/%d", IP2STR(&ip_info->ip), prefix);
  ESP_LOGI(TAG_IP, "Gateway: " IPSTR, IP2STR(&ip_info->gw));
}

esp_err_t net_eth_start(void) {
  // Prevent excessive logging.
  esp_log_level_set("system_api", ESP_LOG_WARN);
  esp_log_level_set("esp_eth.netif.netif_glue", ESP_LOG_WARN);
  esp_log_level_set("esp_netif_handlers", ESP_LOG_WARN);

  // Register event handlers for logging.
  ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                             &net_eth_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                             &net_ip_event_handler, NULL));

  // Create default network configuration for usage with ethernet interface.
  esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t *netif = esp_netif_new(&netif_cfg);

  // Configure physical ethernet, also known as PHY or OSI layer 1.
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = -1;
  phy_config.reset_gpio_num = GPIO_NUM_16;
  esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

  // Configure media access control, also known as MAC os OSI layer 2.
  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  esp32_emac_config.smi_mdc_gpio_num = GPIO_NUM_23;
  esp32_emac_config.smi_mdio_gpio_num = GPIO_NUM_18;
  esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);

  // Create link configuration by connecting MAC and PHY.
  esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);

  // Install the ethernet driver by applying the link configuration.
  static esp_eth_handle_t eth_handle = NULL;
  ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

  // Attach ethernet driver to IP network interface.
  esp_eth_netif_glue_handle_t eth_driver = esp_eth_new_netif_glue(eth_handle);
  ESP_ERROR_CHECK(esp_netif_attach(netif, eth_driver));

  // Start the ethernet driver state machine.
  ESP_ERROR_CHECK(esp_eth_start(eth_handle));

  return ESP_OK;
}
