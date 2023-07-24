#include "ESPTools/logger.h"

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
    ESPTOOLS_LOGV("Test message");
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
