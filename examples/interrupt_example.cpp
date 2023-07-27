#include <ESPTools/logger.h>
#include <ESPTools/interrupt.h>

extern "C"
{
  void app_main(void);
}

void app_main()
{
  // Tag used for the logging system
  static constexpr char LOG_TAG[]{"Interrupt Test"};
  // Set the logging level of this tag to verbose
  esp_log_level_set(LOG_TAG, ESP_LOG_VERBOSE);

  // Set the logging level ESPTools library to verbose
  esp_log_level_set(ESPTools::Interrupt::LOG_TAG, ESP_LOG_VERBOSE);
  esp_log_level_set(ESPTools::LOG_TAG, ESP_LOG_VERBOSE);

  // GPIO Pins
  // static constexpr gpio_num_t pir_gpio_num{GPIO_NUM_2};
  static constexpr gpio_num_t bed_gpio_num{GPIO_NUM_3};

  // Interruptions
  // static ESPTools::Interrupt pir_interrupt(pir_gpio_num,
  //                                         pdMS_TO_TICKS(100),
  //                                         pdMS_TO_TICKS(5000),
  //                                         GPIO_MODE_INPUT,
  //                                         GPIO_PULLUP_DISABLE,
  //                                         GPIO_PULLDOWN_ENABLE,
  //                                         false);
  static ESPTools::Interrupt bed_interrupt(bed_gpio_num,
                                           pdMS_TO_TICKS(20),
                                           pdMS_TO_TICKS(20),
                                           GPIO_MODE_INPUT,
                                           GPIO_PULLUP_ENABLE,
                                           GPIO_PULLDOWN_DISABLE,
                                           true);

  while (true)
  {
    if (bed_interrupt.WaitForSingleInterrupt(pdMS_TO_TICKS(10000)))
      ESPTOOLS_LOGI("(GPIO %u) Received Bed interrupt %s",
                    bed_interrupt.GpioNum(), bed_interrupt.RawState().ToStr());
    else
      ESPTOOLS_LOGW("(GPIO %u) Bed interrupt timeout", bed_interrupt.GpioNum());
  }
}
