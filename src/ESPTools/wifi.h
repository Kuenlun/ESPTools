#pragma once

#include "ESPTools/core.h"

namespace ESPTools
{

  class WiFi
  {
  public:
    // Tag used for the logging system
    static constexpr char LOG_TAG[]{ESPTOOLS_LOG_TAG_CREATOR("WiFi")};

    /**
     * @brief Initialize and configure the ESP32 WiFi module for station (STA) mode and connects to STA
     *
     * This function performs the following steps:
     *  1. Initializes the Non-Volatile Storage (NVS).
     *  2. Initializes the underlying TCP/IP stack.
     *  3. Creates a default event loop task.
     *  4. Creates a default WiFi station network interface for STA mode.
     *  5. Initializes WiFi with default configuration settings.
     *  6. Create a FreeRTOS EventGroup to manage WiFi related events
     *  7. Registers event handler instances for WiFi events, including STA got IP event.
     *  8. Sets the WiFi operating mode as station.
     *  9. Configures WiFi settings, including SSID and password.
     * 10. Starts WiFi in station mode.
     */
    static esp_err_t ConnectToSTA(const char *const ssid,
                                  const char *const pass,
                                  const uint32_t max_retries = 5);

    /**
     * @brief Wait for a WiFi connection or connection failure.
     *
     * This function waits until either a successful WiFi connection (WIFI_CONNECTED_BIT) is established
     * or the maximum number of connection retries (WIFI_FAIL_BIT) is reached. The event bits are set by
     * the 'EventHandler()' function.
     *
     * @return true if a successful connection is established, false if the connection fails.
     */
    static esp_err_t WaitForWiFiConnection();

    static const char *GetSSID();

    static const char *GetPASS();

  }; // class WiFi

} // namespace ESPTools
