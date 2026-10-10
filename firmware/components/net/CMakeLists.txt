idf_component_register(SRCS "idf/net.cpp" "idf/console.cpp" INCLUDE_DIRS ".." PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES settings engine eyes
                       PRIV_REQUIRES esp_wifi esp_netif esp_event mdns console esp_driver_usb_serial_jtag esp_driver_uart esp_driver_gpio
                                     esp_timer esp_system esp_hw_support log nvs_flash freertos)
