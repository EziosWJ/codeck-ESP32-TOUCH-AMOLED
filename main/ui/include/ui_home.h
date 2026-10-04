#pragma once

#include "esp_err.h"
#include "lvgl.h"

/** Build the static 466 x 466 AMOLED home face on the active display. */
esp_err_t ui_home_init(lv_display_t *display);

