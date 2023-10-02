#pragma once

#include "ESPTools/core.h"

#include <esp_event_base.h>

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

  private:
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
                             int32_t event_id, void *event_data);

  }; // class WiFi

} // namespace ESPTools
