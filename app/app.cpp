#include "app/app.hpp"
#include "bsp/bsp.hpp"
#include "dashboard.hpp"
#include "lvgl_port.hpp"
#include "lvgl/lvgl.h"
#include "esp_log.h"

namespace app {
namespace {
constexpr const char* log_tag = "dashboard";
} // namespace

/**
 * @brief Brings up the board and draws the dashboard on its display.
 * @return ESP_OK on success, otherwise the error of the step that failed.
 */
esp_err_t start()
{
    esp_err_t status = bsp::init();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "board init failed: %s", esp_err_to_name(status));
        return status;
    }
    ESP_LOGI(log_tag, "starting");

    status = lvgl_port::start();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display setup failed: %s", esp_err_to_name(status));
        return status;
    }

    dashboard::create();
    lv_refr_now(nullptr);
    bsp::display_backlight(true);
    return ESP_OK;
}
} // namespace app
