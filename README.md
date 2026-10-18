# Desk dashboard firmware

C++17 firmware for the ESP32-S3-WROOM-1-N16R8 board in
[desk-dashboard-hw](https://github.com/rspolyak/desk-dashboard-hw). The UI is a
static dashboard drawn with the LVGL graphics library; the values it shows are
fixed text. The board has no display driver yet, so the firmware boots, logs
that it found no display, and stays idle.

Clone with `--recurse-submodules`, or run `git submodule update --init` once:
LVGL is a submodule in `third_party/lvgl`.

## Build and flash

Uses the existing ESP-IDF **v5.4** installation on this machine:

```sh
. <ESP-IDF directory>/export.sh
idf.py -B build/esp build
idf.py -B build/esp -p PORT flash monitor
```

On macOS the board's port is `/dev/cu.usbmodem…`. The USB-C connector goes
straight to the ESP32-S3's built-in USB port, which carries both flashing and
the console. Pressing RESET restarts the chip together with that USB port, so
the serial monitor loses the port for a moment and reconnects on its own.

The default chip is `esp32s3`; other chips are rejected. `sdkconfig.defaults`
selects 16 MB flash to match the module. PSRAM (the module's external RAM) is
disabled. The project uses the standard ESP-IDF bootloader, partition layout,
console and FreeRTOS configuration. C++ exceptions and run-time type
information (RTTI) are disabled. Generated `sdkconfig` is ignored; keep
intentional shared settings in `sdkconfig.defaults`.

On another machine, activate its IDF installation instead of the absolute path
above. ESP-IDF and toolchains are external dependencies, not vendored here.

Fonts in `app/fonts/` are Montserrat Bold, converted to LVGL's C format;
their license is `app/fonts/OFL.txt`.

See [docs/architecture.md](docs/architecture.md) for the code structure.
