idf_component_register(SRCS "idf/web.cpp" INCLUDE_DIRS ".." PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES engine eyes net
                       PRIV_REQUIRES esp_http_server esp_timer esp_system esp_app_format app_update esp_wifi settings log freertos)
