#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define DEVICE_CONFIG_VERSION 1
#define DEVICE_API_URL_DEFAULT "http://codeck.wangj.de/api/account"
typedef struct {
    uint32_t version;
    char ssid[33];
    char wifi_password[65];
    char api_url[257];
    char api_username[129];
    char api_password[129];
} device_config_t;

void device_config_defaults(device_config_t *config);
bool device_config_valid(const device_config_t *config);
/* NOT_FOUND means first boot; all other storage errors are preserved. */
esp_err_t device_config_load(device_config_t *config);
esp_err_t device_config_save(const device_config_t *config);
esp_err_t device_config_clear(void);
