#pragma once

#include "esp_err.h"
#include "lvgl.h"

/** Initialize the board AMOLED panel and register it with the LVGL port. */
esp_err_t amoled_display_init(lv_display_t **out_display);
