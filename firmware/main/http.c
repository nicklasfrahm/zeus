#include "http.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_eth.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>

static httpd_handle_t http_server = NULL;

static esp_err_t http_send_json(httpd_req_t* req, cJSON* data) {
  // Set the content type to JSON.
  httpd_resp_set_type(req, "application/json");

  // Encode JSON object to string and send it.
  httpd_resp_send(req, cJSON_Print(data), HTTPD_RESP_USE_STRLEN);

  return ESP_OK;
};

static esp_err_t health_list_endpoint(httpd_req_t* req) {
  // Fetch information about currently running firmware.
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_app_desc_t running_app_info;
  if (esp_ota_get_partition_description(running, &running_app_info) != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    // TODO: Create more descriptive error response!
    return http_send_json(req, NULL);
  }

  cJSON* firmware = cJSON_CreateObject();
  cJSON_AddStringToObject(firmware, "version", running_app_info.version);
  cJSON_AddStringToObject(firmware, "sdk", running_app_info.idf_ver);

  // Concatenate compile data and time to timestamp.
  int timestamp_len =
      sizeof(running_app_info.date) + sizeof(running_app_info.time) + 2;
  char timestamp[timestamp_len];
  snprintf(timestamp, timestamp_len, "%s %s", running_app_info.date,
           running_app_info.time);
  cJSON_AddStringToObject(firmware, "timestamp", timestamp);

  // Create SHA256 hash string.
  int sha256_len = sizeof(running_app_info.app_elf_sha256);
  char sha256[sha256_len * 2 + 1];
  sha256[sha256_len * 2] = 0;
  for (int i = 0; i < sha256_len; ++i) {
    sprintf(&sha256[i * 2], "%02x", running_app_info.app_elf_sha256[i]);
  }
  cJSON_AddStringToObject(firmware, "sha256", sha256);

  cJSON* data = cJSON_CreateObject();
  cJSON_AddItemReferenceToObject(data, "firmware", firmware);

  cJSON* response = cJSON_CreateObject();
  cJSON_AddItemReferenceToObject(response, "data", data);

  // // Send the response including both, the headers and the body.
  // const char* resp_str = (const char*)"{\"hello\":\"world\"}";
  // httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

  return http_send_json(req, response);
}

static const httpd_uri_t health_list = {
    .method = HTTP_GET,
    .uri = "/health",
    .handler = health_list_endpoint,
};

static httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

  // Start the httpd server.
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGI(TAG_HTTP_SERVER, "failed to start server");
    return NULL;
  }

  // Configure application endpoints.
  httpd_register_uri_handler(server, &health_list);
  ESP_LOGI(TAG_HTTP_SERVER, "listening on 0.0.0.0:%d", config.server_port);
  return server;
}

static void stop_webserver(httpd_handle_t server) {
  // Stop the httpd server
  httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
  httpd_handle_t* server = (httpd_handle_t*)arg;
  if (*server) {
    ESP_LOGI(TAG_HTTP_SERVER, "stopping");
    stop_webserver(*server);
    *server = NULL;
  }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data) {
  httpd_handle_t* server = (httpd_handle_t*)arg;
  if (*server == NULL) {
    ESP_LOGI(TAG_HTTP_SERVER, "starting");
    *server = start_webserver();
  }
}

esp_err_t http_server_init(void) {
  // Start and stop the HTTP server based on the network connection status.
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                             &connect_handler, &http_server));
  ESP_ERROR_CHECK(
      esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED,
                                 &disconnect_handler, &http_server));
  return ESP_OK;
}
