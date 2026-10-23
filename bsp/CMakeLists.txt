idf_component_register(SRCS "bsp.cpp" INCLUDE_DIRS "include" PRIV_REQUIRES esp_driver_gpio)
set_source_files_properties(bsp.cpp PROPERTIES COMPILE_OPTIONS "-std=c++17")
