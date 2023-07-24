#ifdef ESP_TOOLS_DEBUG
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C"
{
  void app_main(void);
}

void app_main()
{
  // Tag used for the logging system
  static constexpr char LOG_TAG[]{"Test"};
  // Set the logging level of this tag to verbose
  esp_log_level_set(LOG_TAG, ESP_LOG_VERBOSE);

  while (true)
  {
    ESP_LOGI(LOG_TAG, "Test message");
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
