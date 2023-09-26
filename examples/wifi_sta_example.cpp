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

#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN

// Tag used for the logging system
static constexpr char LOG_TAG[]{"WiFi"};

// Global EventGroupHandle_t for WiFi Events
static EventGroupHandle_t wifi_event_group;

static constexpr EventBits_t WIFI_CONNECTED_BIT{BIT0};
static constexpr EventBits_t WIFI_FAIL_BIT{BIT1};

static constexpr uint32_t EXAMPLE_ESP_MAXIMUM_RETRY{5};

/**
 * @brief Event handler function for managing Wi-Fi and IP-related events.
 *
 * This function is responsible for handling events related to Wi-Fi and IP connectivity on the ESP32.
 *
 * For Wi-Fi events (event_base == WIFI_EVENT), it handles:
 * - WIFI_EVENT_STA_START: Initiates the Wi-Fi connection process.
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
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
  static uint32_t s_retry_num{0}; // Counter for connection retries

  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
    {
      // Attempt to connect to the Wi-Fi network
      esp_wifi_connect();
      break;
    }
    case WIFI_EVENT_STA_DISCONNECTED:
    {
      ESPTOOLS_LOGW("Connect to the AP failed");
      if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
      {
        // Retry Wi-Fi connection
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
      ESPTOOLS_LOGI("WiFi connected to SSID: %s", EXAMPLE_ESP_WIFI_SSID);
      break;
    }
    default:
      ESPTOOLS_LOGW("Unhandled IP_EVENT: %ld", event_id);
      break;
    }
  }
}

static wifi_config_t wifi_cfg{
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

/**
 * @brief Initialize the Non-Volatile Storage (NVS) system.
 *
 * This function initializes the NVS and handles the following errors:
 * - ESP_ERR_NVS_NO_FREE_PAGES: NVS partition doesn't contain any empty pages. This may happen if NVS partition was truncated.
 * - ESP_ERR_NVS_NEW_VERSION_FOUND: NVS partition contains data in new format and cannot be recognized by this version of code.
 *
 * In case any of these errors occur, erase the entire partition and call nvs_flash_init again.
 */
static void init_nvs()
{
  esp_err_t ret{nvs_flash_init()};
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ESPTOOLS_LOGI("NVS initialized successfully");
}

/**
 * @brief Initialize and configure the ESP32 Wi-Fi module for station (STA) mode.
 *
 * This function performs the following steps:
 * 1. Initializes the Non-Volatile Storage (NVS).
 * 2. Initializes the underlying TCP/IP stack.
 * 3. Creates a default event loop task.
 * 4. Creates a default Wi-Fi station network interface for STA mode.
 * 5. Initializes Wi-Fi with default configuration settings.
 * 6. Registers event handler instances for Wi-Fi events, including STA got IP event.
 * 7. Sets the Wi-Fi operating mode as station.
 * 8. Configures Wi-Fi settings, including SSID and password.
 * 9. Starts Wi-Fi in station mode.
 */
static void wifi_init_sta(wifi_config_t &wifi_cfg)
{
  // Initialize NVS
  init_nvs();

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
                                                      &event_handler,   // Event handler function
                                                      nullptr,          // No user data needed
                                                      nullptr));        // No handler instance tracking

  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,            // Register for IP-related events
                                                      IP_EVENT_STA_GOT_IP, // Handle STA got IP event
                                                      &event_handler,      // Event handler function
                                                      nullptr,             // No user data needed
                                                      nullptr));           // No handler instance tracking

  // Set the WiFi operating mode as station
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  // Configure WiFi settings, including SSID and password
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
  // Start WiFi
  ESP_ERROR_CHECK(esp_wifi_start());

  ESPTOOLS_LOGI("WiFi STA initialized successfully");
}

/**
 * @brief Wait for a WiFi connection or connection failure.
 *
 * This function waits until either a successful WiFi connection (WIFI_CONNECTED_BIT) is established
 * or the maximum number of connection retries (WIFI_FAIL_BIT) is reached. The event bits are set by
 * the 'event_handler()' function.
 *
 * @return true if a successful connection is established, false if the connection fails.
 */
static bool wait_for_wifi_connection()
{
  EventBits_t bits{xEventGroupWaitBits(wifi_event_group,
                                       WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                       pdFALSE,
                                       pdFALSE,
                                       portMAX_DELAY)};

  if (bits & WIFI_CONNECTED_BIT)
  {
    ESPTOOLS_LOGI("Connected to SSID:%s", EXAMPLE_ESP_WIFI_SSID);
    return true;
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGE(LOG_TAG, "Failed to connect to SSID:%s", EXAMPLE_ESP_WIFI_SSID);
  }
  return false;
}

extern "C"
{
  void app_main(void);
}

void app_main()
{
  // Set the logging level of this tag to verbose
  esp_log_level_set(LOG_TAG, ESP_LOG_VERBOSE);

  // Configure and initialize the ESP32 WiFi module in station (STA) mode
  wifi_init_sta(wifi_cfg);
  // Wait until wifi is connected
  wait_for_wifi_connection();
}
