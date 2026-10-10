idf_component_register(SRCS "idf/render_task.cpp" INCLUDE_DIRS ".." PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES gfx eyes freertos esp_timer
                       PRIV_REQUIRES display log esp_system esp_driver_gpio)
