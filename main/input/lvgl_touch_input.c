#include "lvgl_touch_input.h"

#include "esp_check.h"

static const char *TAG = "lvgl_touch";
static ft3168_handle_t s_touch;
static lv_point_t s_last_point;

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    ft3168_data_t touch_data = {0};
    esp_err_t err = ft3168_read(s_touch, &touch_data);
    if (err == ESP_OK && touch_data.pressed) {
        lv_display_t *display = lv_indev_get_display(indev);
        const int32_t hres = lv_display_get_horizontal_resolution(display);
        const int32_t vres = lv_display_get_vertical_resolution(display);
        s_last_point.x = touch_data.x >= hres ? hres - 1 : touch_data.x;
        s_last_point.y = touch_data.y >= vres ? vres - 1 : touch_data.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point = s_last_point;
}

esp_err_t lvgl_touch_input_register(lv_display_t *display, ft3168_handle_t touch)
{
    ESP_RETURN_ON_FALSE(display != NULL, ESP_ERR_INVALID_ARG, TAG, "display is NULL");
    ESP_RETURN_ON_FALSE(touch != NULL, ESP_ERR_INVALID_ARG, TAG, "touch is NULL");

    lv_indev_t *indev = lv_indev_create();
    ESP_RETURN_ON_FALSE(indev != NULL, ESP_ERR_NO_MEM, TAG, "create LVGL input device");

    s_touch = touch;
    s_last_point = (lv_point_t){0};
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_scroll_limit(indev, 8);
    return ESP_OK;
}
