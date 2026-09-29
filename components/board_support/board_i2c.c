#include "board_i2c.h"

#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "board_i2c";
static i2c_master_bus_handle_t s_bus;

esp_err_t board_i2c_init(i2c_master_bus_handle_t *out_bus)
{
    ESP_RETURN_ON_FALSE(out_bus != NULL, ESP_ERR_INVALID_ARG, TAG, "out_bus is NULL");

    if (s_bus == NULL) {
        const i2c_master_bus_config_t config = {
            .i2c_port = BOARD_I2C_PORT,
            .sda_io_num = BOARD_I2C_SDA,
            .scl_io_num = BOARD_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags.enable_internal_pullup = true,
        };
        ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &s_bus), TAG,
                            "create shared I2C bus");
        ESP_LOGI(TAG, "Shared I2C bus ready (port=%d, SDA=%d, SCL=%d)",
                 BOARD_I2C_PORT, BOARD_I2C_SDA, BOARD_I2C_SCL);
    }

    *out_bus = s_bus;
    return ESP_OK;
}
