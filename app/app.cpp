#include "app/app.hpp"
#include "bsp/bsp.hpp"
#include "dashboard.hpp"
#include "lvgl_port.hpp"
#include "network.hpp"
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>

namespace app {
namespace {
constexpr const char* log_tag = "dashboard";

/**
 * @brief Reads the indoor sensor and shows the values in the tiles; on a failed read the tiles keep their values.
 * @param[in] labels The dashboard labels.
 */
void show_climate(const dashboard::Labels& labels)
{
    constexpr std::size_t tile_text_bytes = 16;

    float temperature_c = 0.0f;
    float humidity_percent = 0.0f;
    if (bsp::climate_read(temperature_c, humidity_percent) != ESP_OK) {
        return;
    }
    ESP_LOGI(log_tag, "indoor %.1f C, %.0f %% humidity", temperature_c, humidity_percent);

    char temperature_text[tile_text_bytes] = {0};
    char humidity_text[tile_text_bytes] = {0};
    std::snprintf(temperature_text, sizeof(temperature_text), "%.1f\xC2\xB0" "C", temperature_c);
    std::snprintf(humidity_text, sizeof(humidity_text), "%.0f %%", humidity_percent);
    lv_label_set_text(labels.temperature, temperature_text);
    lv_label_set_text(labels.humidity, humidity_text);
}

/**
 * @brief Picks the backlight brightness for an ambient light level; it rises with the logarithm of the light, as the eye sees it.
 * @param[in] lux Ambient light in lux.
 * @return Brightness in percent: dark_percent at dark_lux or less, 100 at bright_lux or more.
 */
int backlight_percent_for(float lux)
{
    constexpr float dark_lux = 10.0f;
    constexpr float bright_lux = 250.0f;
    constexpr float dark_percent = 10.0f;
    constexpr float bright_percent = 100.0f;

    const float limited_lux = std::clamp(lux, dark_lux, bright_lux);
    const float position = std::log(limited_lux / dark_lux) / std::log(bright_lux / dark_lux);
    return static_cast<int>(std::lround(dark_percent + position * (bright_percent - dark_percent)));
}

/**
 * @brief Reads the ambient light and fades the backlight to match; keeps the brightness if the read fails.
 * @return ESP_OK, or the error of the light read, which the board has already logged.
 */
esp_err_t adjust_backlight()
{
    float lux = 0.0f;
    const esp_err_t status = bsp::light_read(lux);
    if (status != ESP_OK) {
        return status;
    }

    const int percent = backlight_percent_for(lux);
    ESP_LOGI(log_tag, "light %.0f lux, backlight %d %%", lux, percent);
    bsp::display_backlight(percent);
    return ESP_OK;
}

/**
 * @brief Shows in the header whether the board is connected to Wi-Fi.
 * @param[in] label The header's network label.
 */
void show_network(lv_obj_t* label)
{
    const char* text = network::connected() ? "Wi-Fi" : "No Wi-Fi";
    lv_label_set_text(label, text);
}

/**
 * @brief Shows the local time and date once the clock has been set from the internet, or what the board is still
 *        waiting for; the clock and date are redrawn only when the minute changes.
 * @param[in] labels The dashboard labels.
 * @param[in,out] shown_minute Minute currently on the clock; -1 while no time is shown.
 */
void show_time(const dashboard::Labels& labels, int& shown_minute)
{
    constexpr int first_valid_year = 2025;
    constexpr int calendar_base_year = 1900;
    constexpr std::size_t text_bytes = 32;

    const std::time_t now = std::time(nullptr);
    std::tm local = {};
    localtime_r(&now, &local);
    if (local.tm_year + calendar_base_year < first_valid_year) {
        const char* waiting_text = network::connected() ? "Getting the time..." : "Connecting to Wi-Fi...";
        lv_label_set_text(labels.date, waiting_text);
        return;
    }

    if (local.tm_min == shown_minute) {
        return;
    }
    shown_minute = local.tm_min;

    char clock_text[text_bytes] = {0};
    std::strftime(clock_text, sizeof(clock_text), "%H:%M", &local);
    lv_label_set_text(labels.clock, clock_text);

    char date_text[text_bytes] = {0};
    std::strftime(date_text, sizeof(date_text), "%a %d %b", &local);
    for (char& letter : date_text) {
        letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    }
    lv_label_set_text(labels.date, date_text);

    ESP_LOGI(log_tag, "local time %s %s", date_text, clock_text);
}

/**
 * @brief Once per second updates the network label, the clock and the date; on the first pass and every 5 seconds after
 *        it reads the sensors that were set up and adjusts the backlight; never returns.
 * @param[in] labels The dashboard labels.
 * @param[in] climate_ready true if the temperature and humidity sensor was set up.
 * @param[in] light_ready true if the ambient light sensor was set up.
 */
[[noreturn]] void run_forever(const dashboard::Labels& labels, bool climate_ready, bool light_ready)
{
    constexpr std::uint32_t pass_period_ms = 1000;
    constexpr int passes_per_sensor_read = 5;
    constexpr int full_brightness_percent = 100;

    int shown_minute = -1;
    int passes_since_sensor_read = 0;
    for (;;) {
        show_network(labels.network);
        show_time(labels, shown_minute);

        if (passes_since_sensor_read == 0) {
            if (climate_ready) {
                show_climate(labels);
            }
            if (light_ready) {
                (void)adjust_backlight();
            } else {
                bsp::display_backlight(full_brightness_percent);
            }
        }

        lv_refr_now(nullptr);

        passes_since_sensor_read = (passes_since_sensor_read + 1) % passes_per_sensor_read;
        vTaskDelay(pdMS_TO_TICKS(pass_period_ms));
    }
}
} // namespace

/**
 * @brief Brings up the board and the network, draws the dashboard and keeps it up to date.
 * @return Only if the board or the display cannot be set up, with the error of that step.
 */
esp_err_t start()
{
    esp_err_t status = bsp::init();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "board init failed: %s", esp_err_to_name(status));
        return status;
    }
    ESP_LOGI(log_tag, "starting");

    const bool climate_ready = bsp::climate_init() == ESP_OK;
    const bool light_ready = bsp::light_init() == ESP_OK;

    status = lvgl_port::start();
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display setup failed: %s", esp_err_to_name(status));
        return status;
    }

    const dashboard::Labels labels = dashboard::create();
    lv_refr_now(nullptr);

    (void)network::start();

    run_forever(labels, climate_ready, light_ready);
}
} // namespace app
