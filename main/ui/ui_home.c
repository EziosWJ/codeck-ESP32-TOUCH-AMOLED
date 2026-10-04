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
static lv_obj_t *home_page, *network_page, *settings_page, *page_dots[3];
static lv_obj_t *settings_status, *ap_name_label, *ap_password_label, *ap_url_label;
static lv_obj_t *settings_message, *primary_button, *primary_label, *cancel_button;
static lv_obj_t *confirm_panel;
static unsigned active_page;
static bool swipe_tracking, provision_seen, confirming;
static provision_view_t provision_view;
static ui_network_action_cb_t network_action_cb;
static void *network_action_context;
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

static void show_page(unsigned page)
{
    if (active_page == page || page > 2) return;
    active_page = page;
    lv_obj_t *pages[] = {home_page, network_page, settings_page};
    for (unsigned i = 0; i < 3; i++) {
        if (i == page) lv_obj_remove_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(page_dots[i], lv_color_hex(i == page ? COLOR_CYAN : 0x354458), 0);
    }
}

static bool inside(lv_point_t p, int x, int y, int w, int h)
{
    return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
}

static void request_action(network_action_t action)
{
    if (network_action_cb) network_action_cb(action, network_action_context);
}

/* The transparent input layer handles taps and swipes together. A latched
 * swipe never becomes a button tap, even if the finger reverses direction. */
static void swipe_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_PRESS_LOST && code != LV_EVENT_RELEASED) return;
    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev) return;
    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &swipe_start);
        swipe_tracking = true;
    } else if (code == LV_EVENT_PRESS_LOST) swipe_tracking = false;
    else if (swipe_tracking) {
        lv_point_t end;
        lv_indev_get_point(indev, &end);
        int dx = end.x - swipe_start.x, dy = end.y - swipe_start.y;
        if (!confirming && abs(dx) >= SWIPE_DISTANCE && abs(dx) > abs(dy) &&
            ((dx < 0 && active_page < 2) || (dx > 0 && active_page > 0))) {
            swipe_tracking = false;
            show_page(dx < 0 ? active_page + 1 : active_page - 1);
        } else if (code == LV_EVENT_RELEASED) {
            swipe_tracking = false;
            if (abs(dx) >= 12 || abs(dy) >= 12) return;
            if (active_page == 1 && inside(swipe_start, RETURN_X, RETURN_Y, RETURN_WIDTH, RETURN_HEIGHT) &&
                inside(end, RETURN_X, RETURN_Y, RETURN_WIDTH, RETURN_HEIGHT)) show_page(0);
            else if (active_page == 2) {
                if (confirming) {
                    if (inside(swipe_start, 113, 248, 240, 40) && inside(end, 113, 248, 240, 40)) {
                        confirming = false;
                        lv_obj_add_flag(confirm_panel, LV_OBJ_FLAG_HIDDEN);
                        request_action(NETWORK_PROVISION_START);
                    } else if (inside(swipe_start, 113, 298, 240, 40) && inside(end, 113, 298, 240, 40)) {
                        confirming = false;
                        lv_obj_add_flag(confirm_panel, LV_OBJ_FLAG_HIDDEN);
                    }
                } else if (inside(swipe_start, 113, 354, 240, 40) && inside(end, 113, 354, 240, 40) &&
                           !lv_obj_has_flag(primary_button, LV_OBJ_FLAG_HIDDEN)) {
                    if (provision_view.active) request_action(NETWORK_PROVISION_FINISH);
                    else {
                        confirming = true;
                        lv_obj_remove_flag(confirm_panel, LV_OBJ_FLAG_HIDDEN);
                    }
                } else if (provision_view.active && !provision_view.closing && inside(swipe_start, 157, 400, 152, 36) &&
                           inside(end, 157, 400, 152, 36)) request_action(NETWORK_PROVISION_CANCEL);
            }
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

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h, lv_obj_t **out_label)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x102A3C), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x26618A), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = make_label(button, text, &ui_home_zh_20, COLOR_CYAN, 0, 0, 256);
    lv_obj_center(label);
    if (out_label) *out_label = label;
    return button;
}

static void make_settings_page(lv_obj_t *page)
{
    make_network_label(page, "网络设置", 63, COLOR_CYAN);
    settings_status = make_network_label(page, "未配置", 104, COLOR_SECONDARY);
    ap_name_label = make_network_label(page, "手机网页配置 Wi-Fi 和 API", 153, COLOR_TEXT);
    ap_password_label = make_network_label(page, "", 193, COLOR_TEXT);
    ap_url_label = make_network_label(page, "", 233, COLOR_CYAN);
    settings_message = make_network_label(page, "尚未配置，请开始配网", 277, COLOR_SECONDARY);
    lv_label_set_long_mode(settings_message, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(settings_message, 65);
    primary_button = make_button(page, "开始配网", 113, 354, 240, 40, &primary_label);
    cancel_button = make_button(page, "取消配网", 157, 400, 152, 36, NULL);
    lv_obj_add_flag(cancel_button, LV_OBJ_FLAG_HIDDEN);
    confirm_panel = lv_obj_create(page);
    lv_obj_remove_style_all(confirm_panel);
    lv_obj_set_size(confirm_panel, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(confirm_panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(confirm_panel, LV_OPA_80, 0);
    lv_obj_remove_flag(confirm_panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *dialog = lv_obj_create(confirm_panel);
    lv_obj_remove_style_all(dialog);
    lv_obj_set_pos(dialog, 80, 118);
    lv_obj_set_size(dialog, 306, 236);
    lv_obj_set_style_radius(dialog, 24, 0);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0x102031), 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog, 1, 0);
    lv_obj_set_style_border_color(dialog, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_remove_flag(dialog, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = make_label(dialog, "进入配网模式？", &ui_home_zh_20, COLOR_TEXT, 0, 22, 256);
    lv_obj_set_width(title, 306);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *hint = make_label(dialog, "手机连接设备热点\n填写 Wi-Fi 和 API", &ui_home_zh_20, COLOR_SECONDARY, 0, 64, 256);
    lv_obj_set_width(hint, 306);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    make_button(dialog, "确认开启", 33, 130, 240, 40, NULL);
    make_button(dialog, "返回", 33, 180, 240, 40, NULL);
    lv_obj_add_flag(confirm_panel, LV_OBJ_FLAG_HIDDEN);
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
    settings_page = lv_obj_create(screen);
    prepare_page(home_page);
    prepare_page(network_page);
    prepare_page(settings_page);
    lv_obj_set_pos(home_page, 0, 0);
    lv_obj_set_pos(network_page, 0, 0);
    lv_obj_add_flag(network_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_page, LV_OBJ_FLAG_HIDDEN);
    active_page = 0;
    confirming = provision_seen = false;
    memset(&provision_view, 0, sizeof(provision_view));
    network_action_cb = NULL;
    swipe_tracking = false;
    last_percent = -1;
    make_network_page(network_page);
    make_settings_page(settings_page);
    page_dots[0] = make_dot(screen, 206, 444, 8, COLOR_CYAN);
    page_dots[1] = make_dot(screen, 228, 444, 8, 0x354458);
    page_dots[2] = make_dot(screen, 250, 444, 8, 0x354458);

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
    char display_ssid[97];
    ui_home_format_ssid(view->ssid, display_ssid, sizeof(display_ssid));
    set_label_text(ssid_label, display_ssid[0] ? display_ssid : "--");
    char text[48];
    snprintf(text, sizeof(text), "IP: %s", view->connected ? view->ip : "--");
    set_label_text(ip_label, text);
    if (view->connected) snprintf(text, sizeof(text), "信号: %d dBm", view->rssi);
    else snprintf(text, sizeof(text), "信号: --");
    set_label_text(signal_label, text);
}

void ui_home_set_network_action(ui_network_action_cb_t callback, void *context)
{
    network_action_cb = callback;
    network_action_context = context;
    if (provision_seen) ui_home_update_provision(&provision_view);
}

void ui_home_update_provision(const provision_view_t *view)
{
    if (!view || !settings_status) return;
    provision_view = *view;
    if (!provision_seen) {
        provision_seen = true;
        if (!view->configured) show_page(2);
    }
    set_label_text(settings_status, view->active ? "配网模式" : view->configured ? "已配置" : "未配置");
    char text[80];
    snprintf(text, sizeof(text), "热点: %s", view->ap_ssid);
    set_label_text(ap_name_label, view->active ? text : "手机网页配置 Wi-Fi 和 API");
    snprintf(text, sizeof(text), "密码: %s", view->ap_password);
    set_label_text(ap_password_label, view->active ? text : "");
    set_label_text(ap_url_label, view->active ? "http://192.168.4.1" : "");
    set_label_text(settings_message, view->message);
    bool finish = view->active && view->saved && view->phase == PROVISION_SAVED;
    if (network_action_cb && (!view->active || finish)) lv_obj_remove_flag(primary_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(primary_button, LV_OBJ_FLAG_HIDDEN);
    set_label_text(primary_label, finish ? "完成配网" : view->configured ? "重新配网" : "开始配网");
    if (network_action_cb && view->active && !view->closing) lv_obj_remove_flag(cancel_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(cancel_button, LV_OBJ_FLAG_HIDDEN);
}

void ui_home_format_ssid(const char *source, char *target, size_t capacity)
{
    if (!capacity) return;
    size_t out = 0;
    const unsigned char *p = (const unsigned char *)source;
    while (*p && out + 1 < capacity) {
        unsigned n = *p < 0x80 ? 1 : (*p & 0xe0) == 0xc0 ? 2 : (*p & 0xf0) == 0xe0 ? 3 : (*p & 0xf8) == 0xf0 ? 4 : 1;
        uint32_t cp = *p & (n == 1 ? 0x7f : n == 2 ? 0x1f : n == 3 ? 0x0f : 7);
        bool valid = *p < 0x80 || n > 1;
        for (unsigned i = 1; i < n; i++) {
            if (!p[i] || (p[i] & 0xc0) != 0x80) { valid = false; n = 1; break; }
            cp = (cp << 6) | (p[i] & 0x3f);
        }
        lv_font_glyph_dsc_t glyph;
        bool supported = valid && cp >= 32 && ui_home_zh_20.get_glyph_dsc(&ui_home_zh_20, &glyph, cp, 0);
        if (supported && out + n < capacity) { memcpy(target + out, p, n); out += n; }
        else target[out++] = '?';
        p += n;
    }
    target[out] = 0;
}
