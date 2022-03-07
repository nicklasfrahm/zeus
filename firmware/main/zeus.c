#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "http.h"
#include "net.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "update.h"

void app_main(void) {
  // Initialize non-volatile storage.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // The initialization may fail in one of two cases:
    //
    //   (1) OTA app partition table has a smaller NVS
    //       partition size than the non-OTA partition
    //       table.
    //   (2) NVS partition contains data in new format
    //       and cannot be recognized by this version
    //       of code.
    //
    // In either case we erase the NVM partition and
    // re-initialize it.
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // This must be done before registering any event
  // handlers, as for example via the ethernet driver.
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize TCP/IP network interface. Note, that
  // this may be called only once.
  ESP_ERROR_CHECK(esp_netif_init());

  // Set up an HTTP server to serve information about
  // the application and to expose metrics in a format
  // that can be scraped by Prometheus.
  ESP_ERROR_CHECK(http_server_init());

  // Install the ethernet driver and event handlers
  // for some informative logging. Do this after the
  // setup of the HTTP server as it will automatically
  // come online after the interface is up.
  ESP_ERROR_CHECK(net_eth_start());

  // Start thread to handle firmware updates automatically.
  ESP_ERROR_CHECK(update_init(5));
}
