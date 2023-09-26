#include "ESPTools/nvs.h"
#include "ESPTools/logger.h"

#include <nvs_flash.h>

namespace ESPTools
{

  namespace NVS
  {

    void init_nvs()
    {
      esp_err_t ret{nvs_flash_init()};
      if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
      {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
      }
      ESP_ERROR_CHECK(ret);
      ESPTOOLS_LOGI("NVS initialized successfully");
    }

  } // namespace NVS

} // namespace ESPTools
