#ifndef UPDATE_H
#define UPDATE_H

#include <stdint.h>

#include "esp_err.h"

/**
 * Create a background thread that will periodically check for a
 * new firmware.
 *
 * @param[in] interval_mins Number of minutes between checks.
 *
 * @returns ESP_OK or ESP_ERR_INVALID_STATE if the thread can't be started.
 */
esp_err_t update_init(uint32_t interval_mins);

/**
 * Perform a firmware update or block thread until a firmware update may be
 * performed. This function is thread-safe and may also be used to manually
 * trigger a firmware update.
 *
 * @return ESP_OK if the update succeeds.
 */
esp_err_t update_lock(void);

/**
 * Perform a firmware update or return if the lock can't be obtained
 * immediately.This function is thread-safe and may also be used to manually
 * trigger a firmware update.
 *
 * @return ESP_FAIL if the lock can't be obtained or ESP_OK if the update
 * succeeds.
 */
esp_err_t update_trylock(void);

#endif
