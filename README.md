# Desk dashboard firmware

C++17 firmware for the ESP32-S3-WROOM-1-N16R8 board in
[desk-dashboard-hw](https://github.com/rspolyak/desk-dashboard-hw). The UI is a
dashboard drawn with the LVGL graphics library. Implemented: indoor temperature
and humidity from the SHT40 sensor, updated every 5 seconds; clock and date set
from the internet over Wi-Fi; a Wi-Fi status label; backlight brightness that
follows the ambient light sensor. Not implemented yet: the weather and the next
calendar entry are fixed text, and the touchscreen is not used.

Clone with `--recurse-submodules`, or run `git submodule update --init` once:
LVGL is a submodule in `third_party/lvgl`.

## Build and flash

Before the first build, set the Wi-Fi network name and password with
`idf.py menuconfig`, menu "Desk dashboard". They are stored in `sdkconfig`,
which git does not track. The time zone is a constant at the top of
`app/network.cpp`.

Requires ESP-IDF **v5.4**. Load its environment with the `export.sh` script in
the ESP-IDF installation directory, then build and flash:

```sh
. <ESP-IDF directory>/export.sh
idf.py -B build/esp build
idf.py -B build/esp -p PORT flash monitor
```

On macOS the board's port is `/dev/cu.usbmodem…`. The USB-C connector goes
straight to the ESP32-S3's built-in USB port, which carries both flashing and
the console. Pressing RESET restarts the chip together with that USB port, so
the serial monitor loses the port for a moment and reconnects on its own.

Fonts in `app/fonts/` are Montserrat Bold, converted to LVGL's C format;
their license is `app/fonts/OFL.txt`.