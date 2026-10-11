idf_component_register(SRCS "core/forecast.cpp" "idf/client.cpp" INCLUDE_DIRS ".." PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES engine
                       PRIV_REQUIRES net settings esp_http_client mbedtls esp_timer freertos log)
