#include "board_i2c.h"
#include "amoled_display.h"
#include "demo_ui.h"
#include "ft3168.h"
#include "esp_lvgl_port.h"
#include "lvgl_touch_input.h"

#include "esp_log.h"

static const char *TAG = "app_main";

void app_main(void)
{
    i2c_master_bus_handle_t i2c_bus = NULL;
    ft3168_handle_t touch = NULL;
    lv_display_t *display = NULL;

    esp_err_t i2c_err = board_i2c_init(&i2c_bus);
    if (i2c_err != ESP_OK) {
        ESP_LOGE(TAG, "Shared I2C initialization failed: %s", esp_err_to_name(i2c_err));
    }

    esp_err_t err = amoled_display_init(&display);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display initialization failed: %s", esp_err_to_name(err));
        return;
    }

    if (i2c_bus != NULL) {
        err = ft3168_init(i2c_bus, &touch);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "FT3168 initialization failed: %s", esp_err_to_name(err));
        }
    }

    if (!lvgl_port_lock(1000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL during application setup");
        return;
    }

    if (touch != NULL) {
        err = lvgl_touch_input_register(display, touch);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LVGL touch input registration failed: %s", esp_err_to_name(err));
        }
    }

    err = demo_ui_init(display);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Demo UI initialization failed: %s", esp_err_to_name(err));
        lvgl_port_unlock();
        return;
    }

    lvgl_port_unlock();
    ESP_LOGI(TAG, "Application ready: vertical scrolling and horizontal page flips enabled");
}
