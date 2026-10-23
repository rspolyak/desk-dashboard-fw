#pragma once

#include "esp_err.h"

namespace app {
/**
 * @brief Brings up the board, draws the dashboard and keeps the indoor values up to date.
 * @return Only if the board or the display cannot be set up, with the error of that step.
 */
esp_err_t start();
} // namespace app
