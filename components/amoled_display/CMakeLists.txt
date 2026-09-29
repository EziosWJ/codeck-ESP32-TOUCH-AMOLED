idf_component_register(
    SRCS "amoled_display.c"
    INCLUDE_DIRS "include"
    REQUIRES board_support esp_lcd esp_lcd_sh8601 esp_lvgl_port lvgl
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_rom
)
