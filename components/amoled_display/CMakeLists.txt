idf_component_register(
    SRCS "amoled_display.c"
    INCLUDE_DIRS "include"
    REQUIRES board_support esp_lcd esp_lcd_sh8601 esp_lvgl_port lvgl
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_rom
)

# Keep LVGL's fixed-size object heap out of the internal RAM needed by DMA
# display buffers and Wi-Fi. This overrides the pool allocation hook without
# modifying the managed LVGL component.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_include_directories(${lvgl_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_definitions(${lvgl_lib} PRIVATE
    LV_MEM_POOL_INCLUDE="codeck_lvgl_pool.h"
    LV_MEM_POOL_ALLOC=codeck_lvgl_pool_alloc
)
