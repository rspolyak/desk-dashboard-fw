# Architecture

The firmware runs only on the desk dashboard PCB (ESP32-S3) and is built with
ESP-IDF (Espressif's software development kit). It uses ESP-IDF and FreeRTOS
directly for GPIO, buses, tasks, time and logging. Calls only go downward.

```text
 main/       main.cpp     program entry (app_main)
                 |
 app/        app          what the device shows and does
                 |
 bsp/        bsp          the PCB's devices and their pins
                 |
             ESP-IDF drivers, FreeRTOS
```

| If the code is about… | it goes in |
|---|---|
| what the user sees, what the device does | `app/` |
| the PCB's parts, their pins and how they are brought up | `bsp/` |

- `app` never names a chip or a pin. It reaches the hardware only through the
  board support package (BSP) API in `bsp/include/bsp/bsp.hpp`.
- `bsp` holds the pin map and brings up the devices with ESP-IDF drivers, so
  a pin change on the PCB touches only `bsp/bsp.cpp`.

## Error handling

Functions that can fail return ESP-IDF's `esp_err_t`. Every failure is logged
where it is detected, with what failed and the error name.
`ESP_ERR_NOT_SUPPORTED` from the BSP means the board does not offer that device
yet; it is a normal outcome and is not logged as an error.

## BSP API

- `init()` brings up the board's devices.
- `display_init()` reports the display size, or returns
  `ESP_ERR_NOT_SUPPORTED` while the board has no display driver.
  `display_draw()` takes a rectangle of RGB565 pixels (16-bit colour: 5 bits
  red, 6 green, 5 blue, in the CPU's native byte order) and blocks until they
  are taken. `display_backlight()` switches the backlight.

The display driver is not written yet: `display_init()` returns
`ESP_ERR_NOT_SUPPORTED`, and `display_draw()` and `display_backlight()` do
nothing.

## Entry point

`main/main.cpp` defines `app_main()`, which ESP-IDF calls from its main
FreeRTOS task. It calls `app::start()` and returns; the display keeps its
frame after the task ends.

## Application

`app::start()` brings up the board and, if the board has a display, draws the
dashboard once and switches the backlight on. The dashboard is static, so LVGL
is not called again after that frame.

The UI uses the LVGL graphics library v9.6.0, a git submodule in
`third_party/lvgl`, configured by `third_party/lv_conf.h`. The build defines
`LV_KCONFIG_IGNORE`, so LVGL's settings in ESP-IDF's configuration menu
(menuconfig) have no effect. LVGL renders in 40-row bands (partial mode);
`app/lvgl_port.cpp` passes each band to `bsp::display_draw()`. The first frame
is rendered completely before the backlight is switched on.

`app/dashboard.cpp` builds the dashboard screen for 480x320; the values it
shows are fixed text. Fonts are Montserrat Bold converted to LVGL's C format
in `app/fonts/`, under the SIL Open Font License (`app/fonts/OFL.txt`).

## Build

The `app` and `bsp` components live at the project root; IDF finds them and
`third_party/lvgl` through `EXTRA_COMPONENT_DIRS`, because they are not in
IDF's default `components/` directory. The project's own sources are compiled
as C++17; only `app_main` uses the C linkage ESP-IDF requires. Exceptions and
RTTI are disabled.
