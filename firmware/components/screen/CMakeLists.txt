idf_component_register(SRCS "core/view.cpp" "idf/screen_task.cpp" INCLUDE_DIRS ".." PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES gfx engine sensor display clock
                       PRIV_REQUIRES freertos esp_timer esp_driver_gpio heap log esp_system)
