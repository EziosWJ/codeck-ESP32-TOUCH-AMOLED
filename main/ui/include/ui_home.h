#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "quota_data.h"
#include "network_status.h"

/** Build the two swipeable 466 x 466 pages. Caller must hold the LVGL lock. */
esp_err_t ui_home_init(lv_display_t *display);

/** Update existing controls. Caller must hold the LVGL lock. */
void ui_home_update(const quota_view_t *view);

/** Update Wi-Fi details independently of API health. Caller must hold the LVGL lock. */
void ui_home_update_network(const network_view_t *view);
