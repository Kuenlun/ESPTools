#pragma once

#include "ESPTools/core.h"

namespace ESPTools
{

  namespace NVS
  {

    // Tag used for the logging system
    static constexpr char LOG_TAG[]{"NVS"};

    /**
     * @brief Initialize the Non-Volatile Storage (NVS) system.
     *
     * This function initializes the NVS and handles the following errors:
     * - ESP_ERR_NVS_NO_FREE_PAGES: NVS partition doesn't contain any empty pages. This may happen if NVS partition was truncated.
     * - ESP_ERR_NVS_NEW_VERSION_FOUND: NVS partition contains data in new format and cannot be recognized by this version of code.
     *
     * In case any of these errors occur, erase the entire partition and call nvs_flash_init again.
     */
    void init_nvs();

  } // namespace NVS

} // namespace ESPTools
