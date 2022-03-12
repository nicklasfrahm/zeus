#include "update.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_pthread.h"
#include "esp_tls.h"
#include "git.h"
#include "http.h"
#include "semver.h"

// Log prefix to be used.
#define TAG "update"
// Size of the buffer used to write the OTA data to the flash.
#define BUFFER_SIZE 2048

// Update channel, which may either be "latest" or an existing Git tag.
static char* channel = "latest";
// Name of the binary file.
static const char firmware[] = "zeus-esp32.bin";
// Gives access to the current firmware update process.
static esp_ota_handle_t update_handle = 0;
// Protects access to shared resources, such as the receive buffer.
static pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;
// Gives access to the update thread.
static pthread_t thread_handle;

/**
 * Retrieve partitioning information. Check whether the configured boot
 * partition is currently running. Also verify that the partitioning scheme
 * supports OTA updates.
 *
 * @return ESP_OK or ESP_ERR_OTA_SELECT_INFO_INVALID if the partitioning scheme
 * does not support OTA updates.
 */
static esp_err_t update_check_preflight(void) {
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_partition_t* running = esp_ota_get_running_partition();

  if (boot != running) {
    ESP_LOGW(TAG, "Configured OTA boot partition at offset: 0x%08x",
             boot->address);
    ESP_LOGW(TAG, "Running OTA boot partition at offset: 0x%08x",
             running->address);
    ESP_LOGW(TAG, "Your configured boot partition does not match your");
    ESP_LOGW(TAG, "running partition. Your update configuration data");
    ESP_LOGW(TAG, "or your preferred boot image may be corrupted.");
  }
  ESP_LOGI(TAG, "Running partition: %s", running->label);

  const esp_partition_t* update = esp_ota_get_next_update_partition(NULL);
  if (update == NULL) {
    // This error occurs if the device is not partitioned for OTA updates.
    // Usually this is a consequence of a user manually modifying the
    // partitioning scheme.
    ESP_LOGE(TAG,
             "Failed to get update partition: "
             "Unsupported partitioning scheme");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Update partition: %s", update->label);

  return ESP_OK;
}

/**
 * Extract and validate the running firmware image header by checking if we can
 * obtain its firmware information.
 *
 * @param [out] info_running An app description pointer for the running
 * firmware.
 *
 * @return ESP_OK if the check succeeds, otherwise ESP_FAIL.
 */
static esp_err_t update_check_running_header(esp_app_desc_t* info_running) {
  const esp_partition_t* part_running = esp_ota_get_running_partition();

  esp_err_t err = esp_ota_get_partition_description(part_running, info_running);
  if (err != ESP_OK) {
    ESP_LOGE(TAG,
             "Failed to read firware version: "
             "No app info for running firmware");
    return err;
  }
  ESP_LOGI(TAG, "Running firmware: %s", info_running->version);

  return ESP_OK;
}

/**
 * Extract and validate the update firmware image header by checking if the
 * new firmware has previously caused an update to fail.
 *
 * @param[in] buffer A buffer holding the header of the firmware.
 * @param[out] info_update An app description pointer for the update.
 *
 * @return ESP_OK if the check succeeds, otherwise ESP_FAIL.
 */
static esp_err_t update_check_update_header(const char buffer[],
                                            esp_app_desc_t* info_update) {
  // Extract the firmware image header from the downloaded firmware.
  size_t offset =
      sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
  memcpy(info_update, &buffer[offset], sizeof(esp_app_desc_t));
  ESP_LOGI(TAG, "New firmware: %s", info_update->version);

  const esp_partition_t* part_failed = esp_ota_get_last_invalid_partition();
  if (part_failed == NULL) {
    return ESP_OK;
  }

  esp_app_desc_t info_failed;
  esp_err_t err = esp_ota_get_partition_description(part_failed, &info_failed);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Failed firmware: %s", info_failed.version);
  }

  // Check if this particular firmware we are attempting to installed failed
  // during a previous installation attempt.
  if (strcmp(info_failed.version, info_update->version) == 0) {
    ESP_LOGW(TAG, "The new firmware version is the same as the last failed");
    ESP_LOGW(TAG, "version. This means that previously there was an attempt");
    ESP_LOGW(TAG, "to launch the new firmware which failed. The update has");
    ESP_LOGW(TAG, "been rolled back to the previous version. Skipping update");
    ESP_LOGW(TAG, "to new, unstable firmware.");
    return ESP_FAIL;
  }

  return ESP_OK;
}

/**
 * Process the firmware update. Please note that this function is NOT
 * thread-safe. This function is only intended for internal use.
 *
 * @param[in] firmware_url URL to the firmware image.
 * @param[in] user_agent Desired content of the HTTP User-Agent header.
 *
 * @return ESP_OK if the operation succeeds.
 */
static esp_err_t update_execute(const char* firmware_url,
                                const char* user_agent) {
  esp_err_t err = update_check_preflight();
  if (err != ESP_OK) {
    return err;
  }

  esp_http_client_config_t config = {
      .url = firmware_url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 10 * 1000,
      .keep_alive_enable = true,
      .user_agent = user_agent,
      .buffer_size = BUFFER_SIZE,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL) {
    ESP_LOGE(TAG, "Failed to configure HTTP client");
    return ESP_FAIL;
  }

  // Follow redirects until resource is found.
  int32_t status = 0;
  do {
    char url[512];
    esp_http_client_get_url(client, url, 512);
    ESP_LOGW(TAG, "Request: %s", url);

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
      esp_http_client_cleanup(client);
      return err;
    }
    esp_http_client_fetch_headers(client);

    status = esp_http_client_get_status_code(client);
    ESP_LOGW(TAG, "Status: %d", status);

    if (http_is_redirect(status)) {
      status = 0;
      esp_http_client_set_redirection(client);
    }
  } while (status == 0);

  // Handle for update partition.
  const esp_partition_t* part = esp_ota_get_next_update_partition(NULL);
  // Receive buffer for the firmware update.
  char buffer[BUFFER_SIZE + 1] = {0};
  // Count the image length.
  int32_t image_length = 0;
  // Flag whether image header was checked and firmware update was started.
  bool image_header_checked = false;
  while (1) {
    int32_t bytes_read = esp_http_client_read(client, buffer, BUFFER_SIZE);

    // NOTE: I am aware that the arrangement of the `if` statements here adds
    // runtime overhead. I have deliberately chosen to structure the code like
    // this to make it more readable as I find nested `if else` statements hard
    // to comprehend. As for runtime performance optimization, remember that
    // "premature optimization is the root of all evil"!

    if (bytes_read < 0) {
      ESP_LOGE(TAG, "Failed to read TLS data");
      esp_http_client_cleanup(client);
      return ESP_FAIL;
    }

    if (bytes_read > 0) {
      if (image_header_checked == false) {
        size_t app_info_offset = sizeof(esp_image_header_t) +
                                 sizeof(esp_image_segment_header_t) +
                                 sizeof(esp_app_desc_t);

        if (bytes_read <= app_info_offset) {
          ESP_LOGE(TAG,
                   "Failed to download firmware: "
                   "Firmware image incomplete");
          esp_http_client_cleanup(client);
          return ESP_FAIL;
        }

        // Fetch the firmware image header of the currently running firmware.
        esp_app_desc_t info_running;
        err = update_check_running_header(&info_running);
        if (err != ESP_OK) {
          esp_http_client_cleanup(client);
          return err;
        }

        // Check if a previous update for this firmware failed.
        esp_app_desc_t info_update;
        err = update_check_update_header(buffer, &info_update);
        if (err != ESP_OK) {
          esp_http_client_cleanup(client);
          return err;
        }
        image_header_checked = true;

        // Update direction, where 1 is upgrade, -1 is downgrade and 0 is no
        // change.
        uint8_t dir = semver_compare(info_update.version, info_running.version);
        bool is_latest = strcmp(channel, "latest") == 0 ? true : false;

        // If the channel is a specific version, we allow users to up- or
        // downgrade the firmware to a specific version. If the channel is
        // latest in contrast, we only allow firmware upgrades.
        if ((!is_latest && dir == 0) || (is_latest && dir <= 0)) {
          esp_http_client_cleanup(client);
          // It's okay if the firmware is already up-to-date,
          // we don't consider this an error.
          return ESP_OK;
        }

        err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "Failed to start update: %s", esp_err_to_name(err));
          esp_http_client_cleanup(client);
          esp_ota_abort(update_handle);
          return err;
        }
        ESP_LOGI(TAG, "Successfully started update: %s", info_update.version);
      }

      err = esp_ota_write(update_handle, (const void*)buffer, bytes_read);
      if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        esp_ota_abort(update_handle);
        return err;
      }

      image_length += bytes_read;
      ESP_LOGD(TAG, "Written image: %d B", image_length);
    }

    if (bytes_read == 0) {
      // As esp_http_client_read never returns a negative error code. We rely on
      // `errno` to check for underlying transport connectivity closure, if any.
      if (errno == ECONNRESET || errno == ENOTCONN) {
        char* err_name = strerror(errno);

        ESP_LOGE(TAG, "Failed to receive update");
        ESP_LOGE(TAG, "Connection closed prematurely: %s", err_name);

        free(err_name);

        esp_http_client_cleanup(client);
        esp_ota_abort(update_handle);

        return ESP_ERR_HTTP_CONNECTION_CLOSED;
      }

      if (esp_http_client_is_complete_data_received(client) == true) {
        ESP_LOGI(TAG, "Received new firmware image: %d B", image_length);
        break;
      }
    }
  }

  // End update by verifying firmware image.
  err = esp_ota_end(update_handle);
  if (err != ESP_OK) {
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      ESP_LOGE(TAG, "Failed to validate update: Checksum mismatch");
    } else {
      ESP_LOGE(TAG, "Failed to finish update: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return err;
  }

  // Configure the new boot partition.
  err = esp_ota_set_boot_partition(part);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return err;
  }

  ESP_LOGI(TAG, "Restarting system ...");
  esp_restart();

  return ESP_OK;
};

esp_err_t update_lock(void) {
  pthread_mutex_lock(&update_mutex);

  char* firmware_url = git_release_download_url(channel, firmware);
  char* user_agent = http_user_agent();

  esp_err_t err = update_execute(firmware_url, user_agent);

  free(user_agent);
  free(firmware_url);

  pthread_mutex_unlock(&update_mutex);

  return err;
}

esp_err_t update_trylock(void) {
  if (pthread_mutex_trylock(&update_mutex) != 0) {
    return ESP_FAIL;
  }

  char* firmware_url = git_release_download_url(channel, firmware);
  char* user_agent = http_user_agent();

  esp_err_t err = update_execute(firmware_url, user_agent);

  free(user_agent);
  free(firmware_url);

  pthread_mutex_unlock(&update_mutex);

  return err;
}

static void* update_thread(void* arg) {
  uint32_t* interval_mins_ptr = (uint32_t*)arg;
  uint32_t interval_ms = *interval_mins_ptr * 60000;

  while (1) {
    update_lock();

    // Suspend thread until the next update check.
    sleep(interval_ms);
  }

  return NULL;
}

esp_err_t update_init(uint32_t interval_mins) {
  // TODO: Remove this and use events instead!
  // Delay start of update thread to allow internet connection to be established
  // first.
  sleep(10);

  // Start thread for automatic updates.
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 32 * 1024);
  pthread_create(&thread_handle, &attr, update_thread, (void*)&interval_mins);

  return ESP_OK;
}
