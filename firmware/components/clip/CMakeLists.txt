idf_component_register(SRCS "core/clip.cpp" "idf/player.cpp" INCLUDE_DIRS ".."
                       REQUIRES engine gfx
                       PRIV_REQUIRES storage log)
