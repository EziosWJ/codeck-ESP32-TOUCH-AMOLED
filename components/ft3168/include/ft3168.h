#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct ft3168_device *ft3168_handle_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} ft3168_data_t;

/** Attach an FT3168 to an already initialized shared I2C bus. */
esp_err_t ft3168_init(i2c_master_bus_handle_t bus, ft3168_handle_t *out_touch);

/** Read the current single-touch state and raw panel coordinates. */
esp_err_t ft3168_read(ft3168_handle_t touch, ft3168_data_t *out_data);
