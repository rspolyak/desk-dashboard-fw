idf_component_register(SRCS "bsp.cpp" INCLUDE_DIRS "include" PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi esp_lcd)
set_source_files_properties(bsp.cpp PROPERTIES COMPILE_OPTIONS "-std=c++17")
