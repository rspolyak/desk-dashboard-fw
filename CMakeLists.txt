cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET esp32s3)
endif()
# Components live at the project root, not in IDF's default components/ directory.
set(EXTRA_COMPONENT_DIRS app bsp third_party/lvgl)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# LVGL reads third_party/lv_conf.h, not its menuconfig options.
idf_build_set_property(COMPILE_DEFINITIONS "LV_KCONFIG_IGNORE" APPEND)
project(desk_dashboard)
if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "This firmware is for the ESP32-S3 only; set IDF_TARGET=esp32s3")
endif()
