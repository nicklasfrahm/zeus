#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net.h"

#define FIRMWARE_URL                                               \
  "https://github.com/nicklasfrahm/zeus/releases/latest/download/" \
  "zeus-esp32.bin"

void app_main(void) {
  // Create the default event loop that is running in background.
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  // Configure and start the ethernet IP stack.
  ESP_ERROR_CHECK(net_eth_configure_and_start());

  // TODO: Set up OTA update thread.
  printf("Hello World!\n");

  while (1) {
    // Do nothing without entirely blocking all other threads.
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
