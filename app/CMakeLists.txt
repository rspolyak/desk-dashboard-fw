idf_component_register(SRCS app.cpp dashboard.cpp lvgl_port.cpp network.cpp
                            fonts/montserrat_bold_12.c fonts/montserrat_bold_16.c fonts/montserrat_bold_72.c
                       INCLUDE_DIRS "include"
                       REQUIRES bsp lvgl
                       PRIV_REQUIRES esp_event esp_netif esp_timer esp_wifi nvs_flash)
set_source_files_properties(app.cpp dashboard.cpp lvgl_port.cpp network.cpp PROPERTIES COMPILE_OPTIONS "-std=c++17")
