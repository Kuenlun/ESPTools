#include <ESPTools/logger.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sys.h>

#include "secrets.h"

#define EXAMPLE_ESP_MAXIMUM_RETRY 5

#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// Tag used for the logging system
static constexpr char LOG_TAG[]{"WiFi"};

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
  static int s_retry_num = 0;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
    {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGD(LOG_TAG, "retry to connect to the AP");
    }
    else
    {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGW(LOG_TAG, "connect to the AP fail");
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(LOG_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init_sta(void)
{

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {
          /**< SSID of target AP. */
          .ssid = {EXAMPLE_ESP_WIFI_SSID},
          /**< Password of target AP. */
          .password = {EXAMPLE_ESP_WIFI_PASS},
          /**< do all channel scan or fast scan */
          .scan_method = WIFI_ALL_CHANNEL_SCAN,
          /**< whether set MAC address of target AP or not. Generally, station_config.bssid_set needs to be 0; and it needs to be 1 only when users need to check the MAC address of the AP.*/
          .bssid_set = false,
          /**< MAC address of target AP */
          .bssid = {0}, // Uncomment and set the MAC address if needed.
          /**< channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0. */
          .channel = 0,
          /**< Listen interval for ESP32 station to receive beacon when WIFI_PS_MAX_MODEM is set. Units: AP beacon intervals. Defaults to 3 if set to 0. */
          .listen_interval = 3,
          /**< sort the connect AP in the list by rssi or security mode */
          .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
          /**< When sort_method is set, only APs which have an auth mode that is more secure than the selected auth mode and a signal stronger than the minimum RSSI will be used. */
          .threshold = {
              /**< The minimum rssi to accept in the fast scan mode */
              .rssi = -127,
              /**< The weakest authmode to accept in the fast scan mode.
               * Note: In case this value is not set and password is set as per WPA2 standards (password len >= 8),
               * it will be defaulted to WPA2, and the device won't connect to deprecated WEP/WPA networks.
               * Please set authmode threshold as WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK to connect to WEP/WPA networks.
               */
              .authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD},
          /**< Configuration for Protected Management Frame. Will be advertised in RSN Capabilities in RSN IE. */
          .pmf_cfg = {},
          /**< Whether Radio Measurements are enabled for the connection */
          .rm_enabled = false,
          /**< Whether BSS Transition Management is enabled for the connection */
          .btm_enabled = false,
          /**< Whether MBO is enabled for the connection */
          .mbo_enabled = false,
          /**< Whether FT is enabled for the connection */
          .ft_enabled = false,
          /**< Whether OWE is enabled for the connection */
          .owe_enabled = false,
          /**< Whether to enable transition disable feature */
          .transition_disable = false,
          /**< Reserved for future feature set */
          .reserved = 0,
          /**< Whether SAE hash to element is enabled */
          .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
          /**< Number of connection retries station will do before moving to next AP.
           * scan_method should be set as WIFI_ALL_CHANNEL_SCAN to use this config.
           * Note: Enabling this may cause connection time to increase in case the best AP doesn't behave properly.
           */
          .failure_retry_cnt = 0}};

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(LOG_TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  s_wifi_event_group = xEventGroupCreate();
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE,
                                         pdFALSE,
                                         portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(LOG_TAG, "connected to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGE(LOG_TAG, "Failed to connect to SSID:%s, password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  }
  else
  {
    ESP_LOGE(LOG_TAG, "UNEXPECTED EVENT");
  }
}

extern "C"
{
  void app_main(void);
}

void app_main()
{
  // Set the logging level of this tag to verbose
  esp_log_level_set(LOG_TAG, ESP_LOG_VERBOSE);

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESPTOOLS_LOGI("ESP_WIFI_MODE_STA");
  wifi_init_sta();

  while (true)
  {
    ESPTOOLS_LOGV("Test message");
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
