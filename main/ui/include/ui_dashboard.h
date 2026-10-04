#pragma once

#include "lvgl.h"
#include "quota_data.h"
#include <stdint.h>

/* Optional cost fields remain unavailable until supplied by a data source. */
typedef struct {
    bool has_cost, has_comparison, has_hourly;
    bool has_tokens, has_unpriced_tokens, has_conversations;
    double today_cost_usd, yesterday_change_percent;
    double hourly_cost_usd[24];
    uint64_t total_tokens, unpriced_tokens, conversation_count;
} ui_cost_view_t;

/* Caller holds the LVGL lock. These pages create their controls only once. */
void ui_dashboard_init(lv_obj_t *quota_page, lv_obj_t *cost_page);
void ui_dashboard_update_quota(const quota_view_t *view);
void ui_dashboard_update_cost(const ui_cost_view_t *view);
