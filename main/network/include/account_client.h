#pragma once
#include "device_config.h"
#include <stddef.h>
/* Start after network_manager_start() and ui_home_init(). */
esp_err_t account_client_start(void);
/* Background worker only: bounded HTTP request, validates and publishes quota. */
bool account_client_check(const device_config_t *config, unsigned generation, char *message, size_t size);
