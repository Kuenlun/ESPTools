#pragma once

#include "ESPTools/core.h"

#include <mqtt_client.h>

namespace ESPTools
{

  class MQTT
  {
  public:
    // Tag used for the logging system
    static constexpr char LOG_TAG[]{ESPTOOLS_LOG_TAG_CREATOR("MQTT")};

    /**
     * @brief Construct a new MQTT object
     *
     * @param uri
     * @param port
     */
    MQTT(const char *const uri, const uint32_t port = 1883);

    /**
     * @brief Destroy the MQTT object
     *
     */
    ~MQTT();

    /**
     * @brief
     *
     * @param topic
     * @return true
     * @return false
     */
    bool Subscribe(const char *topic) const;

    /**
     * @brief
     */
    void Stop() const;

  private:
    /**
     * @brief Event handler function for managing MQTT events.
     *
     * @param arg Unused argument.
     * @param event_base The event base associated with the event
     * @param event_id The event identifier.
     * @param event_data Event-specific data.
     */
    static void EventHandler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);

  private:
    const esp_mqtt_client_config_t _mqtt_cfg;
    esp_mqtt_client_handle_t _client;

  }; // class MQTT

} // namespace ESPTools
