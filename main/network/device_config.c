#include "device_config.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void device_config_defaults(device_config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->version = DEVICE_CONFIG_VERSION;
    snprintf(c->api_url, sizeof(c->api_url), "%s", DEVICE_API_URL_DEFAULT);
}

bool device_config_valid(const device_config_t *c)
{
    if (!c || c->version != DEVICE_CONFIG_VERSION ||
        !memchr(c->ssid, 0, sizeof(c->ssid)) || !c->ssid[0] ||
        !memchr(c->wifi_password, 0, sizeof(c->wifi_password)) ||
        !memchr(c->api_url, 0, sizeof(c->api_url)) ||
        !memchr(c->api_username, 0, sizeof(c->api_username)) || !c->api_username[0] ||
        !memchr(c->api_password, 0, sizeof(c->api_password)) || !c->api_password[0]) return false;
    size_t n = strlen(c->wifi_password);
    if (n && (n < 8 || n > 64)) return false;
    if (n == 64) for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)c->wifi_password[i])) return false;
    const char *host = NULL;
    if (!strncmp(c->api_url, "http://", 7)) host = c->api_url + 7;
    if (!strncmp(c->api_url, "https://", 8)) host = c->api_url + 8;
    if (!host || !*host || *host == '/' || *host == '?' || *host == '#') return false;
    for (const char *p = c->api_url; *p; p++) if ((unsigned char)*p <= 32 || *p == 127 || *p == '#' || *p == '@') return false;
    return strchr(c->api_username, ':') == NULL;
}

esp_err_t device_config_load(device_config_t *c)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("codeck", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    size_t size = sizeof(*c);
    err = nvs_get_blob(handle, "config", c, &size);
    nvs_close(handle);
    if (err == ESP_OK && (size != sizeof(*c) || !device_config_valid(c))) return ESP_ERR_INVALID_ARG;
    return err;
}

esp_err_t device_config_save(const device_config_t *c)
{
    if (!device_config_valid(c)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("codeck", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(handle, "config", c, sizeof(*c));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t device_config_clear(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("codeck", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_erase_key(handle, "config");
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
