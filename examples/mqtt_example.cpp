#include <ESPTools/logger.h>
#include <ESPTools/nvs.h>
#include <ESPTools/wifi.h>

#include "mqtt_client.h"

#include "secrets.h"

// Tag used for the logging system
static constexpr char LOG_TAG[]{"MQTT Test"};

// Callback para gestionar la conexión MQTT
static void mqtt_event_handler(void *event_handler_arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
  switch (event_id)
  {
  case MQTT_EVENT_ANY:
    ESPTOOLS_LOGI("MQTT_EVENT_ANY");
    break;
  case MQTT_EVENT_ERROR:
    ESPTOOLS_LOGI("MQTT_EVENT_ERROR");
    break;
  case MQTT_EVENT_CONNECTED:
    ESPTOOLS_LOGI("MQTT_EVENT_CONNECTED");
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESPTOOLS_LOGI("MQTT_EVENT_DISCONNECTED");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESPTOOLS_LOGI("MQTT_EVENT_SUBSCRIBED");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESPTOOLS_LOGI("MQTT_EVENT_UNSUBSCRIBED");
    break;
  case MQTT_EVENT_PUBLISHED:
    ESPTOOLS_LOGI("MQTT_EVENT_PUBLISHED");
    break;
  case MQTT_EVENT_DATA:
    ESPTOOLS_LOGI("MQTT_EVENT_DATA");
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    ESPTOOLS_LOGI("MQTT_EVENT_BEFORE_CONNECT");
    break;
  case MQTT_EVENT_DELETED:
    ESPTOOLS_LOGI("MQTT_EVENT_DELETED");
    break;
  default:
    ESPTOOLS_LOGE("Unknown MQTT event");
    break;
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
  // Set the logging level of ESPTools tag to verbose
  esp_log_level_set(ESPTools::LOG_TAG, ESP_LOG_VERBOSE);
  esp_log_level_set(ESPTools::NVS::LOG_TAG, ESP_LOG_VERBOSE);
  esp_log_level_set(ESPTools::WiFi::LOG_TAG, ESP_LOG_VERBOSE);

  // Configure and initialize the ESP32 WiFi module in station (STA) mode
  ESPTools::WiFi::ConnectToSTA(EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, 5);
  // Wait until wifi is connected
  ESPTools::WiFi::WaitForWiFiConnection();

  esp_mqtt_client_config_t mqtt_cfg{
      /**
       *   Broker related configuration
       */
      .broker{
          /**
           * Broker address
           *
           *  - uri have precedence over other fields
           *  - If uri isn't set at least hostname, transport and port should.
           */
          .address{
              .uri{"mqtt://broker.hivemq.com"}, // Complete MQTT broker URI
              .hostname{},                      // Hostname, to set ipv4 pass it as string)
              .transport{},                     // Selects transport
              .path{},                          // Path in the URI
              .port{1883}                       // MQTT server port
          },
          /**
           * Broker identity verification
           *
           * If fields are not set broker's identity isn't verified. it's recommended
           * to set the options in this struct for security reasons.
           */
          .verification{
              .use_global_ca_store{},         // Use a global ca_store, look esp-tls documentation for details.
              .crt_bundle_attach{},           // Pointer to ESP x509 Certificate Bundle attach function for the usage of certificate bundles.
              .certificate{},                 // Certificate data, default is NULL, not required to verify the server.
              .certificate_len{},             // Length of the buffer pointed to by certificate.
              .psk_hint_key{},                // Pointer to PSK struct defined in esp_tls.h to enable PSK authentication (as alternative to certificate verification). PSK is enabled only if there are no other ways to verify broker.
              .skip_cert_common_name_check{}, // Skip any validation of server certificate CN field, this reduces the security of TLS and makes the MQTT client susceptible to MITM attacks
              .alpn_protos{}                  // NULL-terminated list of supported application protocols to be used for ALPN
          }},
      /**
       * Client related credentials for authentication.
       */
      .credentials{
          .username{},           // *MQTT* username
          .client_id{},          // Set *MQTT* client identifier. Ignored if set_null_client_id == true If NULL set the default client id. Default client id is ``ESP32_%CHIPID%`` where `%CHIPID%` are last 3 bytes of MAC address in hex format.
          .set_null_client_id{}, // Selects a NULL client id
          /**
           * Client authentication
           *
           * Fields related to client authentication by broker
           *
           * For mutual authentication using TLS, user could select certificate and key,
           * secure element or digital signature peripheral if available.
           *
           */
          .authentication{
              .password{},           // *MQTT* password
              .certificate{},        // Certificate for ssl mutual authentication, not required if mutual authentication is not needed. Must be provided with `key`.
              .certificate_len{},    // Length of the buffer pointed to by certificate.
              .key{},                // Private key for SSL mutual authentication, not required if mutual authentication is not needed. If it is not NULL, also `certificate` has to be provided.
              .key_len{},            // Length of the buffer pointed to by key.
              .key_password{},       // Client key decryption password, not PEM nor DER, if provided key_password_len` must be correctly set.
              .key_password_len{},   // Length of the password pointed to by `key_password`.
              .use_secure_element{}, // Enable secure element, available in ESP32-ROOM-32SE, for SSL connection.
              .ds_data{}             // Carrier of handle for digital signature parameters, digital signature peripheral is available in some Espressif devices.
          }},
      /**
       * *MQTT* Session related configuration
       */
      .session{
          /**
           * Last Will and Testament message configuration.
           */
          .last_will{
              .topic{},   // LWT (Last Will and Testament) message topic
              .msg{},     // LWT message, may be NULL terminated
              .msg_len{}, // LWT message length, if msg isn't NULL terminated must have the correct length
              .qos{},     // LWT message QoS
              .retain{}   // LWT retained message flag
          },
          .disable_clean_session{},     // *MQTT* clean session, default clean_session is true.
          .keepalive{},                 // *MQTT* keepalive, default is 120 seconds.
          .disable_keepalive{},         // Set `disable_keepalive=true` to turn off keep-alive mechanism, keepalive is active by default. Note: setting the config value `keepalive` to `0` doesn't disable keepalive feature, but uses a default keepalive period.
          .protocol_ver{},              // *MQTT* protocol version used for connection.
          .message_retransmit_timeout{} // timeout for retransmitting of failed packet.
      },
      /**
       * Network related configuration
       */
      .network{
          .reconnect_timeout_ms{},        // Reconnect to the broker after this value in miliseconds if auto reconnect is not disabled (defaults to 10s)
          .timeout_ms{},                  // Abort network operation if it is not completed after this value, in milliseconds defaults to 10s).
          .refresh_connection_after_ms{}, // Refresh connection after this value (in milliseconds)
          .disable_auto_reconnect{}       // Client will reconnect to server (when errors/disconnect). Set `disable_auto_reconnect=true` to disable
      },
      /**
       * Client task configuration
       */
      .task{
          .priority{},  // *MQTT* task priority
          .stack_size{} // *MQTT* task stack size
      },
      /**
       * Client buffer size configuration
       *
       * Client have two buffers for input and output respectivelly.
       */
      .buffer{
          .size{},    // size of *MQTT* send/receive buffer
          .out_size{} // size of *MQTT* output buffer. If not defined, defaults to the size defined by ``buffer_size``
      }};

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, client);

  esp_mqtt_client_start(client);

  esp_mqtt_client_subscribe(client, "Kuenlun_MQTT_Test", 0);

  vTaskDelete(nullptr);

  esp_mqtt_client_stop(client);
}
