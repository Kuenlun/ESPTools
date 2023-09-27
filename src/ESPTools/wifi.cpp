#include "ESPTools/wifi.h"
#include "ESPTools/logger.h"
#include "ESPTools/nvs.h"

#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <string.h>

namespace ESPTools
{

  // Global EventGroupHandle_t for WiFi Events
  static EventGroupHandle_t wifi_event_group;

  static constexpr EventBits_t WIFI_CONNECTED_BIT{CreateBitMaskAt(0)};
  static constexpr EventBits_t WIFI_FAIL_BIT{CreateBitMaskAt(1)};

  static uint32_t max_reconnection_retries{};

  static wifi_config_t wifi_cfg{.sta = {
                                    // SSID of target AP.
                                    .ssid{},
                                    // Password of target AP.
                                    .password{},
                                    // do all channel scan or fast scan
                                    .scan_method = WIFI_ALL_CHANNEL_SCAN,
                                    // whether set MAC address of target AP or not. Generally, station_config.bssid_set needs to be 0; and it needs to be 1 only when users need to check the MAC address of the AP.
                                    .bssid_set = false,
                                    // MAC address of target AP
                                    .bssid = {0}, // Uncomment and set the MAC address if needed.
                                    // channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0.
                                    .channel = 0,
                                    // Listen interval for ESP32 station to receive beacon when WIFI_PS_MAX_MODEM is set. Units: AP beacon intervals. Defaults to 3 if set to 0.
                                    .listen_interval = 3,
                                    // sort the connect AP in the list by rssi or security mode
                                    .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                                    // When sort_method is set, only APs which have an auth mode that is more secure than the selected auth mode and a signal stronger than the minimum RSSI will be used.
                                    .threshold = {
                                        // The minimum rssi to accept in the fast scan mode
                                        .rssi = -127,
                                        // The weakest authmode to accept in the fast scan mode.
                                        // Note: In case this value is not set and password is set as per WPA2 standards (password len >= 8),
                                        // it will be defaulted to WPA2, and the device won't connect to deprecated WEP/WPA networks.
                                        // Please set authmode threshold as WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK to connect to WEP/WPA networks.
                                        .authmode = WIFI_AUTH_OPEN},
                                    // Configuration for Protected Management Frame. Will be advertised in RSN Capabilities in RSN IE.
                                    .pmf_cfg = {},
                                    // Whether Radio Measurements are enabled for the connection
                                    .rm_enabled = false,
                                    // Whether BSS Transition Management is enabled for the connection
                                    .btm_enabled = false,
                                    // Whether MBO is enabled for the connection
                                    .mbo_enabled = false,
                                    // Whether FT is enabled for the connection
                                    .ft_enabled = false,
                                    // Whether OWE is enabled for the connection
                                    .owe_enabled = false,
                                    // Whether to enable transition disable feature
                                    .transition_disable = false,
                                    // Reserved for future feature set
                                    .reserved = 0,
                                    // Whether SAE hash to element is enabled
                                    .sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
                                    // Number of connection retries station will do before moving to next AP.
                                    // scan_method should be set as WIFI_ALL_CHANNEL_SCAN to use this config.
                                    // Note: Enabling this may cause connection time to increase in case the best AP doesn't behave properly.
                                    .failure_retry_cnt = 0}};

  /**
   * @brief Event handler function for managing WiFi and IP-related events.
   *
   * This function is responsible for handling events related to WiFi and IP connectivity on the ESP32.
   *
   * For WiFi events (event_base == WIFI_EVENT), it handles:
   * - WIFI_EVENT_STA_START: Initiates the WiFi connection process.
   * - WIFI_EVENT_STA_DISCONNECTED: Manages disconnections, retries connection if possible.
   *
   * For IP events (event_base == IP_EVENT), it handles:
   * - IP_EVENT_STA_GOT_IP: Processes successful IP address acquisition, resets retry counter.
   *
   * @param arg Unused argument.
   * @param event_base The event base associated with the event.
   * @param event_id The event identifier.
   * @param event_data Event-specific data.
   */
  static void EventHandler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
  {
    static uint32_t s_retry_num{0}; // Counter for connection retries

    if (event_base == WIFI_EVENT)
    {
      switch (event_id)
      {
      case WIFI_EVENT_STA_START:
      {
        // Attempt to connect to the WiFi network
        esp_wifi_connect();
        break;
      }
      case WIFI_EVENT_STA_DISCONNECTED:
      {
        ESPTOOLS_LOGW("Connect to the AP failed");
        if (s_retry_num < max_reconnection_retries)
        {
          // Retry WiFi connection
          esp_wifi_connect();
          s_retry_num++;
          ESPTOOLS_LOGD("Retry to connect to the AP");
        }
        else
        {
          // Maximum retry limit reached, indicate connection failure
          xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
          ESPTOOLS_LOGE("Connection to the AP failed");
        }
        break;
      }
      default:
        ESPTOOLS_LOGW("Unhandled WIFI_EVENT: %ld", event_id);
        break;
      }
    }
    else if (event_base == IP_EVENT)
    {
      switch (event_id)
      {
      case IP_EVENT_STA_GOT_IP:
      {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        // Log the obtained IP address
        ESPTOOLS_LOGD("Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0; // Reset the retry counter on successful connection
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESPTOOLS_LOGI("WiFi connected");
        break;
      }
      default:
        ESPTOOLS_LOGW("Unhandled IP_EVENT: %ld", event_id);
        break;
      }
    }
  }

  esp_err_t WiFi::ConnectToSTA(const char *const ssid,
                               const char *const pass,
                               const uint32_t max_retries)
  {
    strncpy(reinterpret_cast<char *>(wifi_cfg.sta.ssid), ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy(reinterpret_cast<char *>(wifi_cfg.sta.password), pass, sizeof(wifi_cfg.sta.password));

    max_reconnection_retries = max_retries;

    // Initialize NVS
    ESPTools::NVS::init_nvs();

    // Initialize the underlying TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop task
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Create a default WiFi station network interface for STA mode
    esp_netif_create_default_wifi_sta();
    // Initialize WiFi with default configuration settings
    static const wifi_init_config_t wifi_default_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_default_init_cfg));

    // Create an EventGroup to manage WiFi related events
    wifi_event_group = xEventGroupCreate();
    assert(wifi_event_group);
    // Register event handler instances for WiFi events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,       // Register for all WiFi events
                                                        ESP_EVENT_ANY_ID, // Handle any event ID
                                                        &EventHandler,   // Event handler function
                                                        nullptr,          // No user data needed
                                                        nullptr));        // No handler instance tracking

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,            // Register for IP-related events
                                                        IP_EVENT_STA_GOT_IP, // Handle STA got IP event
                                                        &EventHandler,      // Event handler function
                                                        nullptr,             // No user data needed
                                                        nullptr));           // No handler instance tracking

    // Set the WiFi operating mode as station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // Configure WiFi settings, including SSID and password
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESPTOOLS_LOGI("WiFi STA initialized successfully");
    return ESP_OK;
  }

  esp_err_t WiFi::WaitForWiFiConnection()
  {
    EventBits_t bits{xEventGroupWaitBits(wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE,
                                         pdFALSE,
                                         portMAX_DELAY)};

    if (bits & WIFI_CONNECTED_BIT)
    {
      ESPTOOLS_LOGI("Connected to SSID: %s", GetSSID());
      return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
      ESP_LOGE(LOG_TAG, "Failed to connect to SSID: %s", GetSSID());
    }
    return ESP_FAIL;
  }

  const char *WiFi::GetSSID() { return reinterpret_cast<const char *>(wifi_cfg.sta.ssid); };

  const char *WiFi::GetPASS() { return reinterpret_cast<const char *>(wifi_cfg.sta.password); };

} // namespace ESPTools
