#include "ui_home.h"

#include "esp_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ui_home";
extern const lv_font_t ui_home_zh_20;

static lv_obj_t *clock_label, *status_dot, *title_label, *percent_label;
static lv_obj_t *reset_label, *countdown_label;
static lv_obj_t *quota_segments[5], *quota_glow, *quota_highlight;
static lv_obj_t *network_status_label, *ssid_label, *ip_label, *signal_label;
static lv_obj_t *service_label, *sync_label;
static lv_obj_t *home_page, *network_page, *page_dots[2];
static bool home_active, swipe_tracking;
static lv_point_t swipe_start;
static int last_percent;

#define COLOR_TEXT       0xF7F9FC
#define COLOR_SECONDARY  0x9AA8BA
#define COLOR_CYAN       0x25D9F3
#define COLOR_ONLINE     0x2EE6A6
#define QUOTA_START_ANGLE 125
#define QUOTA_SWEEP_DEGREES 290
#define QUOTA_SEGMENT_COUNT 5
#define SWIPE_DISTANCE 28
#define RETURN_X 157
#define RETURN_Y 398
#define RETURN_WIDTH 152
#define RETURN_HEIGHT 40

static void set_label_text(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static void set_text_color(lv_obj_t *label, uint32_t color)
{
    lv_color_t next = lv_color_hex(color);
    if (!lv_color_eq(lv_obj_get_style_text_color(label, 0), next)) {
        lv_obj_set_style_text_color(label, next, 0);
    }
}

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

static lv_obj_t *make_network_label(lv_obj_t *parent, const char *text, int32_t y,
                                    uint32_t color)
{
    lv_obj_t *label = make_label(parent, text, &ui_home_zh_20, color, 73, y, 256);
    lv_obj_set_width(label, 320);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

static void show_page(bool show_home)
{
    if (home_active == show_home) return;
    home_active = show_home;
    lv_obj_add_flag(show_home ? network_page : home_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(show_home ? home_page : network_page, LV_OBJ_FLAG_HIDDEN);
    for (unsigned i = 0; i < 2; i++) {
        lv_obj_set_style_bg_color(page_dots[i],
            lv_color_hex((i == 0) == home_active ? COLOR_CYAN : 0x354458), 0);
    }
}

/* Recognize the displacement while the finger is still down. Latch each
 * gesture so continuing the same touch cannot switch back a second time. */
static void swipe_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_PRESS_LOST &&
        code != LV_EVENT_RELEASED) return;
    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev) return;
    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &swipe_start);
        swipe_tracking = true;
    } else if (code == LV_EVENT_PRESS_LOST) {
        swipe_tracking = false;
    } else if ((code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) && swipe_tracking) {
        lv_point_t end;
        lv_indev_get_point(indev, &end);
        int32_t dx = end.x - swipe_start.x;
        int32_t dy = end.y - swipe_start.y;
        if (abs(dx) >= SWIPE_DISTANCE && abs(dx) > abs(dy) &&
            ((home_active && dx < 0) || (!home_active && dx > 0))) {
            swipe_tracking = false;
            show_page(!home_active);
        } else if (code == LV_EVENT_RELEASED) {
            swipe_tracking = false;
            if (!home_active && abs(dx) < 12 && abs(dy) < 12 &&
                swipe_start.x >= RETURN_X && swipe_start.x < RETURN_X + RETURN_WIDTH &&
                swipe_start.y >= RETURN_Y && swipe_start.y < RETURN_Y + RETURN_HEIGHT &&
                end.x >= RETURN_X && end.x < RETURN_X + RETURN_WIDTH &&
                end.y >= RETURN_Y && end.y < RETURN_Y + RETURN_HEIGHT) show_page(true);
        }
    }
}

static void prepare_page(lv_obj_t *page)
{
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
}

static void make_network_page(lv_obj_t *page)
{
    make_network_label(page, "联网状态", 63, COLOR_CYAN);
    network_status_label = make_network_label(page, "未配置", 108, COLOR_SECONDARY);
    make_network_label(page, "Wi-Fi", 153, COLOR_SECONDARY);
    ssid_label = make_network_label(page, "--", 185, COLOR_TEXT);
    ip_label = make_network_label(page, "IP: --", 238, COLOR_TEXT);
    signal_label = make_network_label(page, "信号: --", 279, COLOR_SECONDARY);
    service_label = make_network_label(page, "额度服务: 不可用", 325, COLOR_SECONDARY);
    sync_label = make_network_label(page, "时间同步: 未同步", 362, COLOR_SECONDARY);
    lv_obj_t *back = lv_obj_create(page);
    lv_obj_remove_style_all(back);
    lv_obj_set_pos(back, RETURN_X, RETURN_Y);
    lv_obj_set_size(back, RETURN_WIDTH, RETURN_HEIGHT);
    lv_obj_set_style_radius(back, 20, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x08131F), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_border_color(back, lv_color_hex(0x26618A), 0);
    lv_obj_remove_flag(back, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = make_label(back, "< 额度", &ui_home_zh_20, COLOR_CYAN, 0, 0, 256);
    lv_obj_center(label);
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

    home_page = lv_obj_create(screen);
    network_page = lv_obj_create(screen);
    prepare_page(home_page);
    prepare_page(network_page);
    lv_obj_set_pos(home_page, 0, 0);
    lv_obj_set_pos(network_page, 0, 0);
    lv_obj_add_flag(network_page, LV_OBJ_FLAG_HIDDEN);
    home_active = true;
    swipe_tracking = false;
    last_percent = -1;
    make_network_page(network_page);
    page_dots[0] = make_dot(screen, 217, 444, 8, COLOR_CYAN);
    page_dots[1] = make_dot(screen, 239, 444, 8, 0x354458);

    lv_obj_t *input = lv_obj_create(screen);
    lv_obj_remove_style_all(input);
    lv_obj_set_size(input, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(input, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(input, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(input, swipe_event, LV_EVENT_ALL, NULL);

    /* Keep the existing home layout in the first tile. */
    screen = home_page;
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
    lv_color_t status_color = lv_color_hex(view->online ? COLOR_ONLINE : 0xF39A51);
    if (!lv_color_eq(lv_obj_get_style_bg_color(status_dot, 0), status_color)) {
        lv_obj_set_style_bg_color(status_dot, status_color, 0);
    }
    if (view->has_quota) {
        char window[24], title[48], percent[8];
        quota_format_window(view->quota.window_minutes, window, sizeof(window));
        snprintf(title, sizeof(title), "%s 额度", window);
        snprintf(percent, sizeof(percent), "%d", view->quota.remaining_percent);
        set_label_text(title_label, title);
        set_label_text(percent_label, percent);
        if (last_percent != view->quota.remaining_percent) {
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
            last_percent = view->quota.remaining_percent;
        }
    }
    char clock[8], reset[32], countdown[80];
    quota_format_times(view, clock, sizeof(clock), reset, sizeof(reset), countdown, sizeof(countdown));
    set_label_text(clock_label, clock);
    set_label_text(reset_label, reset);
    set_label_text(countdown_label, countdown);
    set_label_text(service_label, view->online ? "额度服务: 正常" : "额度服务: 不可用");
    set_text_color(service_label, view->online ? COLOR_ONLINE : COLOR_SECONDARY);
    set_label_text(sync_label, view->time_synced ? "时间同步: 已同步" : "时间同步: 未同步");
}

void ui_home_update_network(const network_view_t *view)
{
    if (!view || !network_status_label) return;
    set_label_text(network_status_label, !view->configured ? "未配置" :
                      view->connected ? "已连接" : "未连接");
    set_text_color(network_status_label, view->connected ? COLOR_ONLINE : 0xF39A51);
    set_label_text(ssid_label, view->ssid[0] ? view->ssid : "--");
    char text[48];
    snprintf(text, sizeof(text), "IP: %s", view->connected ? view->ip : "--");
    set_label_text(ip_label, text);
    if (view->connected) snprintf(text, sizeof(text), "信号: %d dBm", view->rssi);
    else snprintf(text, sizeof(text), "信号: --");
    set_label_text(signal_label, text);
}
