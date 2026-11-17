idf_component_register(SRCS "core/airq.cpp" "core/sht4x.cpp" "core/vocindex.cpp" "idf/bme688.cpp" "third_party/bme68x.c"
                       "third_party/sensirion_gas_index/sensirion_gas_index_algorithm.c" INCLUDE_DIRS ".." "third_party"
                       PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES gfx freertos
                       PRIV_REQUIRES esp_driver_i2c esp_timer log settings)
set_source_files_properties(third_party/bme68x.c third_party/sensirion_gas_index/sensirion_gas_index_algorithm.c PROPERTIES COMPILE_OPTIONS "-w")
