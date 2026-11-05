#pragma once

#include "lvgl/lvgl.h"

namespace app::dashboard {
/**
 * @brief The labels whose text the application updates.
 */
struct Labels {
    lv_obj_t* date = nullptr;
    lv_obj_t* network = nullptr;
    lv_obj_t* clock = nullptr;
    lv_obj_t* temperature = nullptr;
    lv_obj_t* humidity = nullptr;
};

/**
 * @brief Builds the dashboard on the active LVGL screen, laid out for 480x320.
 * @return The labels the application updates; the clock starts empty, the date line as "Connecting to Wi-Fi...",
 *         the network label as "No Wi-Fi" and the indoor values as dashes.
 */
Labels create();
} // namespace app::dashboard
