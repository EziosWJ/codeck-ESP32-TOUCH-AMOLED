#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "quota_data.h"
#include "network_status.h"
#include "network_manager.h"
#include "ui_dashboard.h"
#include <stddef.h>

/** Build the five swipeable 466 x 466 pages. Caller must hold the LVGL lock. */
esp_err_t ui_home_init(lv_display_t *display);

/** Update existing controls. Caller must hold the LVGL lock. */
void ui_home_update(const quota_view_t *view);

/** Supply optional cost statistics for page five. Caller holds the LVGL lock. */
void ui_home_update_cost(const ui_cost_view_t *view);

/** Update Wi-Fi details independently of API health. Caller must hold the LVGL lock. */
void ui_home_update_network(const network_view_t *view);

/* All UI APIs require the LVGL lock; callback must enqueue work only. */
typedef void (*ui_network_action_cb_t)(network_action_t action, void *context);
void ui_home_set_network_action(ui_network_action_cb_t callback, void *context);
void ui_home_update_provision(const provision_view_t *view);
void ui_home_format_ssid(const char *source, char *target, size_t capacity);
