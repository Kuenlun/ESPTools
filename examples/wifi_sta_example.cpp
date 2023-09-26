#include <ESPTools/logger.h>
#include <ESPTools/nvs.h>
#include <ESPTools/wifi.h>

#include "secrets.h"

// Tag used for the logging system
static constexpr char LOG_TAG[]{"WiFi Test"};

extern "C"
{
  void app_main(void);
}

void app_main()
{
  // Set the logging level of this tag to verbose
  esp_log_level_set(LOG_TAG, ESP_LOG_VERBOSE);
  // Set the logging level of ESPTools tag to verbose
  esp_log_level_set(ESPTools::LOG_TAG, ESP_LOG_VERBOSE);
  esp_log_level_set(ESPTools::NVS::LOG_TAG, ESP_LOG_VERBOSE);
  esp_log_level_set(ESPTools::WiFi::LOG_TAG, ESP_LOG_VERBOSE);

  ESPTools::WiFi wifi_obj(EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, 5);

  // Configure and initialize the ESP32 WiFi module in station (STA) mode
  wifi_obj.wifi_init_sta();
  // Wait until wifi is connected
  wifi_obj.wait_for_wifi_connection();
}
