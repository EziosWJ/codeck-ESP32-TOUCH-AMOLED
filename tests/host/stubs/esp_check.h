#pragma once
#include "esp_err.h"
#define ESP_RETURN_ON_FALSE(condition, error, tag, ...) do { \
    (void)(tag); if (!(condition)) return (error); \
} while (0)
