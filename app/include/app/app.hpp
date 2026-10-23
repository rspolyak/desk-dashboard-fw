#pragma once

#include "esp_err.h"

namespace app {
/**
 * @brief Brings up the board and draws the dashboard on its display.
 * @return ESP_OK on success, otherwise the error of the step that failed.
 */
esp_err_t start();
} // namespace app
