idf_component_register(SRCS "core/provision.cpp" "idf/ble.cpp" INCLUDE_DIRS ".."
                       REQUIRES engine
                       PRIV_REQUIRES bt esp_wifi esp_app_format esp_system esp_timer esp_hw_support log freertos
                                     settings net clock screen)
