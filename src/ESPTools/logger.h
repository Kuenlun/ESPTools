#pragma once

#include "ESPTools/core.h"

#ifdef ESPTOOLS_DEBUG
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif
#include <esp_log.h>

// Macros used for logging. Wrapper around ESP log macros that include the caller function
#define ESPTOOLS_LOGV(format, ...) \
    ESP_LOGV(LOG_TAG, "[%s] " format, __func__ __VA_OPT__(, ) __VA_ARGS__)
#define ESPTOOLS_LOGD(format, ...) \
    ESP_LOGD(LOG_TAG, "[%s] " format, __func__ __VA_OPT__(, ) __VA_ARGS__)
#define ESPTOOLS_LOGI(format, ...) \
    ESP_LOGI(LOG_TAG, "[%s] " format, __func__ __VA_OPT__(, ) __VA_ARGS__)
#define ESPTOOLS_LOGW(format, ...) \
    ESP_LOGW(LOG_TAG, "[%s] " format, __func__ __VA_OPT__(, ) __VA_ARGS__)
#define ESPTOOLS_LOGE(format, ...) \
    ESP_LOGE(LOG_TAG, "[%s] " format, __func__ __VA_OPT__(, ) __VA_ARGS__)
