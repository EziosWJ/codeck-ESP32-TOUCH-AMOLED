#include "account_client.h"
#include "quota_data.h"
#include "ui_home.h"
#include "sdkconfig.h"

#if !CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH
#error "Enable CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH in menuconfig"
#endif

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if __has_include("config.local.h")
#include "config.local.h"
#else
#define CODECK_WIFI_SSID ""
#define CODECK_WIFI_PASSWORD ""
#define CODECK_API_URL "http://codeck.wangj.de/api/account"
#define CODECK_API_USERNAME ""
#define CODECK_API_PASSWORD ""
#endif

#define CONNECTED_BIT BIT0
#define TIME_SYNCED_BIT BIT1
#define POLL_INTERVAL_MS 60000
#define RECONNECT_INTERVAL_MS 5000

static const char *TAG = "account_client";
static EventGroupHandle_t events;
static TaskHandle_t request_task_handle;
static bool started;
static portMUX_TYPE view_lock = portMUX_INITIALIZER_UNLOCKED;
static quota_view_t latest_view;

static void publish(bool online, const quota_data_t *quota)
{
    portENTER_CRITICAL(&view_lock);
    latest_view.online = online;
    if (quota) {
        latest_view.has_quota = true;
        latest_view.quota = *quota;
    }
    portEXIT_CRITICAL(&view_lock);
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(events, CONNECTED_BIT);
        publish(false, NULL);
        if (request_task_handle) xTaskNotifyGive(request_task_handle);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(events, CONNECTED_BIT);
        esp_err_t err = esp_netif_sntp_start();
        if (err != ESP_OK) ESP_LOGW(TAG, "SNTP start failed: %s", esp_err_to_name(err));
        if (request_task_handle) xTaskNotifyGive(request_task_handle);
        ESP_LOGI(TAG, "Wi-Fi has IP; scheduling account refresh");
    }
}

static void time_sync(struct timeval *tv)
{
    (void)tv;
    xEventGroupSetBits(events, TIME_SYNCED_BIT);
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
        quota_response_t *response = event->user_data;
        if (!quota_response_append(response, event->data, (size_t)event->data_len)) return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void fetch_account(void)
{
    quota_response_t response = {.data = malloc(QUOTA_RESPONSE_MAX_BYTES + 1)};
    if (!response.data) {
        ESP_LOGW(TAG, "Cannot allocate response buffer");
        publish(false, NULL);
        return;
    }
    response.data[0] = '\0';
    esp_http_client_config_t config = {
        .url = CODECK_API_URL,
        .user_agent = "Codeck/1.0",
        .username = CODECK_API_USERNAME,
        .password = CODECK_API_PASSWORD,
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .disable_auto_redirect = true,
        .max_authorization_retries = -1,
        .event_handler = http_event,
        .user_data = &response,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(response.data);
        publish(false, NULL);
        ESP_LOGW(TAG, "HTTP client initialization failed");
        return;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    quota_data_t quota;
    bool success = err == ESP_OK && status == 200 && !response.overflow &&
                   quota_parse_json(response.data, response.length, &quota) &&
                   (xEventGroupGetBits(events) & CONNECTED_BIT);
    if (success) {
        publish(true, &quota);
        ESP_LOGI(TAG, "Quota refreshed: %d%% remaining", quota.remaining_percent);
    } else {
        publish(false, NULL);
        ESP_LOGW(TAG, "Account refresh failed: transport=%s HTTP=%d overflow=%d",
                 esp_err_to_name(err), status, response.overflow);
    }
    esp_http_client_cleanup(client);
    free(response.data);
}

static void request_task(void *arg)
{
    (void)arg;
    TickType_t last_connect = xTaskGetTickCount() - pdMS_TO_TICKS(RECONNECT_INTERVAL_MS);
    for (;;) {
        /* Drain the notification which brought us here before starting a request. */
        ulTaskNotifyTake(pdTRUE, 0);
        if (xEventGroupGetBits(events) & CONNECTED_BIT) {
            fetch_account();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(POLL_INTERVAL_MS));
        } else {
            TickType_t elapsed = xTaskGetTickCount() - last_connect;
            TickType_t interval = pdMS_TO_TICKS(RECONNECT_INTERVAL_MS);
            if (elapsed >= interval) {
                last_connect = xTaskGetTickCount();
                esp_err_t err = esp_wifi_connect();
                if (err != ESP_OK) ESP_LOGW(TAG, "Wi-Fi connect failed: %s", esp_err_to_name(err));
                ulTaskNotifyTake(pdTRUE, interval);
            } else {
                ulTaskNotifyTake(pdTRUE, interval - elapsed);
            }
        }
    }
}

static network_view_t read_network_view(void)
{
    network_view_t view = {.configured = CODECK_WIFI_SSID[0] != '\0'};
    snprintf(view.ssid, sizeof(view.ssid), "%s", CODECK_WIFI_SSID);
    wifi_ap_record_t ap;
    esp_netif_ip_info_t ip;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if ((xEventGroupGetBits(events) & CONNECTED_BIT) && netif &&
        esp_wifi_sta_get_ap_info(&ap) == ESP_OK &&
        esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr &&
        (xEventGroupGetBits(events) & CONNECTED_BIT)) {
        view.connected = true;
        memcpy(view.ssid, ap.ssid, sizeof(ap.ssid));
        view.ssid[32] = '\0';
        view.rssi = ap.rssi;
        snprintf(view.ip, sizeof(view.ip), IPSTR, IP2STR(&ip.ip));
    }
    return view;
}

static void ui_task(void *arg)
{
    (void)arg;
    for (;;) {
        quota_view_t view;
        portENTER_CRITICAL(&view_lock);
        view = latest_view;
        portEXIT_CRITICAL(&view_lock);
        view.online = view.online && (xEventGroupGetBits(events) & CONNECTED_BIT);
        view.time_synced = (xEventGroupGetBits(events) & TIME_SYNCED_BIT) != 0;
        view.now = (int64_t)time(NULL);
        network_view_t network = read_network_view();
        if (lvgl_port_lock(100)) {
            ui_home_update(&view);
            ui_home_update_network(&network);
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t account_client_start(void)
{
    if (started) return ESP_ERR_INVALID_STATE;
    if (!CODECK_WIFI_SSID[0] || !CODECK_API_USERNAME[0] || !CODECK_API_PASSWORD[0]) {
        ESP_LOGW(TAG, "Fill main/config.local.h to enable Wi-Fi and account requests");
        return ESP_OK;
    }
    if (strlen(CODECK_WIFI_SSID) > 32 || strlen(CODECK_WIFI_PASSWORD) > 64 ||
        strncmp(CODECK_API_URL, "http://", 7) != 0) return ESP_ERR_INVALID_ARG;

    esp_err_t err = nvs_flash_init();
    /* Preserve NVS on initialization errors instead of silently erasing data. */
    if (err != ESP_OK) return err;
    err = esp_netif_init();
    if (err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_t *netif = NULL;
    TaskHandle_t ui_task_handle = NULL;
    bool wifi_initialized = false, sntp_initialized = false;
    events = xEventGroupCreate();
    if (!events) return ESP_ERR_NO_MEM;
    netif = esp_netif_create_default_wifi_sta();
    if (!netif) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) goto fail;
    wifi_initialized = true;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail;
    err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event, NULL);
    if (err != ESP_OK) goto fail;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL);
    if (err != ESP_OK) goto fail;
    if (setenv("TZ", "CST-8", 1) != 0) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    tzset();
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    sntp.start = false;
    sntp.sync_cb = time_sync;
    err = esp_netif_sntp_init(&sntp);
    if (err != ESP_OK) goto fail;
    sntp_initialized = true;

    wifi_config_t wifi = {0};
    memcpy(wifi.sta.ssid, CODECK_WIFI_SSID, strlen(CODECK_WIFI_SSID));
    memcpy(wifi.sta.password, CODECK_WIFI_PASSWORD, strlen(CODECK_WIFI_PASSWORD));
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_start();
    if (err != ESP_OK) goto fail;
    if (xTaskCreate(ui_task, "quota_ui", 4096, NULL, 3, &ui_task_handle) != pdPASS ||
        xTaskCreate(request_task, "account_http", 8192, NULL, 4, &request_task_handle) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    started = true;
    return ESP_OK;

fail:
    /* A failed start must not leave background tasks using partially initialized state. */
    if (ui_task_handle) vTaskDelete(ui_task_handle);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event);
    if (wifi_initialized) {
        esp_wifi_stop();
        esp_wifi_deinit();
    }
    if (sntp_initialized) esp_netif_sntp_deinit();
    if (netif) esp_netif_destroy_default_wifi(netif);
    vEventGroupDelete(events);
    events = NULL;
    return err;
}
