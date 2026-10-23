#pragma once

#include "lvgl/lvgl.h"

namespace app::dashboard {
/**
 * @brief The labels that show the indoor sensor values.
 */
struct ClimateLabels {
    lv_obj_t* temperature = nullptr;
    lv_obj_t* humidity = nullptr;
};

/**
 * @brief Builds the dashboard on the active LVGL screen, laid out for 480x320; the indoor values start as dashes.
 * @return The labels of the INDOOR and HUMIDITY tiles, for updating their values.
 */
ClimateLabels create();
} // namespace app::dashboard
