#pragma once

#include "esp_err.h"

namespace app::network {
/**
 * @brief Connects to the Wi-Fi network, keeps reconnecting when it drops, and sets the clock from the internet.
 * @return ESP_OK, or the error of the setup step that failed.
 */
esp_err_t start();

/**
 * @brief Tells whether the board is connected to the Wi-Fi network.
 * @return true while connected.
 */
bool connected();
} // namespace app::network
