#pragma once

#include "esp_err.h"

namespace app {
/**
 * @brief Brings up the board, draws the dashboard and keeps the indoor values up to date.
 * @return Only if setup fails, with its error, or with ESP_OK if the sensor is missing and nothing needs updating.
 */
esp_err_t start();
} // namespace app
