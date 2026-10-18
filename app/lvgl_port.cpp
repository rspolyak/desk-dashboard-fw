#include "lvgl_port.hpp"
#include "bsp/bsp.hpp"
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstddef>
#include <cstdint>

namespace app::lvgl_port {
namespace {
constexpr const char* log_tag = "dashboard";
constexpr std::int64_t microseconds_per_millisecond = 1000;
constexpr std::size_t widest_display_pixels = 480;
constexpr std::size_t draw_buffer_rows = 40;
constexpr std::size_t draw_buffer_bytes = widest_display_pixels * draw_buffer_rows * sizeof(std::uint16_t);

// Static: LVGL draws into it for the lifetime of the program.
alignas(LV_DRAW_BUF_ALIGN) std::uint8_t draw_buffer[draw_buffer_bytes] = {0};

/**
 * @brief Gives LVGL the current time.
 * @return Milliseconds since an arbitrary start point.
 */
std::uint32_t tick_ms() { return static_cast<std::uint32_t>(esp_timer_get_time() / microseconds_per_millisecond); }

/**
 * @brief Sends one rendered band of pixels from LVGL to the board display.
 * @param[in] display LVGL display that rendered the band.
 * @param[in] area Screen rectangle the pixels belong to.
 * @param[in] pixels RGB565 pixels of the rectangle, row-major.
 */
void flush(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels)
{
    bsp::display_draw(area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area),
                      reinterpret_cast<const std::uint16_t*>(pixels));
    lv_display_flush_ready(display);
}
} // namespace

/**
 * @brief Connects LVGL to the board display.
 * @return ESP_OK, ESP_ERR_NOT_SUPPORTED if the board has no display, or the error of the step that failed.
 */
esp_err_t start()
{
    int width = 0;
    int height = 0;
    const esp_err_t status = bsp::display_init(width, height);
    if (status == ESP_ERR_NOT_SUPPORTED) {
        return status;
    }
    if (status != ESP_OK) {
        ESP_LOGE(log_tag, "display init failed: %s", esp_err_to_name(status));
        return status;
    }

    lv_init();
    lv_tick_set_cb(tick_ms);

    lv_display_t* display = lv_display_create(width, height);
    if (!display) {
        ESP_LOGE(log_tag, "lv_display_create failed");
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(display, draw_buffer, nullptr, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    return ESP_OK;
}
} // namespace app::lvgl_port
