#include "dashboard.hpp"
#include "lvgl/lvgl.h"
#include <cstdint>

extern "C" {
LV_FONT_DECLARE(montserrat_bold_12)
LV_FONT_DECLARE(montserrat_bold_16)
LV_FONT_DECLARE(montserrat_bold_72)
}

namespace app::dashboard {
namespace {
constexpr std::uint32_t background_color = 0x0F1724;
constexpr std::uint32_t divider_color = 0x2A3A50;
constexpr std::uint32_t tile_color = 0x1E2A3B;
constexpr std::uint32_t text_color = 0xF1F5F9;
constexpr std::uint32_t label_grey = 0xC0C9D6;
constexpr std::uint32_t value_cyan = 0xA5E8F7;
constexpr std::uint32_t sun_cream = 0xF6D3A1;

constexpr int screen_center_x = 240;

constexpr int header_margin_x = 24;
constexpr int date_y = 24;
constexpr int network_y = 27;
constexpr int divider_x = 16;
constexpr int divider_y = 62;
constexpr int divider_width = 448;
constexpr int divider_thickness = 1;

constexpr int clock_x = 20;
constexpr int clock_offset_y = -6;
constexpr int weather_text_y = 156;

constexpr int tile_y = 226;
constexpr int tile_width = 144;
constexpr int tile_height = 78;
constexpr int tile_radius = 10;
constexpr int tile_padding = 12;
constexpr int tile_text_inset_y = 14;
constexpr int tile_x[3] = {12, 168, 324};

constexpr int sun_x = 398;
constexpr int sun_y = 112;
constexpr int sun_radius = 19;
constexpr int ray_count = 8;
constexpr int ray_step_degrees = 360 / ray_count;
constexpr int ray_inner = 27;
constexpr int ray_outer = 37;
constexpr int ray_width = 4;
constexpr int points_per_ray = 2;

// lv_line keeps a pointer to its points, so they need static storage.
lv_point_precise_t ray_points[ray_count][points_per_ray] = {};

/**
 * @brief Creates a filled rectangle without LVGL's default styling.
 * @param[in] parent Object to place the rectangle in.
 * @param[in] x Left edge, relative to the parent.
 * @param[in] y Top edge, relative to the parent.
 * @param[in] width Width in pixels.
 * @param[in] height Height in pixels.
 * @param[in] color Fill colour as 0xRRGGBB.
 * @return The new rectangle.
 */
lv_obj_t* plain_box(lv_obj_t* parent, int x, int y, int width, int height, std::uint32_t color)
{
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    return box;
}

/**
 * @brief Creates a text label with the given font and colour.
 * @param[in] parent Object to place the label in.
 * @param[in] value Text to show; LVGL keeps its own copy.
 * @param[in] font Font to draw the text with.
 * @param[in] color Text colour as 0xRRGGBB.
 * @return The new label.
 */
lv_obj_t* label(lv_obj_t* parent, const char* value, const lv_font_t& font, std::uint32_t color)
{
    lv_obj_t* created = lv_label_create(parent);
    lv_label_set_text(created, value);
    lv_obj_set_style_text_font(created, &font, LV_PART_MAIN);
    lv_obj_set_style_text_color(created, lv_color_hex(color), LV_PART_MAIN);
    return created;
}

/**
 * @brief Creates one bottom-row tile with a small title and a larger value.
 * @param[in] parent Object to place the tile in.
 * @param[in] x Left edge of the tile.
 * @param[in] title Title shown at the top of the tile.
 * @param[in] value Value shown at the bottom; cut off with "..." if wider than the tile.
 * @param[in] value_color Value colour as 0xRRGGBB.
 * @return The value label, for updating the value later.
 */
lv_obj_t* tile(lv_obj_t* parent, int x, const char* title, const char* value, std::uint32_t value_color)
{
    lv_obj_t* box = plain_box(parent, x, tile_y, tile_width, tile_height, tile_color);
    lv_obj_set_style_radius(box, tile_radius, LV_PART_MAIN);
    lv_obj_align(label(box, title, montserrat_bold_12, label_grey), LV_ALIGN_TOP_LEFT, tile_padding,
                 tile_text_inset_y);
    lv_obj_t* value_label = label(box, value, montserrat_bold_16, value_color);
    lv_obj_set_width(value_label, tile_width - 2 * tile_padding);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(value_label, LV_ALIGN_BOTTOM_LEFT, tile_padding, -tile_text_inset_y);
    return value_label;
}

/**
 * @brief Draws the sun icon: a disc with eight rounded rays.
 * @param[in] parent Object to draw the icon in.
 */
void sun(lv_obj_t* parent)
{
    lv_obj_t* disc = plain_box(parent, sun_x - sun_radius, sun_y - sun_radius, 2 * sun_radius,
                               2 * sun_radius, sun_cream);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    for (int i = 0; i < ray_count; ++i) {
        const auto angle = static_cast<std::int16_t>(i * ray_step_degrees);
        const auto cosine = lv_trigo_cos(angle);
        const auto sine = lv_trigo_sin(angle);
        ray_points[i][0] = {static_cast<lv_value_precise_t>(sun_x + ((ray_inner * cosine) >> LV_TRIGO_SHIFT)),
                            static_cast<lv_value_precise_t>(sun_y + ((ray_inner * sine) >> LV_TRIGO_SHIFT))};
        ray_points[i][1] = {static_cast<lv_value_precise_t>(sun_x + ((ray_outer * cosine) >> LV_TRIGO_SHIFT)),
                            static_cast<lv_value_precise_t>(sun_y + ((ray_outer * sine) >> LV_TRIGO_SHIFT))};
        lv_obj_t* ray = lv_line_create(parent);
        lv_line_set_points(ray, ray_points[i], points_per_ray);
        lv_obj_set_style_line_width(ray, ray_width, LV_PART_MAIN);
        lv_obj_set_style_line_color(ray, lv_color_hex(sun_cream), LV_PART_MAIN);
        lv_obj_set_style_line_rounded(ray, true, LV_PART_MAIN);
    }
}
} // namespace

/**
 * @brief Builds the dashboard on the active LVGL screen, laid out for 480x320; the indoor values start as dashes.
 * @return The labels of the INDOOR and HUMIDITY tiles, for updating their values.
 */
ClimateLabels create()
{
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(background_color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollable(screen, false);

    lv_obj_align(label(screen, "SUN 27 SEP", montserrat_bold_16, text_color), LV_ALIGN_TOP_LEFT,
                 header_margin_x, date_y);
    lv_obj_align(label(screen, "Wi-Fi", montserrat_bold_12, text_color), LV_ALIGN_TOP_RIGHT,
                 -header_margin_x, network_y);
    plain_box(screen, divider_x, divider_y, divider_width, divider_thickness, divider_color);

    lv_obj_align(label(screen, "14:37", montserrat_bold_72, text_color), LV_ALIGN_LEFT_MID, clock_x,
                 clock_offset_y);
    sun(screen);
    lv_obj_align(label(screen, "18\xC2\xB0 Sunny", montserrat_bold_16, text_color), LV_ALIGN_TOP_MID,
                 sun_x - screen_center_x, weather_text_y);

    ClimateLabels climate = {};
    climate.temperature = tile(screen, tile_x[0], "INDOOR", "--.-\xC2\xB0" "C", value_cyan);
    climate.humidity = tile(screen, tile_x[1], "HUMIDITY", "-- %", value_cyan);
    tile(screen, tile_x[2], "NEXT", "15:00 Standup", text_color);
    return climate;
}
} // namespace app::dashboard
