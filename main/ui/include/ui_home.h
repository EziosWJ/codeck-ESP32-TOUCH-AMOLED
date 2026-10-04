#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "quota_data.h"

/** Build the 466 x 466 home face. Caller must hold the LVGL lock. */
esp_err_t ui_home_init(lv_display_t *display);

/** Update existing controls. Caller must hold the LVGL lock. */
void ui_home_update(const quota_view_t *view);
