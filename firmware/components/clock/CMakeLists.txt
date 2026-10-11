idf_component_register(SRCS "idf/clock.cpp" INCLUDE_DIRS ".." PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES settings PRIV_REQUIRES esp_netif log)
