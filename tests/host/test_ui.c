#include "ui_home.h"
#include "others/snapshot/lv_snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
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
    lv_obj_t *screen = lv_display_get_screen_active(display);
    uint32_t objects = lv_obj_get_child_count(screen);
    snapshot(screen, "ui_waiting.ppm");
    quota_view_t view = {.has_quota = true, .online = true, .time_synced = true,
        .now = 1791117315, .quota = {95, 300, 1791120915}};
    ui_home_update(&view);
    snapshot(screen, "ui_95.ppm");
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
        assert(lv_obj_get_child_count(screen) == objects);
    }
    lv_display_delete(display);
    lv_deinit();
    puts("UI snapshots and 1000 updates without recreating controls passed");
    return 0;
}
