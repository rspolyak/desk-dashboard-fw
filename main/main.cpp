#include "app/app.hpp"

/**
 * @brief Program entry, called by ESP-IDF from its main task; the application logs its own failures.
 */
extern "C" void app_main() { (void)app::start(); }
