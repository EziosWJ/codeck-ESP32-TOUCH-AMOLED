#include "ui_home.h"
#include "lvgl_touch_input.h"
#include "others/snapshot/lv_snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static lv_indev_data_t pointer;
static uint32_t flushed_pixels;
static ft3168_data_t touch_sample;
static esp_err_t touch_error;

esp_err_t ft3168_read(ft3168_handle_t touch, ft3168_data_t *data)
{
    assert(touch);
    *data = touch_sample;
    return touch_error;
}

static void read_pointer(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    *data = pointer;
}

static void advance(unsigned ms)
{
    for (unsigned i = 0; i < ms; i += 10) {
        lv_tick_inc(10);
        lv_timer_handler();
    }
}

static void stroke(lv_point_t start, lv_point_t end, unsigned step_ms)
{
    advance(100);
    flushed_pixels = 0;
    pointer.state = LV_INDEV_STATE_PRESSED;
    pointer.point = start;
    advance(50);
    for (int i = 1; i <= 10; i++) {
        pointer.point.x = start.x + (end.x - start.x) * i / 10;
        pointer.point.y = start.y + (end.y - start.y) * i / 10;
        advance(step_ms);
    }
    /* Immediate switching draws at most one full frame per stroke. */
    assert(flushed_pixels <= 466 * 466);
    flushed_pixels = 0;
    pointer.state = LV_INDEV_STATE_RELEASED;
    advance(600);
    if (abs(end.x - start.x) < 12 && abs(end.y - start.y) < 12) {
        assert(flushed_pixels <= 466 * 466);
    } else assert(flushed_pixels == 0);
}

static void swipe(int start, int end)
{
    stroke((lv_point_t){start, 230}, (lv_point_t){end, 230}, 40);
}

static void assert_active(lv_obj_t *active, lv_obj_t *hidden)
{
    assert(!lv_obj_has_flag(active, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(hidden, LV_OBJ_FLAG_HIDDEN));
}

static bool has_text(lv_obj_t *obj, const char *text)
{
    if (lv_obj_check_type(obj, &lv_label_class) &&
        strcmp(lv_label_get_text(obj), text) == 0) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        if (has_text(lv_obj_get_child(obj, i), text)) return true;
    }
    return false;
}

static uint32_t count_objects(lv_obj_t *obj)
{
    uint32_t count = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        count += count_objects(lv_obj_get_child(obj, i));
    }
    return count;
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    flushed_pixels += (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    (void)pixels;
    lv_display_flush_ready(display);
}

static void snapshot(lv_obj_t *screen, const char *filename)
{
    lv_obj_update_layout(screen);
    lv_draw_buf_t *image = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888);
    assert(image);
    FILE *file = fopen(filename, "wb");
    assert(file);
    fprintf(file, "P6\n%u %u\n255\n", image->header.w, image->header.h);
    for (uint32_t y = 0; y < image->header.h; y++) {
        const uint8_t *row = image->data + y * image->header.stride;
        for (uint32_t x = 0; x < image->header.w; x++) {
            uint8_t rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            fwrite(rgb, 1, sizeof(rgb), file);
        }
    }
    fclose(file);
    lv_draw_buf_destroy(image);
}

int main(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();
    lv_init();
    lv_display_t *display = lv_display_create(466, 466);
    assert(display);
    static uint8_t draw_buffer[466 * 32 * 2];
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    assert(ui_home_init(display) == ESP_OK);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_read_cb(indev, read_pointer);
    lv_indev_set_scroll_limit(indev, 8);
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_t *home = lv_obj_get_child(screen, 0);
    lv_obj_t *network_page = lv_obj_get_child(screen, 1);
    uint32_t objects = count_objects(screen);
    snapshot(screen, "ui_waiting.ppm");
    quota_view_t view = {.has_quota = true, .online = true, .time_synced = true,
        .now = 1791117315, .quota = {95, 300, 1791120915}};
    ui_home_update(&view);
    snapshot(screen, "ui_95.ppm");
    network_view_t network = {.configured = true, .connected = true,
        .ssid = "Codeck Wi-Fi", .ip = "192.168.1.42", .rssi = -57};
    ui_home_update_network(&network);
    advance(100);
    flushed_pixels = 0;
    ui_home_update(&view);
    ui_home_update_network(&network);
    advance(100);
    assert(flushed_pixels == 0);
    view.now++;
    ui_home_update(&view);
    advance(100);
    /* A countdown tick should not invalidate the full-screen quota ring. */
    assert(flushed_pixels < 466 * 466 / 2);
    swipe(360, 90);
    assert_active(network_page, home);
    assert(has_text(network_page, "已连接"));
    assert(has_text(network_page, "Codeck Wi-Fi"));
    assert(has_text(network_page, "IP: 192.168.1.42"));
    snapshot(screen, "ui_network_online.ppm");
    view.online = false;
    ui_home_update(&view);
    assert(has_text(network_page, "已连接"));
    assert(has_text(network_page, "额度服务: 不可用"));
    network.connected = false;
    ui_home_update_network(&network);
    assert(has_text(network_page, "未连接"));
    assert(has_text(network_page, "IP: --"));
    assert(has_text(network_page, "信号: --"));
    snapshot(screen, "ui_network_offline.ppm");
    network.configured = false;
    network.ssid[0] = '\0';
    ui_home_update_network(&network);
    assert(has_text(network_page, "未配置"));
    snapshot(screen, "ui_network_unconfigured.ppm");
    swipe(90, 360);
    assert_active(home, network_page);
    /* Slow 64 px swipes across text and blank areas must work both ways. */
    const int rows[] = {80, 120, 190, 245, 290, 340, 380};
    for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        stroke((lv_point_t){270, rows[i]}, (lv_point_t){206, rows[i] + 5}, 100);
        assert_active(network_page, home);
        stroke((lv_point_t){206, rows[i]}, (lv_point_t){270, rows[i] - 5}, 100);
        assert_active(home, network_page);
    }
    stroke((lv_point_t){233, 180}, (lv_point_t){233, 280}, 40);
    assert_active(home, network_page);
    stroke((lv_point_t){233, 230}, (lv_point_t){210, 230}, 40);
    assert_active(home, network_page);
    /* A diagonal swipe switches before release; reversal cannot switch twice. */
    pointer.state = LV_INDEV_STATE_PRESSED;
    pointer.point = (lv_point_t){270, 220};
    advance(50);
    pointer.point = (lv_point_t){237, 241};
    advance(50);
    assert_active(network_page, home);
    pointer.point = (lv_point_t){340, 241};
    advance(50);
    assert_active(network_page, home);
    pointer.state = LV_INDEV_STATE_RELEASED;
    advance(100);
    /* The visible return control also works with a simple tap. */
    stroke((lv_point_t){233, 418}, (lv_point_t){233, 418}, 40);
    assert_active(home, network_page);

    /* Exercise the production touch adapter with brief losses and read errors. */
    lv_indev_delete(indev);
    assert(lvgl_touch_input_register(display, (ft3168_handle_t)1) == ESP_OK);
    touch_sample = (ft3168_data_t){.pressed = true, .x = 270, .y = 220};
    advance(40);
    touch_sample.pressed = false;
    advance(20);
    touch_sample = (ft3168_data_t){.pressed = true, .x = 238, .y = 235};
    advance(30);
    assert_active(network_page, home);
    touch_sample.pressed = false;
    advance(40);
    touch_sample = (ft3168_data_t){.pressed = true, .x = 200, .y = 220};
    advance(30);
    touch_error = ESP_ERR_INVALID_ARG;
    advance(20);
    touch_error = ESP_OK;
    touch_sample.x = 232;
    touch_sample.y = 235;
    advance(30);
    assert_active(home, network_page);
    touch_sample.pressed = false;
    advance(40);
    view.quota.remaining_percent = 100;
    ui_home_update(&view);
    snapshot(screen, "ui_100.ppm");
    view.quota.remaining_percent = 0;
    view.online = false;
    ui_home_update(&view);
    snapshot(screen, "ui_zero.ppm");
    for (unsigned i = 0; i < 1000; i++) {
        view.quota.remaining_percent = i % 101;
        view.now++;
        ui_home_update(&view);
        ui_home_update_network(&network);
        assert(count_objects(screen) == objects);
    }
    lv_display_delete(display);
    lv_deinit();
    puts("UI snapshots and 1000 updates without recreating controls passed");
    return 0;
}
