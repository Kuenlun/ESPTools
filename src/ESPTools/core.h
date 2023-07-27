#pragma once

#include <freertos/FreeRTOS.h>

// Main tag used for the logging system
#define ESPTOOLS_LOG_TAG "ESPTools"
// Macro used to create the subtag for the library modules
#define ESPTOOLS_LOG_TAG_CREATOR(SUBTAG) ESPTOOLS_LOG_TAG "::" SUBTAG

namespace ESPTools
{

  // Master tag for ESPTools
  static constexpr char LOG_TAG[]{ESPTOOLS_LOG_TAG};

// Core ID for aplication tasks, used for FreeRTOS task creation
#ifdef CONFIG_FREERTOS_UNICORE
  static constexpr BaseType_t APP_CORE_ID{0};
#else
  static constexpr BaseType_t APP_CORE_ID{1};
#endif

  // Maximum value for UBaseType_t
  extern const UBaseType_t UBASETYPE_MAX;

  /**
   * @brief Generates a bitmask with bit set at given position.
   *
   * @param bitPosition The index of bit to set.
   * @return Bitmask with bit set at 'bitPosition'.
   */
  constexpr unsigned long CreateBitMaskAt(const uint8_t bitPosition) { return 1UL << bitPosition; }

  /**
   * @brief Wrapper around `gpio_install_isr_service` to keep track of ISR service installation
   * status, as ESP-IDF doesn't provide a way to determine whether the ISR service has been
   * installed or not. Please, use this function and not `gpio_install_isr_service`.
   *
   * @param intr_alloc_flags Flags used to allocate the interrupt. One or multiple (ORred)
   * ESP_INTR_FLAG_* values. See esp_intr_alloc.h for more info.
   */
  void InstallIsrService(const int intr_alloc_flags);

} // namespace ESPTools
