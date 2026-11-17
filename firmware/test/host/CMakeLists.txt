cmake_minimum_required(VERSION 3.20)
project(desk_host_tests C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Werror $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti> -fsanitize=address,undefined -fno-sanitize-recover=undefined)
add_link_options(-fsanitize=address,undefined)
file(GLOB TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
file(GLOB CORE_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../components/*/core/*.cpp)
# Vendored plain-C third-party sources that core/ code builds on (warnings off: not ours).
file(GLOB THIRD_PARTY_C CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../components/sensor/third_party/sensirion_gas_index/*.c)
set_source_files_properties(${THIRD_PARTY_C} PROPERTIES COMPILE_OPTIONS "-w")
list(APPEND CORE_SRCS ${THIRD_PARTY_C})
add_executable(host_tests main.cpp ${TESTS} ${CORE_SRCS})
target_include_directories(host_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../components)
target_compile_definitions(host_tests PRIVATE GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../golden")

# Renders the pages to raw frames for tools/preview_pages.py (not a test).
add_executable(page_preview preview.cpp ${CORE_SRCS})
target_include_directories(page_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../components)

# Renders each day-page prop for tools/prop_sheet.py (not a test).
add_executable(prop_sheet prop_sheet.cpp ${CORE_SRCS})
target_include_directories(prop_sheet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../components)

# Replays an iaq_log.py trace through sensor::Tracker and sensor::VocIndex (not a test).
add_executable(iaq_replay iaq_replay.cpp ${CORE_SRCS})
target_include_directories(iaq_replay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../components)
