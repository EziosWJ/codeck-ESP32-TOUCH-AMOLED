#include "amoled_display.h"

#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_sh8601.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SH8601_ID 0x86
#define CO5300_ID 0xFF

static const char *TAG = "amoled_display";

// The 466x466 Waveshare panel needs sleep-out and brightness commands in
// addition to the generic SH8601 driver's pixel format setup.
static const sh8601_lcd_init_cmd_t s_sh8601_init_cmds[] = {
    {0x11, NULL, 0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, NULL, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

static const sh8601_lcd_init_cmd_t s_co5300_init_cmds[] = {
    {0x11, NULL, 0, 80},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 1},
    {0x63, (uint8_t[]){0xFF}, 1, 1},
    {0x51, (uint8_t[]){0x00}, 1, 1},
    {0x29, NULL, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

static void write_id_byte(uint8_t value)
{
    for (int bit = 0; bit < 8; bit++) {
        gpio_set_level(BOARD_LCD_DATA0, (value & 0x80) != 0);
        value <<= 1;
        gpio_set_level(BOARD_LCD_CLK, 0);
        gpio_set_level(BOARD_LCD_CLK, 1);
    }
}

static esp_err_t read_panel_id(uint8_t *out_id)
{
    ESP_RETURN_ON_FALSE(out_id != NULL, ESP_ERR_INVALID_ARG, TAG, "out_id is NULL");

    const gpio_config_t pins = {
        .pin_bit_mask = (1ULL << BOARD_LCD_CS) | (1ULL << BOARD_LCD_CLK) |
                        (1ULL << BOARD_LCD_DATA0) | (1ULL << BOARD_LCD_DATA1) |
                        (1ULL << BOARD_LCD_DATA2) | (1ULL << BOARD_LCD_DATA3) |
                        (1ULL << BOARD_LCD_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&pins), TAG, "configure panel ID pins");
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_LCD_CS, 0), TAG, "assert panel CS");
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_LCD_RST, 1), TAG, "release panel reset");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_LCD_RST, 0), TAG, "assert panel reset");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_LCD_RST, 1), TAG, "release panel reset");
    vTaskDelay(pdMS_TO_TICKS(120));

    write_id_byte(0x03);
    write_id_byte(0x00);
    write_id_byte(0xDA);
    write_id_byte(0x00);

    ESP_RETURN_ON_ERROR(gpio_set_direction(BOARD_LCD_DATA0, GPIO_MODE_INPUT),
                        TAG, "set panel data pin to input");
    uint8_t id = 0;
    for (int bit = 0; bit < 8; bit++) {
        ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_LCD_CLK, 0), TAG, "clock panel ID read");
        esp_rom_delay_us(1);
        id = (id << 1) | gpio_get_level(BOARD_LCD_DATA0);
        ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_LCD_CLK, 1), TAG, "clock panel ID read");
        esp_rom_delay_us(1);
    }
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_LCD_CS, 1), TAG, "deassert panel CS");

    *out_id = id;
    return ESP_OK;
}

static void round_flush_area(lv_area_t *area)
{
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
}

esp_err_t amoled_display_init(lv_display_t **out_display)
{
    ESP_RETURN_ON_FALSE(out_display != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "out_display is NULL");
    *out_display = NULL;

    uint8_t panel_id = 0;
    ESP_RETURN_ON_ERROR(read_panel_id(&panel_id), TAG, "read panel ID");
    ESP_LOGI(TAG, "LCD ID register 0xDA: 0x%02X", panel_id);
    if (panel_id != SH8601_ID && panel_id != CO5300_ID) {
        ESP_LOGE(TAG, "Unknown panel ID; expected 0x86 or 0xFF");
        return ESP_ERR_NOT_SUPPORTED;
    }
    const bool is_co5300 = panel_id == CO5300_ID;
    ESP_LOGI(TAG, "Using %s initialization", is_co5300 ? "CO5300" : "SH8601");

    const spi_bus_config_t bus_config = SH8601_PANEL_BUS_QSPI_CONFIG(
        BOARD_LCD_CLK, BOARD_LCD_DATA0, BOARD_LCD_DATA1,
        BOARD_LCD_DATA2, BOARD_LCD_DATA3,
        BOARD_LCD_H_RES * BOARD_LCD_DRAW_LINES * sizeof(uint16_t));
    ESP_LOGI(TAG, "Initializing QSPI bus");
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO),
                        TAG, "initialize QSPI bus");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config =
        SH8601_PANEL_IO_QSPI_CONFIG(BOARD_LCD_CS, NULL, NULL);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_HOST,
                                                 &io_config, &io_handle),
                        TAG, "create panel IO");

    sh8601_vendor_config_t vendor_config = {
        .init_cmds = is_co5300 ? s_co5300_init_cmds : s_sh8601_init_cmds,
        .init_cmds_size = is_co5300
                              ? sizeof(s_co5300_init_cmds) / sizeof(s_co5300_init_cmds[0])
                              : sizeof(s_sh8601_init_cmds) / sizeof(s_sh8601_init_cmds[0]),
        .flags.use_qspi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_LOGI(TAG, "Initializing AMOLED panel");
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle),
                        TAG, "create AMOLED panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_handle), TAG, "reset AMOLED panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_handle), TAG, "initialize AMOLED panel");
    if (is_co5300) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(panel_handle, 6, 0), TAG,
                            "set CO5300 panel gap");
    }

    const lvgl_port_cfg_t lvgl_config = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_config), TAG, "initialize LVGL port");

    const lvgl_port_display_cfg_t display_config = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = BOARD_LCD_H_RES * BOARD_LCD_DRAW_LINES,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .rounder_cb = round_flush_area,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
            /* The application uses the panel's native orientation. */
            .sw_rotate = false,
        },
    };
    *out_display = lvgl_port_add_disp(&display_config);
    ESP_RETURN_ON_FALSE(*out_display != NULL, ESP_ERR_NO_MEM, TAG,
                        "register LVGL display");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle, true), TAG,
                        "turn on AMOLED panel");
    ESP_LOGI(TAG, "AMOLED display ready (%d x %d)", BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    return ESP_OK;
}
