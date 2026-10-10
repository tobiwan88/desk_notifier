idf_component_register(
    SRCS "src/cat_eyes.c" "src/cat_eyes_anim.c" "src/cat_eyes_render.c" "src/cat_eyes_styles.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
)
# The per-pixel loop is hot: build it for speed even in a size-optimised app.
set_source_files_properties("src/cat_eyes_render.c" PROPERTIES COMPILE_OPTIONS "-O2")
