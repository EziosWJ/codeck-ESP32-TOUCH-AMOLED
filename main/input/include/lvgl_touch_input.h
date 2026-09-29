#pragma once

#include "esp_err.h"
#include "ft3168.h"
#include "lvgl.h"

/** Register FT3168 as a pointer input device for the supplied LVGL display. */
esp_err_t lvgl_touch_input_register(lv_display_t *display, ft3168_handle_t touch);
