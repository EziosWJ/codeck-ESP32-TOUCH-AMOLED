#pragma once

#include "esp_err.h"
#include "lvgl.h"

/** Create the two-page touch demonstration on the supplied display. */
esp_err_t demo_ui_init(lv_display_t *display);
