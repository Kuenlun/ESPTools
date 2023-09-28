#include "ESPTools/mqtt.h"
#include "ESPTools/logger.h"

namespace ESPTools
{

  MQTT::MQTT(const char *const uri, const uint32_t port)
      : _mqtt_cfg{
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
                    .uri{uri},    // Complete MQTT broker URI
                    .hostname{},  // Hostname, to set ipv4 pass it as string)
                    .transport{}, // Selects transport
                    .path{},      // Path in the URI
                    .port{port}   // MQTT server port
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
            }}
  {
    _client = esp_mqtt_client_init(&_mqtt_cfg);
    esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, EventHandler, _client);

    esp_mqtt_client_start(_client);
  }

  /**
   * @brief Destroy the MQTT::MQTT object
   *
   */
  MQTT::~MQTT()
  {
    // The client does not need to be stopped separately since "esp_mqtt_client_destroy" function
    // already handles client termination.
    ESP_ERROR_CHECK(esp_mqtt_client_destroy(_client));
    ESPTOOLS_LOGI("Client destroyed successfully");
  }

  bool MQTT::Subscribe(const char *const topic) const
  {
    const int ret{esp_mqtt_client_subscribe(_client, topic, 0)};
    if (ret == -1)
    {
      ESPTOOLS_LOGE("Could not subscribe to topic: %s", topic);
      return false;
    }
    else
    {
      ESPTOOLS_LOGI("Subscribed to topic: %s (message ID: %d)", topic, ret);
      return true;
    }
  }

  void MQTT::Stop() const { ESP_ERROR_CHECK(esp_mqtt_client_stop(_client)); }

  void MQTT::EventHandler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
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

} // namespace ESPTools
