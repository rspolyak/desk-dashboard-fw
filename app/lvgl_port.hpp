#pragma once

#include "esp_err.h"

namespace app::lvgl_port {
/**
 * @brief Connects LVGL to the board display.
 * @return ESP_OK, or the error of the step that failed.
 */
esp_err_t start();
} // namespace app::lvgl_port
