#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

/** Initialize the board's shared I2C bus and return its master handle. */
esp_err_t board_i2c_init(i2c_master_bus_handle_t *out_bus);
