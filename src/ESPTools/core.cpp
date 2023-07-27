#include "ESPTools/core.h"
#include "ESPTools/logger.h"

#include <driver/gpio.h>

#include <limits>

namespace ESPTools
{

  constexpr UBaseType_t UBASETYPE_MAX{std::numeric_limits<UBaseType_t>::max()};

  void InstallIsrService(const int intr_alloc_flags)
  {
    // Flag to track whether ISR service has been installed or not
    static bool interrupt_initialized{false};
    // Install ISR service if not installed
    if (!interrupt_initialized)
    {
      const esp_err_t ret{gpio_install_isr_service(intr_alloc_flags)};
      switch (ret)
      {
      case ESP_ERR_INVALID_STATE:
        ESPTOOLS_LOGW("Please use only InstallIsrService to install the ISR service");
        [[fallthrough]];
      case ESP_OK:
        interrupt_initialized = true;
        break;
      default:
        ESP_ERROR_CHECK(ret);
      }
    }
  }

} // namespace ESPTools
