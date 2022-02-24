#include <stdio.h>

#include "esp_log.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
  // Create the default event loop that is running in background.
  // ESP_ERROR_CHECK(esp_event_loop_create_default());
  
  while (1) {
    printf("Hello World!\n");

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}