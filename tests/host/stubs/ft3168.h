#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef void *ft3168_handle_t;
typedef struct { bool pressed; uint16_t x, y; } ft3168_data_t;
esp_err_t ft3168_read(ft3168_handle_t touch, ft3168_data_t *data);
