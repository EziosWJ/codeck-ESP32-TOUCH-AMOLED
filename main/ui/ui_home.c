#include "ui_home.h"

#include "esp_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "ui_home";
extern const lv_font_t ui_home_zh_20;
extern const lv_font_t ui_home_digits_96;
extern const lv_font_t ui_dashboard_zh_16;

static lv_obj_t *clock_label, *status_dot, *title_label, *percent_label;
static lv_obj_t *reset_label, *countdown_label, *remaining_label;
static lv_obj_t *percent_sign, *home_cost, *home_tasks, *home_running;
static lv_obj_t *network_status_label, *ssid_label, *ip_label, *signal_label;
static lv_obj_t *service_label, *sync_label;
enum { PAGE_HOME, PAGE_QUOTA, PAGE_COST, PAGE_NETWORK, PAGE_SETTINGS, PAGE_COUNT };
static lv_obj_t *home_page, *network_page, *settings_page, *quota_page, *cost_page;
static lv_obj_t *page_dots[PAGE_COUNT];
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

/* Draw the face directly, as on the detail pages: no full-screen canvas or
 * transient controls are needed on the 128 KiB LVGL heap. */
static void face_arc(lv_layer_t *layer, int x, int y, int radius, int width,
                     int start, int end, uint32_t color, lv_opa_t opacity, bool rounded)
{
    if (start == end) return;
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center = (lv_point_t){x, y};
    d.radius = radius;
    d.width = width;
    d.start_angle = start;
    d.end_angle = end;
    d.color = lv_color_hex(color);
    d.opa = opacity;
    d.rounded = rounded;
    lv_draw_arc(layer, &d);
}

static void face_rect(lv_layer_t *layer, int x, int y, int w, int h, int radius,
                      uint32_t top, uint32_t bottom, uint32_t edge)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = radius;
    d.bg_color = lv_color_hex(top);
    d.bg_grad.dir = LV_GRAD_DIR_VER;
    d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0].color = d.bg_color;
    d.bg_grad.stops[0].opa = LV_OPA_COVER;
    d.bg_grad.stops[1].color = lv_color_hex(bottom);
    d.bg_grad.stops[1].opa = LV_OPA_COVER;
    d.bg_grad.stops[1].frac = 255;
    d.border_width = edge ? 1 : 0;
    d.border_color = lv_color_hex(edge);
    if (edge) {
        d.shadow_color = d.border_color;
        d.shadow_width = 12;
        d.shadow_opa = LV_OPA_20;
    }
    lv_area_t area = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &d, &area);
}

static void face_line(lv_layer_t *layer, int x1, int y1, int x2, int y2,
                      uint32_t color, int width)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1 = (lv_point_precise_t){x1, y1};
    d.p2 = (lv_point_precise_t){x2, y2};
    d.color = lv_color_hex(color);
    d.width = width;
    d.round_start = d.round_end = true;
    lv_draw_line(layer, &d);
}

static void face_gradient(lv_layer_t *layer, int x, int y, int radius, int width,
                          int start, int sweep, const uint32_t colors[4])
{
    for (int i = 0; i < 48; i++) {
        int a = start + sweep * i / 48;
        int b = start + sweep * (i + 1) / 48;
        if (a == b) continue;
        /* Keep the blue/cyan/blue/violet palette anchored to the dial, even
         * when the remaining percentage changes. */
        int position = (a - start) * 3 * 255 / QUOTA_SWEEP_DEGREES;
        int stop = position / 255;
        if (stop > 2) stop = 2;
        lv_color_t color = lv_color_mix(lv_color_hex(colors[stop + 1]),
                                       lv_color_hex(colors[stop]), position - stop * 255);
        face_arc(layer, x, y, radius, width, a, b + (i < 47 ? 1 : 0),
                 lv_color_to_u32(color) & 0xffffff, LV_OPA_COVER, i == 0 || i == 47);
    }
}

static void draw_home(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target_obj(event), &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    int x = area.x1, y = area.y1;
    face_rect(layer, x + 8, y + 8, 450, 450, LV_RADIUS_CIRCLE, 0x020E17, 0x02030C, 0);
    face_rect(layer, x + 73, y + 103, 320, 320, LV_RADIUS_CIRCLE, 0x062233, 0x01050E, 0x124C70);
    face_arc(layer, x + 233, y + 233, 226, 2, 38, 142, 0x6651FA, LV_OPA_70, true);
    static const uint32_t track[] = {0x092F52, 0x063D43, 0x142A55, 0x48207A};
    static const uint32_t bright[] = {0x187FFF, 0x16ECEB, 0x218FFF, 0xBE53FF};
    face_gradient(layer, x + 233, y + 233, 219, 18, QUOTA_START_ANGLE,
                  QUOTA_SWEEP_DEGREES, track);
    /* Fine dial ticks sit inside the bright outer ring. */
    for (int a = 125; a <= 415; a += 5) {
        double rad = a * 3.141592653589793 / 180.0;
        int inner = a % 25 == 0 ? 194 : 198;
        face_line(layer, x + 233 + (int)(inner * cos(rad)), y + 233 + (int)(inner * sin(rad)),
                  x + 233 + (int)(202 * cos(rad)), y + 233 + (int)(202 * sin(rad)),
                  a % 25 == 0 ? 0x185B80 : 0x0B293F, 1);
    }
    int sweep = last_percent > 0 ? QUOTA_SWEEP_DEGREES * last_percent / 100 : 0;
    if (sweep) {
        face_arc(layer, x + 233, y + 233, 223, 26, 125, 125 + sweep, 0x167ED8, LV_OPA_20, true);
        face_gradient(layer, x + 233, y + 233, 219, 18, 125, sweep, bright);
        double rad = (125 + sweep) * 3.141592653589793 / 180.0;
        int cx = x + 233 + (int)(210 * cos(rad));
        int cy = y + 233 + (int)(210 * sin(rad));
        face_rect(layer, cx - 13, cy - 13, 26, 26, LV_RADIUS_CIRCLE, 0x124D86, 0x124D86, 0);
        face_rect(layer, cx - 9, cy - 9, 18, 18, LV_RADIUS_CIRCLE, 0xFFFFFF, 0xBEF8FF, 0);
    }
    static const int stars[][2] = {{69, 199}, {115, 184}, {92, 255}, {389, 204}, {362, 287}, {142, 430}, {320, 432}};
    for (unsigned i = 0; i < sizeof(stars) / sizeof(stars[0]); i++) {
        face_rect(layer, x + stars[i][0], y + stars[i][1], 2, 2, 1,
                  i < 3 ? 0x18CFFF : 0x7868FF, 0x279AFF, 0);
    }
    /* Clock icons and a quiet divider keep the reset information in the face. */
    face_arc(layer, x + 177, y + 125, 9, 2, 0, 360, COLOR_CYAN, LV_OPA_COVER, false);
    face_line(layer, x + 177, y + 125, x + 177, y + 119, COLOR_CYAN, 2);
    face_line(layer, x + 177, y + 125, x + 182, y + 125, COLOR_CYAN, 2);
    face_line(layer, x + 141, y + 279, x + 325, y + 279, 0x1C425A, 1);
    face_arc(layer, x + 161, y + 293, 7, 2, 0, 360, COLOR_SECONDARY, LV_OPA_COVER, false);
    face_line(layer, x + 161, y + 293, x + 161, y + 288, COLOR_SECONDARY, 1);
    face_line(layer, x + 161, y + 293, x + 165, y + 293, COLOR_SECONDARY, 1);
    static const uint32_t edges[] = {0x7545C1, 0x009B98, 0x176DD7};
    for (int i = 0; i < 3; i++) {
        int left = x + 89 + i * 98;
        face_rect(layer, left, y + 337, 92, 88, 34,
                  i == 1 ? 0x042B29 : 0x10152D, 0x030714, edges[i]);
    }
    face_rect(layer, x + 124, y + 348, 23, 23, LV_RADIUS_CIRCLE, 0xFF72A4, 0xEE397E, 0);
    face_rect(layer, x + 225, y + 350, 16, 16, 2, 0x042B29, 0x042B29, 0x00E2BC);
    face_line(layer, x + 229, y + 357, x + 233, y + 361, 0x00E2BC, 2);
    face_line(layer, x + 233, y + 361, x + 243, y + 350, 0x00E2BC, 2);
    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init(&triangle);
    triangle.color = lv_color_hex(0x159FFF);
    triangle.p[0] = (lv_point_precise_t){x + 326, y + 348};
    triangle.p[1] = (lv_point_precise_t){x + 326, y + 369};
    triangle.p[2] = (lv_point_precise_t){x + 343, y + 358};
    lv_draw_triangle(layer, &triangle);
}

static lv_obj_t *face_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                            uint32_t color, int x, int y, int width)
{
    lv_obj_t *label = make_label(parent, text, font, color, x, y, 256);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void center_percent(void)
{
    lv_obj_update_layout(percent_label);
    int width = lv_obj_get_width(percent_label);
    int sign_width = lv_obj_get_width(percent_sign);
    int left = (466 - width - sign_width - 3) / 2;
    lv_obj_set_x(percent_label, left);
    lv_obj_set_x(percent_sign, left + width + 3);
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
    if (active_page == page || page >= PAGE_COUNT) return;
    active_page = page;
    lv_obj_t *pages[] = {home_page, quota_page, cost_page, network_page, settings_page};
    for (unsigned i = 0; i < PAGE_COUNT; i++) {
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
            ((dx < 0 && active_page + 1 < PAGE_COUNT) || (dx > 0 && active_page > 0))) {
            swipe_tracking = false;
            show_page(dx < 0 ? active_page + 1 : active_page - 1);
        } else if (code == LV_EVENT_RELEASED) {
            swipe_tracking = false;
            if (abs(dx) >= 12 || abs(dy) >= 12) return;
            if (active_page == PAGE_NETWORK && inside(swipe_start, RETURN_X, RETURN_Y, RETURN_WIDTH, RETURN_HEIGHT) &&
                inside(end, RETURN_X, RETURN_Y, RETURN_WIDTH, RETURN_HEIGHT)) show_page(PAGE_HOME);
            else if (active_page == PAGE_SETTINGS) {
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
    lv_obj_t *label = make_label(back, "< 首页", &ui_home_zh_20, COLOR_CYAN, 0, 0, 256);
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
    quota_page = lv_obj_create(screen);
    cost_page = lv_obj_create(screen);
    network_page = lv_obj_create(screen);
    settings_page = lv_obj_create(screen);
    prepare_page(home_page);
    prepare_page(network_page);
    prepare_page(settings_page);
    prepare_page(quota_page);
    prepare_page(cost_page);
    lv_obj_set_pos(home_page, 0, 0);
    lv_obj_set_pos(network_page, 0, 0);
    lv_obj_add_flag(network_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(quota_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cost_page, LV_OBJ_FLAG_HIDDEN);
    active_page = PAGE_HOME;
    confirming = provision_seen = false;
    memset(&provision_view, 0, sizeof(provision_view));
    network_action_cb = NULL;
    swipe_tracking = false;
    last_percent = -1;
    make_network_page(network_page);
    make_settings_page(settings_page);
    ui_dashboard_init(quota_page, cost_page);
    for (unsigned i = 0; i < PAGE_COUNT; i++) {
        page_dots[i] = make_dot(screen, 185 + (int)i * 22, 444, 8,
                               i == 0 ? COLOR_CYAN : 0x354458);
    }

    lv_obj_t *input = lv_obj_create(screen);
    lv_obj_remove_style_all(input);
    lv_obj_set_size(input, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(input, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(input, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(input, swipe_event, LV_EVENT_ALL, NULL);

    screen = home_page;
    lv_obj_add_event_cb(screen, draw_home, LV_EVENT_DRAW_MAIN, NULL);
    clock_label = face_label(screen, "", &lv_font_montserrat_20, COLOR_TEXT, 178, 50, 110);
    status_dot = make_dot(screen, 182, 85, 12, COLOR_SECONDARY);
    make_label(screen, "Codeck", &lv_font_montserrat_24, COLOR_TEXT, 203, 77, 256);
    title_label = face_label(screen, "-- 额度", &ui_home_zh_20, COLOR_TEXT, 189, 109, 113);
    percent_label = make_label(screen, "--", &ui_home_digits_96, COLOR_TEXT, 0, 141, 256);
    percent_sign = make_label(screen, "%", &lv_font_montserrat_36, COLOR_TEXT, 0, 201, 256);
    center_percent();
    remaining_label = face_label(screen, "剩余额度 -- / --h", &ui_home_zh_20, COLOR_CYAN, 98, 244, 270);
    reset_label = make_label(screen, "", &ui_dashboard_zh_16, COLOR_SECONDARY, 178, 283, 256);
    countdown_label = face_label(screen, "", &ui_home_zh_20, COLOR_CYAN, 93, 305, 280);
    face_label(screen, "$", &lv_font_montserrat_20, 0x111323, 124, 350, 23);
    home_cost = face_label(screen, "$--", &lv_font_montserrat_24, COLOR_TEXT, 90, 374, 90);
    home_tasks = face_label(screen, "--", &lv_font_montserrat_28, COLOR_TEXT, 188, 372, 90);
    home_running = face_label(screen, "--", &lv_font_montserrat_28, COLOR_TEXT, 286, 372, 90);
    face_label(screen, "今日成本", &ui_dashboard_zh_16, 0xBCC8F5, 91, 401, 90);
    face_label(screen, "今日任务", &ui_dashboard_zh_16, 0xA6E8EB, 189, 401, 90);
    face_label(screen, "运行中", &ui_dashboard_zh_16, 0xA8C5FA, 287, 401, 90);

    return ESP_OK;
}

void ui_home_update(const quota_view_t *view)
{
    if (!view || !percent_label) return;
    ui_dashboard_update_quota(view);
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
        center_percent();
        int percent_value = view->quota.remaining_percent;
        if (percent_value < 0) percent_value = 0;
        if (percent_value > 100) percent_value = 100;
        if (last_percent != percent_value) {
            last_percent = percent_value;
            lv_obj_invalidate(home_page);
        }
        char remaining[80];
        double hours = view->quota.window_minutes / 60.0;
        snprintf(remaining, sizeof(remaining), "剩余额度 %.1f / %.1fh",
                 hours * percent_value / 100.0, hours);
        set_label_text(remaining_label, remaining);
    } else {
        set_label_text(title_label, "-- 额度");
        set_label_text(percent_label, "--");
        set_label_text(remaining_label, "剩余额度 -- / --h");
        center_percent();
        if (last_percent != -1) {
            last_percent = -1;
            lv_obj_invalidate(home_page);
        }
    }
    char clock[8], reset[32], countdown[80];
    quota_format_times(view, clock, sizeof(clock), reset, sizeof(reset), countdown, sizeof(countdown));
    set_label_text(clock_label, clock);
    char reset_display[48];
    if (reset[0]) snprintf(reset_display, sizeof(reset_display), "重置时间 %.5s", reset);
    else reset_display[0] = '\0';
    set_label_text(reset_label, reset_display);
    set_label_text(countdown_label, countdown);
    set_label_text(service_label, view->online ? "额度服务: 正常" : "额度服务: 不可用");
    set_text_color(service_label, view->online ? COLOR_ONLINE : COLOR_SECONDARY);
    set_label_text(sync_label, view->time_synced ? "时间同步: 已同步" : "时间同步: 未同步");
}

void ui_home_update_cost(const ui_cost_view_t *view)
{
    ui_dashboard_update_cost(view);
    if (!view || !home_cost) return;
    char text[32];
    if (view->has_cost && isfinite(view->today_cost_usd) && view->today_cost_usd >= 0) {
        if (view->today_cost_usd < 1000) snprintf(text, sizeof(text), "$%.2f", view->today_cost_usd);
        else snprintf(text, sizeof(text), "$%.1fK", fmin(view->today_cost_usd / 1000, 999.9));
    } else snprintf(text, sizeof(text), "$--");
    const lv_font_t *font = strlen(text) > 5 ? &lv_font_montserrat_20 : &lv_font_montserrat_24;
    if (lv_obj_get_style_text_font(home_cost, 0) != font) lv_obj_set_style_text_font(home_cost, font, 0);
    set_label_text(home_cost, text);
}

void ui_home_update_activity(const ui_activity_view_t *view)
{
    if (!view || !home_tasks) return;
    char text[24];
    if (view->has_today_tasks && view->today_tasks > 999) snprintf(text, sizeof(text), "999+");
    else if (view->has_today_tasks) snprintf(text, sizeof(text), "%u", (unsigned)view->today_tasks);
    else snprintf(text, sizeof(text), "--");
    set_label_text(home_tasks, text);
    if (view->has_running_tasks && view->running_tasks > 999) snprintf(text, sizeof(text), "999+");
    else if (view->has_running_tasks) snprintf(text, sizeof(text), "%u", (unsigned)view->running_tasks);
    else snprintf(text, sizeof(text), "--");
    set_label_text(home_running, text);
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
        if (!view->configured) show_page(PAGE_SETTINGS);
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
