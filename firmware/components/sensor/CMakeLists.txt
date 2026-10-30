idf_component_register(SRCS "core/airq.cpp" "core/sht4x.cpp" "idf/bme688.cpp" "third_party/bme68x.c" INCLUDE_DIRS ".." "third_party"
                       PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES gfx freertos
                       PRIV_REQUIRES esp_driver_i2c esp_timer log)
set_source_files_properties(third_party/bme68x.c PROPERTIES COMPILE_OPTIONS "-w")
