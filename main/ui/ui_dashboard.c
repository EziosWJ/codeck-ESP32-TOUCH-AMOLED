#include "ui_dashboard.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern const lv_font_t ui_home_zh_20;
extern const lv_font_t ui_dashboard_zh_16;
extern const lv_font_t ui_cost_digits_64;

#define WHITE 0xF5F8FF
#define MUTED 0xA4B7D4
#define CYAN 0x2BE8FA
#define PURPLE 0xD776FF
#define GREEN 0x13E7B3
#define PINK 0xF347C8
#define CHART_X 56
#define CHART_Y 313
#define CHART_WIDTH 354
#define CHART_HEIGHT 72

typedef struct {
    lv_obj_t *page, *clock;
    bool online;
} dashboard_t;

typedef struct {
    lv_obj_t *number, *reset, *countdown;
    int percent;
} quota_card_t;

static dashboard_t quota_dashboard, cost_dashboard;
static quota_card_t cards[2];
static lv_obj_t *cost_total, *cost_change, *cost_metrics[3], *chart_empty;
static int bar_heights[24];
static bool has_comparison;
static double change_percent;

/* Paint decoration directly so the round-screen effects do not require dozens
 * of persistent widgets or a 466 x 466 canvas in the device's LVGL heap. */
typedef struct { lv_layer_t *layer; int x, y; } painter_t;

static lv_color_t blend(uint32_t a, uint32_t b, int fraction)
{
    return lv_color_mix(lv_color_hex(b), lv_color_hex(a), (uint8_t)fraction);
}

static void rect(painter_t *p, int x, int y, int w, int h, int radius,
                 uint32_t top, uint32_t bottom, uint32_t edge, bool glow)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = radius;
    d.bg_color = lv_color_hex(top);
    d.bg_grad.dir = LV_GRAD_DIR_VER;
    d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0].color = d.bg_color;
    d.bg_grad.stops[0].opa = LV_OPA_COVER;
    d.bg_grad.stops[0].frac = 0;
    d.bg_grad.stops[1].color = lv_color_hex(bottom);
    d.bg_grad.stops[1].opa = LV_OPA_COVER;
    d.bg_grad.stops[1].frac = 255;
    d.border_width = edge ? 1 : 0;
    d.border_color = lv_color_hex(edge);
    if (glow) {
        d.shadow_color = d.border_color;
        d.shadow_width = 20;
        d.shadow_opa = LV_OPA_50;
    }
    lv_area_t area = {p->x + x, p->y + y, p->x + x + w - 1, p->y + y + h - 1};
    lv_draw_rect(p->layer, &d, &area);
}

static void arc(painter_t *p, int x, int y, int radius, int width,
                int start, int end, lv_color_t color, lv_opa_t opacity, bool rounded)
{
    if (start == end) return;
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center = (lv_point_t){p->x + x, p->y + y};
    d.radius = radius;
    d.width = width;
    d.start_angle = start;
    d.end_angle = end;
    d.color = color;
    d.opa = opacity;
    d.rounded = rounded;
    lv_draw_arc(p->layer, &d);
}

static void gradient_arc(painter_t *p, int x, int y, int radius, int width,
                         int start, int sweep, uint32_t first, uint32_t last)
{
    if (!sweep) return;
    arc(p, x, y, radius + 2, width + 4, start, start + sweep,
        lv_color_hex(first), LV_OPA_20, true);
    const int segments = 16;
    for (int i = 0; i < segments; i++) {
        int a = start + sweep * i / segments;
        int b = start + sweep * (i + 1) / segments;
        if (a == b) continue;
        arc(p, x, y, radius, width, a, b + (i + 1 < segments ? 1 : 0),
            blend(first, last, i * 255 / (segments - 1)), LV_OPA_COVER,
            i == 0 || i + 1 == segments);
    }
}

static void line(painter_t *p, int x1, int y1, int x2, int y2, uint32_t color, int width)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1 = (lv_point_precise_t){p->x + x1, p->y + y1};
    d.p2 = (lv_point_precise_t){p->x + x2, p->y + y2};
    d.color = lv_color_hex(color);
    d.width = width;
    d.round_start = d.round_end = true;
    lv_draw_line(p->layer, &d);
}

static void clock_icon(painter_t *p, int x, int y, uint32_t color)
{
    arc(p, x, y, 8, 2, 0, 360, lv_color_hex(color), LV_OPA_COVER, false);
    line(p, x, y, x, y - 4, color, 2);
    line(p, x, y, x + 4, y + 2, color, 2);
}

static void header(painter_t *p, dashboard_t *dashboard)
{
    rect(p, 17, 17, 432, 432, LV_RADIUS_CIRCLE, 0x020B14, 0x02030D, 0, false);
    gradient_arc(p, 233, 233, 211, 8, 145, 104, 0x146EFF, 0x20E7E8);
    gradient_arc(p, 233, 233, 211, 8, 294, 98, 0x1B56CC, 0xB34BFE);
    gradient_arc(p, 233, 233, 216, 2, 37, 106, 0xD257FF, 0x227EFF);
    rect(p, 177, 61, 11, 11, LV_RADIUS_CIRCLE,
         dashboard->online ? GREEN : 0xF19B57,
         dashboard->online ? GREEN : 0xF19B57, dashboard->online ? GREEN : 0xF19B57, true);
    static const int stars[][2] = {{70, 250}, {96, 135}, {133, 93}, {391, 237}, {364, 113}, {149, 420}};
    for (unsigned i = 0; i < sizeof(stars) / sizeof(stars[0]); i++) {
        uint32_t color = i < 3 ? 0x27BEFF : 0x937AFF;
        rect(p, stars[i][0], stars[i][1], 2, 2, 1, color, color, color, true);
    }
}

static void draw_quota(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target_obj(event), &area);
    painter_t p = {lv_event_get_layer(event), area.x1, area.y1};
    header(&p, &quota_dashboard);
    rect(&p, 181, 97, 27, 27, 6, 0x072A43, 0x081637, 0x38B6FF, true);
    arc(&p, 194, 112, 9, 2, 160, 380, lv_color_hex(0x4CCBFF), LV_OPA_COVER, true);
    line(&p, 194, 114, 199, 107, 0x4CCBFF, 3);

    for (unsigned i = 0; i < 2; i++) {
        int y = i ? 286 : 137;
        int cy = y + 67;
        uint32_t accent = i ? 0x9A58F8 : 0x16CDFA;
        rect(&p, i ? 76 : 65, y, i ? 314 : 336, 135, 32,
             i ? 0x20103B : 0x082B40, 0x030718, accent, true);
        arc(&p, 148, cy, 59, 13, 0, 360, lv_color_hex(i ? 0x30234F : 0x153347), LV_OPA_COVER, false);
        int percent = cards[i].percent;
        if (percent >= 0) {
            gradient_arc(&p, 148, cy, 59, 13, 270, 360 * percent / 100,
                         i ? 0xF078F4 : 0x37F2EE, i ? 0x7B46F6 : 0x245EFF);
        }
        clock_icon(&p, 228, y + 66, MUTED);
    }
}

static void metric_icon(painter_t *p, int x, int y, unsigned type, uint32_t color)
{
    if (type == 0) {
        rect(p, x, y + 3, 19, 19, 4, color, color, 0, false);
        rect(p, x, y, 19, 7, LV_RADIUS_CIRCLE, 0x7795FF, color, 0, false);
        line(p, x, y + 11, x + 18, y + 11, 0x182B79, 2);
        line(p, x, y + 17, x + 18, y + 17, 0x182B79, 2);
    } else if (type == 1) {
        rect(p, x + 2, y, 16, 23, 3, 0x09213B, 0x052032, color, true);
        line(p, x + 12, y, x + 12, y + 7, color, 2);
        line(p, x + 12, y + 7, x + 18, y + 7, color, 2);
        line(p, x + 6, y + 12, x + 13, y + 12, color, 2);
        line(p, x + 6, y + 17, x + 13, y + 17, color, 2);
    } else {
        rect(p, x, y + 1, 24, 18, 5, 0x171130, 0x080C23, color, true);
        line(p, x + 5, y + 19, x + 5, y + 24, color, 2);
        line(p, x + 5, y + 24, x + 12, y + 19, color, 2);
        rect(p, x + 7, y + 8, 3, 3, 2, 0xB9BDFF, 0xB9BDFF, 0, false);
        rect(p, x + 14, y + 8, 3, 3, 2, 0xB9BDFF, 0xB9BDFF, 0, false);
    }
}

static void draw_cost(lv_event_t *event)
{
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target_obj(event), &area);
    painter_t p = {lv_event_get_layer(event), area.x1, area.y1};
    header(&p, &cost_dashboard);
    rect(&p, 179, 94, 35, 35, LV_RADIUS_CIRCLE, 0xFE6598, 0xE40082, 0xEC267E, true);
    line(&p, 64, 139, 402, 139, 0x16314F, 1);
    line(&p, 288, 158, 288, 207, 0x263053, 1);
    if (has_comparison) {
        lv_draw_triangle_dsc_t d;
        lv_draw_triangle_dsc_init(&d);
        bool down = change_percent < 0;
        d.color = lv_color_hex(down ? GREEN : PINK);
        d.p[0] = (lv_point_precise_t){p.x + 307, p.y + (down ? 165 : 181)};
        d.p[1] = (lv_point_precise_t){p.x + 325, p.y + (down ? 165 : 181)};
        d.p[2] = (lv_point_precise_t){p.x + 316, p.y + (down ? 181 : 165)};
        lv_draw_triangle(p.layer, &d);
    }
    for (int x = CHART_X; x <= CHART_X + CHART_WIDTH; x += CHART_WIDTH / 6) {
        for (int y = CHART_Y - CHART_HEIGHT; y < CHART_Y; y += 10) {
            line(&p, x, y, x, y + 3, 0x1A3451, 1);
        }
    }
    line(&p, CHART_X, CHART_Y, CHART_X + CHART_WIDTH, CHART_Y, 0x16315A, 1);
    for (unsigned i = 0; i < 24; i++) {
        int h = bar_heights[i];
        if (h <= 0) continue;
        lv_color_t color = i < 12 ? blend(0x10E8E9, 0x379AFF, i * 255 / 11) :
                                   blend(0x9877FF, 0xF43AAA, (i - 12) * 255 / 11);
        uint32_t hex = lv_color_to_u32(color) & 0xffffff;
        rect(&p, CHART_X + (int)i * CHART_WIDTH / 24 + 3, CHART_Y - h,
             10, h, 5, hex, i < 12 ? 0x076CD2 : 0x6422A8, 0, false);
    }
    static const uint32_t accents[] = {0x435BFF, 0x18C4F5, 0xA449FD};
    for (unsigned i = 0; i < 3; i++) {
        int x = 76 + (int)i * 106;
        rect(&p, x, 342, 100, 70, 18, i == 1 ? 0x082C3C : 0x18133B,
             0x030714, accents[i], true);
        metric_icon(&p, x + 7, 359, i, i == 1 ? 0x3BDBFF : 0x6A83FF);
    }
}

static lv_obj_t *label(lv_obj_t *page, const char *text, const lv_font_t *font,
                       uint32_t color, int x, int y, int width, lv_text_align_t align)
{
    lv_obj_t *obj = lv_label_create(page);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(obj, align, 0);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static void text(lv_obj_t *obj, const char *value)
{
    if (strcmp(lv_label_get_text(obj), value)) lv_label_set_text(obj, value);
}

static void invalidate(lv_obj_t *obj, int x, int y, int w, int h)
{
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    area = (lv_area_t){area.x1 + x, area.y1 + y, area.x1 + x + w - 1, area.y1 + y + h - 1};
    lv_obj_invalidate_area(obj, &area);
}

static void make_header(dashboard_t *dashboard, lv_obj_t *page)
{
    dashboard->page = page;
    dashboard->online = false;
    dashboard->clock = label(page, "--:--", &lv_font_montserrat_24, WHITE, 171, 29, 124, LV_TEXT_ALIGN_CENTER);
    label(page, "Codeck", &lv_font_montserrat_24, WHITE, 195, 53, 118, LV_TEXT_ALIGN_LEFT);
}

void ui_dashboard_init(lv_obj_t *quota_page, lv_obj_t *cost_page)
{
    memset(cards, 0, sizeof(cards));
    memset(bar_heights, 0, sizeof(bar_heights));
    has_comparison = false;
    change_percent = 0;
    make_header(&quota_dashboard, quota_page);
    make_header(&cost_dashboard, cost_page);
    lv_obj_add_event_cb(quota_page, draw_quota, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(cost_page, draw_cost, LV_EVENT_DRAW_MAIN, NULL);
    label(quota_page, "额度", &ui_home_zh_20, WHITE, 222, 94, 86, LV_TEXT_ALIGN_LEFT);
    label(cost_page, "$", &lv_font_montserrat_28, WHITE, 184, 97, 26, LV_TEXT_ALIGN_CENTER);
    label(cost_page, "成本", &ui_home_zh_20, WHITE, 228, 95, 84, LV_TEXT_ALIGN_LEFT);
    for (unsigned i = 0; i < 2; i++) {
        int y = i ? 286 : 137;
        uint32_t accent = i ? PURPLE : CYAN;
        cards[i].percent = -1;
        cards[i].number = label(quota_page, "--", &lv_font_montserrat_36, WHITE,
                                99, y + 43, 81, LV_TEXT_ALIGN_CENTER);
        label(quota_page, "%", &lv_font_montserrat_14, WHITE, 175, y + 61, 24, LV_TEXT_ALIGN_LEFT);
        label(quota_page, "剩余", &ui_dashboard_zh_16, MUTED, 120, y + 83, 60, LV_TEXT_ALIGN_CENTER);
        label(quota_page, i ? "7天额度" : "5H 额度", &ui_home_zh_20, WHITE, 219, y + 24, 167, LV_TEXT_ALIGN_LEFT);
        cards[i].reset = label(quota_page, "等待数据", &ui_dashboard_zh_16, MUTED,
                               242, y + 55, 145, LV_TEXT_ALIGN_LEFT);
        cards[i].countdown = label(quota_page, "--", &ui_dashboard_zh_16, accent,
                                   219, y + 89, 168, LV_TEXT_ALIGN_LEFT);
    }
    cost_total = label(cost_page, "$--", &ui_cost_digits_64, WHITE, 64, 137, 223, LV_TEXT_ALIGN_LEFT);
    label(cost_page, "今日总成本", &ui_home_zh_20, MUTED, 68, 211, 210, LV_TEXT_ALIGN_LEFT);
    cost_change = label(cost_page, "--", &lv_font_montserrat_24, GREEN, 327, 159, 88, LV_TEXT_ALIGN_LEFT);
    label(cost_page, "较昨日", &ui_home_zh_20, MUTED, 315, 194, 96, LV_TEXT_ALIGN_LEFT);
    chart_empty = label(cost_page, "暂无小时数据", &ui_dashboard_zh_16, MUTED,
                         CHART_X, 268, CHART_WIDTH, LV_TEXT_ALIGN_CENTER);
    const char *hours[] = {"0时", "6时", "12时", "18时", "24时"};
    for (unsigned i = 0; i < 5; i++) {
        label(cost_page, hours[i], &ui_dashboard_zh_16, MUTED,
              45 + (int)i * 88, 315, 47, LV_TEXT_ALIGN_CENTER);
    }
    const char *names[] = {"总 Tokens", "未计价", "对话轮次"};
    for (unsigned i = 0; i < 3; i++) {
        int x = 76 + (int)i * 106;
        cost_metrics[i] = label(cost_page, "--", &lv_font_montserrat_20, WHITE,
                                x + 28, 358, 70, LV_TEXT_ALIGN_CENTER);
        label(cost_page, names[i], &ui_dashboard_zh_16, MUTED, x + 2, 384, 96, LV_TEXT_ALIGN_CENTER);
    }
}

static void update_header(dashboard_t *dashboard, const char *clock, bool online)
{
    text(dashboard->clock, clock[0] ? clock : "--:--");
    if (dashboard->online != online) {
        dashboard->online = online;
        invalidate(dashboard->page, 168, 52, 30, 30);
    }
}

void ui_dashboard_update_quota(const quota_view_t *view)
{
    if (!view || !quota_dashboard.page) return;
    char clock[8], reset[32], countdown[80];
    quota_format_times(view, clock, sizeof(clock), reset, sizeof(reset), countdown, sizeof(countdown));
    update_header(&quota_dashboard, clock, view->online);
    update_header(&cost_dashboard, clock, view->online);
    for (unsigned i = 0; i < 2; i++) {
        bool available = i ? view->has_weekly_quota : view->has_quota;
        const quota_data_t *q = i ? &view->weekly_quota : &view->quota;
        int percent = available ? LV_CLAMP(0, q->remaining_percent, 100) : -1;
        if (cards[i].percent != percent) {
            cards[i].percent = percent;
            invalidate(quota_dashboard.page, 83, i ? 286 : 137, 132, 135);
        }
        char number[16];
        if (available) snprintf(number, sizeof(number), "%d", percent);
        else snprintf(number, sizeof(number), "--");
        text(cards[i].number, number);
        if (!available) {
            text(cards[i].reset, "等待数据");
            text(cards[i].countdown, "--");
        } else if (!view->time_synced) {
            text(cards[i].reset, "时间未同步");
            text(cards[i].countdown, "--");
        } else {
            int64_t seconds = q->resets_at > view->now ? q->resets_at - view->now : 0;
            char detail[64];
            if (i) {
                if (seconds >= 86400) snprintf(detail, sizeof(detail), "%lld天后重置", (long long)((seconds + 86399) / 86400));
                else snprintf(detail, sizeof(detail), "%lld小时后重置", (long long)((seconds + 3599) / 3600));
                text(cards[i].reset, seconds ? detail : "等待重置");
                if (seconds >= 86400) snprintf(detail, sizeof(detail), "剩余 %lld天", (long long)((seconds + 86399) / 86400));
                else snprintf(detail, sizeof(detail), "剩余 %lld小时", (long long)((seconds + 3599) / 3600));
            } else {
                time_t when = (time_t)q->resets_at;
                struct tm local;
                char reset_time[8] = "--:--";
                if (localtime_r(&when, &local)) strftime(reset_time, sizeof(reset_time), "%H:%M", &local);
                snprintf(detail, sizeof(detail), "重置时间 %s", reset_time);
                text(cards[i].reset, detail);
                snprintf(detail, sizeof(detail), "还有 %lld小时%lld分", (long long)(seconds / 3600), (long long)(seconds % 3600 / 60));
            }
            text(cards[i].countdown, detail);
        }
    }
}

static void compact_number(uint64_t value, bool available, char *out, size_t size)
{
    if (!available) snprintf(out, size, "--");
    else if (value >= 1000000000) snprintf(out, size, "%.1fB", (double)value / 1000000000);
    else if (value >= 1000000) snprintf(out, size, "%.1fM", (double)value / 1000000);
    else if (value >= 100000) snprintf(out, size, "%.0fK", (double)value / 1000);
    else if (value >= 10000) snprintf(out, size, "%.1fK", (double)value / 1000);
    else if (value >= 1000) snprintf(out, size, "%" PRIu64 ",%03" PRIu64, value / 1000, value % 1000);
    else snprintf(out, size, "%" PRIu64, value);
}

void ui_dashboard_update_cost(const ui_cost_view_t *view)
{
    if (!view || !cost_dashboard.page) return;
    char value[48];
    if (view->has_cost && isfinite(view->today_cost_usd) && view->today_cost_usd >= 0) {
        if (view->today_cost_usd < 1000) snprintf(value, sizeof(value), "$%.2f", view->today_cost_usd);
        else snprintf(value, sizeof(value), "$%.1fK", view->today_cost_usd / 1000);
    } else snprintf(value, sizeof(value), "$--");
    text(cost_total, value);
    bool comparison = view->has_comparison && isfinite(view->yesterday_change_percent);
    double change = comparison ? view->yesterday_change_percent : 0;
    if (comparison != has_comparison || change != change_percent) {
        has_comparison = comparison;
        change_percent = change;
        invalidate(cost_dashboard.page, 300, 153, 30, 37);
    }
    if (comparison) snprintf(value, sizeof(value), "%+.0f%%", change);
    else snprintf(value, sizeof(value), "--");
    text(cost_change, value);
    lv_color_t color = lv_color_hex(change > 0 ? PINK : GREEN);
    if (!lv_color_eq(lv_obj_get_style_text_color(cost_change, 0), color)) lv_obj_set_style_text_color(cost_change, color, 0);
    compact_number(view->total_tokens, view->has_tokens, value, sizeof(value));
    text(cost_metrics[0], value);
    compact_number(view->unpriced_tokens, view->has_unpriced_tokens, value, sizeof(value));
    text(cost_metrics[1], value);
    compact_number(view->conversation_count, view->has_conversations, value, sizeof(value));
    text(cost_metrics[2], value);
    double max = 0;
    for (unsigned i = 0; i < 24; i++) {
        if (view->has_hourly && isfinite(view->hourly_cost_usd[i]) && view->hourly_cost_usd[i] > max) max = view->hourly_cost_usd[i];
    }
    bool changed = false;
    for (unsigned i = 0; i < 24; i++) {
        double cost = view->hourly_cost_usd[i];
        int h = max > 0 && isfinite(cost) && cost > 0 ? (int)(CHART_HEIGHT * (cost / max)) : 0;
        if (h == 0 && max > 0 && isfinite(cost) && cost > 0) h = 1;
        if (bar_heights[i] != h) { bar_heights[i] = h; changed = true; }
    }
    bool empty = !view->has_hourly;
    if (empty != !lv_obj_has_flag(chart_empty, LV_OBJ_FLAG_HIDDEN)) {
        if (empty) lv_obj_remove_flag(chart_empty, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(chart_empty, LV_OBJ_FLAG_HIDDEN);
    }
    if (changed) invalidate(cost_dashboard.page, CHART_X, CHART_Y - CHART_HEIGHT, CHART_WIDTH, CHART_HEIGHT + 1);
}
