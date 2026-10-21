idf_component_register(SRCS "core/view.cpp" "core/pages.cpp" "core/squirrel.cpp" "core/day.cpp" "core/day_view.cpp" "core/fun.cpp" "core/memory.cpp" "core/bond.cpp" "core/shop.cpp" "core/friend_view.cpp" "core/props_calendar.cpp" "core/props_env.cpp" "core/props_window.cpp" "core/props_critters.cpp" "core/props_scenery.cpp" "core/props_friend.cpp" "core/props_wear.cpp" "core/props_places.cpp" "core/props_home.cpp" "core/sky.cpp" "idf/screen_task.cpp"
                       INCLUDE_DIRS ".." PRIV_INCLUDE_DIRS "../../main"
                       REQUIRES gfx engine sensor display clock weather
                       PRIV_REQUIRES freertos esp_timer esp_driver_gpio heap log esp_system esp_hw_support settings)
