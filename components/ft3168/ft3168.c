#include "ft3168.h"

#include <stdlib.h>

#include "esp_check.h"
#include "esp_log.h"

#define FT3168_I2C_ADDRESS 0x38
#define FT3168_I2C_CLOCK_HZ 600000

struct ft3168_device {
    i2c_master_dev_handle_t i2c_device;
};

static const char *TAG = "ft3168";

esp_err_t ft3168_init(i2c_master_bus_handle_t bus, ft3168_handle_t *out_touch)
{
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_ERR_INVALID_ARG, TAG, "I2C bus is NULL");
    ESP_RETURN_ON_FALSE(out_touch != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "out_touch is NULL");
    *out_touch = NULL;

    struct ft3168_device *touch = calloc(1, sizeof(*touch));
    ESP_RETURN_ON_FALSE(touch != NULL, ESP_ERR_NO_MEM, TAG, "allocate touch device");

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT3168_I2C_ADDRESS,
        .scl_speed_hz = FT3168_I2C_CLOCK_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &device_config, &touch->i2c_device);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add FT3168 to I2C bus: %s", esp_err_to_name(err));
        free(touch);
        return err;
    }

    const uint8_t init_command[] = {0x00, 0x00};
    err = i2c_master_transmit(touch->i2c_device, init_command,
                              sizeof(init_command), 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialize FT3168: %s", esp_err_to_name(err));
        i2c_master_bus_rm_device(touch->i2c_device);
        free(touch);
        return err;
    }

    *out_touch = touch;
    ESP_LOGI(TAG, "FT3168 ready on I2C address 0x%02X", FT3168_I2C_ADDRESS);
    return ESP_OK;
}

esp_err_t ft3168_read(ft3168_handle_t touch, ft3168_data_t *out_data)
{
    ESP_RETURN_ON_FALSE(touch != NULL, ESP_ERR_INVALID_ARG, TAG, "touch is NULL");
    ESP_RETURN_ON_FALSE(out_data != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "out_data is NULL");
    *out_data = (ft3168_data_t){0};

    const uint8_t count_reg = 0x02;
    uint8_t count = 0;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(touch->i2c_device, &count_reg, 1,
                                                     &count, 1, 30),
                        TAG, "read touch count");

    if ((count & 0x0F) == 0) {
        return ESP_OK;
    }

    const uint8_t point_reg = 0x03;
    uint8_t raw[4] = {0};
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(touch->i2c_device, &point_reg, 1,
                                                     raw, sizeof(raw), 30),
                        TAG, "read touch coordinates");

    out_data->x = ((uint16_t)(raw[0] & 0x0F) << 8) | raw[1];
    out_data->y = ((uint16_t)(raw[2] & 0x0F) << 8) | raw[3];
    out_data->pressed = true;
    return ESP_OK;
}
