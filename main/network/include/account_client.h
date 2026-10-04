#pragma once

#include "esp_err.h"

/* Start once, after ui_home_init(). Missing local Wi-Fi config leaves UI usable. */
esp_err_t account_client_start(void);
