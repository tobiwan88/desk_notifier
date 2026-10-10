idf_component_register(SRCS "idf/display.cpp" INCLUDE_DIRS ".." PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES gfx engine esp_lcd esp_driver_spi esp_driver_gpio freertos
                       PRIV_REQUIRES espressif__esp_lcd_gc9a01 log)
