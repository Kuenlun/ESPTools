#include <ESPTools/logger.h>
#include <ESPTools/gpio_state.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C"
{
  void app_main(void);
}

void app_main()
{
  // Tag used for the logging system
  static constexpr char LOG_TAG[]{"GPIO State"};
  // Set the logging level of this tag to verbose
  esp_log_level_set(LOG_TAG, ESP_LOG_VERBOSE);

  // GPIO state initialized to ESPTools::GpioState::Undefined
  ESPTools::GpioState state_1;
  // GPIO state initialized to ESPTools::GpioState::Low
  ESPTools::GpioState state_2(0);
  // GPIO state initialized to ESPTools::GpioState::High
  ESPTools::GpioState state_3(0, true);

  while (true)
  {
    ESPTOOLS_LOGV("State 1 -> %s, State 2 -> %s, State 3 -> %s",
                  state_1.ToStr(), state_2.ToStr(), state_3.ToStr());
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
