#include "ui_home.h"

#include "esp_check.h"
#include <stdio.h>

static const char *TAG = "ui_home";
extern const lv_font_t ui_home_zh_20;

static lv_obj_t *clock_label, *status_dot, *title_label, *percent_label;
static lv_obj_t *reset_label, *countdown_label;
static lv_obj_t *quota_segments[5], *quota_glow, *quota_highlight;

#define COLOR_TEXT       0xF7F9FC
#define COLOR_SECONDARY  0x9AA8BA
#define COLOR_CYAN       0x25D9F3
#define COLOR_ONLINE     0x2EE6A6
#define QUOTA_START_ANGLE 125
#define QUOTA_SWEEP_DEGREES 290
#define QUOTA_SEGMENT_COUNT 5

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                             uint32_t color, int32_t x, int32_t y, int32_t scale)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    if(scale != 256) lv_obj_set_style_transform_scale(label, scale, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static lv_obj_t *make_arc(lv_obj_t *parent, int32_t size, int32_t width,
                           uint32_t color, lv_opa_t indicator_opa, lv_opa_t track_opa)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_arc_set_range(arc, 0, 100);
    /* 290 degrees around the top and sides; 70 degree gap centred at 6 o'clock. */
    lv_arc_set_bg_angles(arc, QUOTA_START_ANGLE, 55);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, indicator_opa, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x101827), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, track_opa, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return arc;
}

static void make_quota_ring(lv_obj_t *screen)
{
    static const uint32_t segment_colors[QUOTA_SEGMENT_COUNT] = {
        0x20E3FF, 0x22C9F5, 0x3280FF, 0x5568FF, 0x7C5CFF
    };
    /* All eight Arc objects are created once, in paint order. */
    quota_glow = make_arc(screen, 452, 30, 0x22C9F5, LV_OPA_30, LV_OPA_TRANSP);
    make_arc(screen, 446, 20, 0, LV_OPA_TRANSP, LV_OPA_50);

    for (int32_t i = 0; i < QUOTA_SEGMENT_COUNT; i++) {
        lv_obj_t *segment = make_arc(screen, 446, 20, segment_colors[i],
                                     LV_OPA_COVER, LV_OPA_TRANSP);
        quota_segments[i] = segment;
        lv_obj_add_flag(segment, LV_OBJ_FLAG_HIDDEN);
    }

    quota_highlight = make_arc(screen, 438, 5, 0x7CF4FF, LV_OPA_50, LV_OPA_TRANSP);
    lv_obj_add_flag(quota_glow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(quota_highlight, LV_OBJ_FLAG_HIDDEN);
}

static void make_reset_glass(lv_obj_t *screen)
{
    lv_obj_t *glass = lv_obj_create(screen);
    lv_obj_remove_style_all(glass);
    lv_obj_set_pos(glass, 115, 278);
    lv_obj_set_size(glass, 236, 102);
    lv_obj_set_style_radius(glass, 22, 0);
    lv_obj_set_style_bg_color(glass, lv_color_hex(0x08131F), 0);
    lv_obj_set_style_bg_opa(glass, LV_OPA_40, 0);
    lv_obj_set_style_border_color(glass, lv_color_hex(0x26618A), 0);
    lv_obj_set_style_border_width(glass, 1, 0);
    lv_obj_set_style_border_opa(glass, LV_OPA_30, 0);
    lv_obj_remove_flag(glass, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *make_dot(lv_obj_t *parent, int32_t x, int32_t y, int32_t size, uint32_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_pos(dot, x, y);
    lv_obj_set_size(dot, size, size);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}

esp_err_t ui_home_init(lv_display_t *display)
{
    ESP_RETURN_ON_FALSE(display != NULL, ESP_ERR_INVALID_ARG, TAG, "display is NULL");
    lv_display_set_default(display);
    lv_obj_t *screen = lv_display_get_screen_active(display);
    ESP_RETURN_ON_FALSE(screen != NULL, ESP_ERR_NO_MEM, TAG, "screen is NULL");

    lv_obj_clean(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    make_quota_ring(screen);
    make_reset_glass(screen);

    clock_label = make_label(screen, "", &lv_font_montserrat_28, COLOR_TEXT, 196, 40, 256);
    status_dot = make_dot(screen, 164, 87, 10, COLOR_SECONDARY);
    make_label(screen, "Codeck", &lv_font_montserrat_28, COLOR_TEXT, 183, 78, 256);
    title_label = make_label(screen, "-- 额度", &ui_home_zh_20, COLOR_CYAN, 193, 125, 384);
    percent_label = make_label(screen, "--", &lv_font_montserrat_48, COLOR_TEXT, 170, 189, 496);
    make_label(screen, "%", &lv_font_montserrat_28, COLOR_TEXT, 295, 216, 496);

    reset_label = make_label(screen, "", &ui_home_zh_20, COLOR_SECONDARY, 173, 295, 256);
    countdown_label = make_label(screen, "", &ui_home_zh_20, COLOR_CYAN, 154, 334, 288);

    return ESP_OK;
}

void ui_home_update(const quota_view_t *view)
{
    if (!view || !percent_label) return;
    lv_obj_set_style_bg_color(status_dot, lv_color_hex(view->online ? COLOR_ONLINE : 0xF39A51), 0);
    if (view->has_quota) {
        char window[24], title[48], percent[8];
        quota_format_window(view->quota.window_minutes, window, sizeof(window));
        snprintf(title, sizeof(title), "%s 额度", window);
        snprintf(percent, sizeof(percent), "%d", view->quota.remaining_percent);
        lv_label_set_text(title_label, title);
        lv_label_set_text(percent_label, percent);
        lv_obj_set_x(percent_label, view->quota.remaining_percent == 100 ? 135 :
                                   view->quota.remaining_percent < 10 ? 195 : 170);
        int32_t sweep = QUOTA_SWEEP_DEGREES * view->quota.remaining_percent / 100;
        lv_obj_t *effects[] = {quota_glow, quota_highlight};
        for (size_t i = 0; i < 2; i++) {
            lv_arc_set_value(effects[i], view->quota.remaining_percent);
            if (sweep) lv_obj_remove_flag(effects[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(effects[i], LV_OBJ_FLAG_HIDDEN);
        }
        for (int32_t i = 0; i < QUOTA_SEGMENT_COUNT; i++) {
            int32_t start = QUOTA_START_ANGLE + sweep * i / QUOTA_SEGMENT_COUNT;
            int32_t end = QUOTA_START_ANGLE + sweep * (i + 1) / QUOTA_SEGMENT_COUNT;
            if (end > start) {
                if (i > 0) start--;
                lv_arc_set_angles(quota_segments[i], start, end);
                lv_obj_remove_flag(quota_segments[i], LV_OBJ_FLAG_HIDDEN);
            } else lv_obj_add_flag(quota_segments[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    char clock[8], reset[32], countdown[80];
    quota_format_times(view, clock, sizeof(clock), reset, sizeof(reset), countdown, sizeof(countdown));
    lv_label_set_text(clock_label, clock);
    lv_label_set_text(reset_label, reset);
    lv_label_set_text(countdown_label, countdown);
}
