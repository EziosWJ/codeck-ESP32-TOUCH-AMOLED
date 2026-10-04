#include "ui_home.h"

#include "esp_check.h"
#include <stdio.h>

static const char *TAG = "ui_home";
extern const lv_font_t ui_home_zh_20;

/* Static preview values. No clock, account, or network dependency is used here. */
static const struct {
    const char *time;
    const char *brand;
    const char *quota_window;
    int remaining_percent;
    const char *reset_time;
    const char *countdown;
} mock = {
    .time = "10:24",
    .brand = "Codeck",
    .quota_window = "5H",
    .remaining_percent = 78,
    .reset_time = "16:40",
    .countdown = "2小时36分",
};

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
    lv_arc_set_value(arc, mock.remaining_percent);
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
    const int32_t active_sweep = QUOTA_SWEEP_DEGREES * mock.remaining_percent / 100;

    /* All eight Arc objects are created once, in paint order. */
    make_arc(screen, 452, 30, 0x22C9F5, LV_OPA_30, LV_OPA_TRANSP);
    make_arc(screen, 446, 20, 0, LV_OPA_TRANSP, LV_OPA_50);

    for (int32_t i = 0; i < QUOTA_SEGMENT_COUNT; i++) {
        int32_t start = QUOTA_START_ANGLE + active_sweep * i / QUOTA_SEGMENT_COUNT;
        const int32_t end = QUOTA_START_ANGLE + active_sweep * (i + 1) / QUOTA_SEGMENT_COUNT;
        if (i > 0) start -= 1; /* Rounded caps overlap instead of revealing the track. */
        lv_obj_t *segment = make_arc(screen, 446, 20, segment_colors[i],
                                     LV_OPA_COVER, LV_OPA_TRANSP);
        lv_arc_set_angles(segment, start, end);
    }

    make_arc(screen, 438, 5, 0x7CF4FF, LV_OPA_50, LV_OPA_TRANSP);
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

static void make_dot(lv_obj_t *parent, int32_t x, int32_t y, int32_t size, uint32_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_pos(dot, x, y);
    lv_obj_set_size(dot, size, size);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
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

    make_label(screen, mock.time, &lv_font_montserrat_28, COLOR_TEXT, 196, 40, 256);
    make_dot(screen, 164, 87, 10, COLOR_ONLINE);
    make_label(screen, mock.brand, &lv_font_montserrat_28, COLOR_TEXT, 183, 78, 256);

    char title_text[16];
    snprintf(title_text, sizeof(title_text), "%s 额度", mock.quota_window);
    make_label(screen, title_text, &ui_home_zh_20, COLOR_CYAN, 193, 125, 384);

    char percent_text[4];
    snprintf(percent_text, sizeof(percent_text), "%d", mock.remaining_percent);
    make_label(screen, percent_text, &lv_font_montserrat_48, COLOR_TEXT, 170, 189, 496);
    make_label(screen, "%", &lv_font_montserrat_28, COLOR_TEXT, 295, 216, 496);

    char reset_text[24];
    snprintf(reset_text, sizeof(reset_text), "%s 重置", mock.reset_time);
    make_label(screen, reset_text, &ui_home_zh_20, COLOR_SECONDARY, 173, 295, 256);

    char countdown_text[32];
    snprintf(countdown_text, sizeof(countdown_text), "还有 %s", mock.countdown);
    make_label(screen, countdown_text, &ui_home_zh_20, COLOR_CYAN, 154, 334, 288);

    return ESP_OK;
}
