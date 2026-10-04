#include "account_client.h"
#include "network_manager.h"
#include "quota_data.h"
#include "ui_home.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "account_client";
static portMUX_TYPE view_lock = portMUX_INITIALIZER_UNLOCKED;
static quota_view_t latest_view;
static bool started, synced;
static unsigned latest_generation;

static void time_sync(struct timeval *tv)
{
    (void)tv;
    portENTER_CRITICAL(&view_lock);
    synced = true;
    portEXIT_CRITICAL(&view_lock);
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
        quota_response_t *r = event->user_data;
        if (!quota_response_append(r, event->data, (size_t)event->data_len)) return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool account_client_check(const device_config_t *config, unsigned generation, char *message, size_t size)
{
    if (!network_manager_connected() || generation != network_manager_generation()) {
        snprintf(message, size, "配置已保存，网络已断开");
        return false;
    }
    if (!strncmp(config->api_url, "https://", 8)) {
        /* Certificate validity needs a real clock. Never disable verification. */
        for (unsigned i = 0; time(NULL) < 1704067200 && i < 100; i++) vTaskDelay(pdMS_TO_TICKS(100));
        if (time(NULL) < 1704067200) {
            portENTER_CRITICAL(&view_lock);
            latest_view.online = false;
            portEXIT_CRITICAL(&view_lock);
            snprintf(message, size, "时间未同步，暂不能验证 HTTPS");
            return false;
        }
    }
    if (!network_manager_connected() || generation != network_manager_generation()) {
        snprintf(message, size, "配置已保存，网络已断开");
        return false;
    }
    quota_response_t response = {.data = malloc(QUOTA_RESPONSE_MAX_BYTES + 1)};
    if (!response.data) {
        portENTER_CRITICAL(&view_lock);
        latest_view.online = false;
        portEXIT_CRITICAL(&view_lock);
        snprintf(message, size, "API 内存不足");
        return false;
    }
    response.data[0] = 0;
    esp_http_client_config_t http = {
        .url = config->api_url, .username = config->api_username, .password = config->api_password,
        .user_agent = "Codeck/1.0", .auth_type = HTTP_AUTH_TYPE_BASIC,
        .method = HTTP_METHOD_GET, .timeout_ms = 10000, .disable_auto_redirect = true,
        .max_authorization_retries = -1, .event_handler = http_event, .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http);
    esp_err_t err = ESP_ERR_NO_MEM;
    int status = 0;
    quota_account_data_t quota;
    bool success = false;
    if (client) {
        esp_http_client_set_header(client, "Accept", "application/json");
        err = esp_http_client_perform(client);
        status = esp_http_client_get_status_code(client);
        success = err == ESP_OK && status == 200 && !response.overflow &&
                  quota_parse_account_json(response.data, response.length, &quota) &&
                  network_manager_connected() && generation == network_manager_generation();
        esp_http_client_cleanup(client);
    }
    if (generation == network_manager_generation()) {
        portENTER_CRITICAL(&view_lock);
        latest_view.online = success;
        latest_generation = generation;
        if (success) {
            latest_view.has_quota = true;
            latest_view.quota = quota.primary;
            latest_view.has_weekly_quota = quota.has_secondary && quota.secondary.window_minutes == 10080;
            latest_view.weekly_quota = quota.secondary;
        }
        portEXIT_CRITICAL(&view_lock);
    }
    free(response.data);
    if (success) snprintf(message, size, "联网成功，API 验证成功");
    else if (err != ESP_OK) snprintf(message, size, "联网成功，API 连接或证书验证失败");
    else if (status != 200) snprintf(message, size, "联网成功，API HTTP %d", status);
    else snprintf(message, size, "联网成功，API 数据无效");
    ESP_LOGI(TAG, "Account request: transport=%s HTTP=%d valid=%d", esp_err_to_name(err), status, success);
    return success;
}

static void request_task(void *arg)
{
    (void)arg;
    unsigned last_generation = (unsigned)-1;
    TickType_t last_request = xTaskGetTickCount() - pdMS_TO_TICKS(60000);
    for (;;) {
        provision_view_t provision;
        network_manager_get_provision(&provision);
        if (provision.active && provision.phase == PROVISION_CHECKING_API) {
            device_config_t config;
            bool configured;
            unsigned generation = network_manager_get_config(&config, &configured);
            char message[128];
            bool ok = configured && account_client_check(&config, generation, message, sizeof(message));
            network_manager_api_result(generation, ok, configured ? message : "配置不可用");
        } else if (!provision.active && network_manager_connected()) {
            unsigned generation = network_manager_generation();
            TickType_t now = xTaskGetTickCount();
            if (generation != last_generation || now - last_request >= pdMS_TO_TICKS(60000)) {
                device_config_t config;
                bool configured;
                generation = network_manager_get_config(&config, &configured);
                if (configured) {
                    char message[128];
                    account_client_check(&config, generation, message, sizeof(message));
                    last_generation = generation;
                    last_request = now;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ui_task(void *arg)
{
    (void)arg;
    for (;;) {
        quota_view_t view;
        portENTER_CRITICAL(&view_lock);
        view = latest_view;
        view.time_synced = synced;
        unsigned published_generation = latest_generation;
        portEXIT_CRITICAL(&view_lock);
        view.online = view.online && network_manager_connected() &&
                      published_generation == network_manager_generation();
        view.now = (int64_t)time(NULL);
        network_view_t network = network_manager_get_network();
        provision_view_t provision;
        network_manager_get_provision(&provision);
        if (lvgl_port_lock(100)) {
            ui_home_update(&view);
            ui_home_update_network(&network);
            ui_home_update_provision(&provision);
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void ui_action(network_action_t action, void *context)
{
    (void)context;
    network_manager_action(action);
}

esp_err_t account_client_start(void)
{
    if (started) return ESP_ERR_INVALID_STATE;
    if (setenv("TZ", "CST-8", 1) != 0) return ESP_ERR_NO_MEM;
    tzset();
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    sntp.start = false;
    sntp.sync_cb = time_sync;
    esp_err_t err = esp_netif_sntp_init(&sntp);
    bool sntp_ready = err == ESP_OK;
    if (!sntp_ready) ESP_LOGW(TAG, "SNTP initialization failed: %s", esp_err_to_name(err));
    if (sntp_ready && network_manager_connected()) esp_netif_sntp_start();
    if (!lvgl_port_lock(1000)) {
        if (sntp_ready) esp_netif_sntp_deinit();
        return ESP_ERR_TIMEOUT;
    }
    ui_home_set_network_action(ui_action, NULL);
    lvgl_port_unlock();
    TaskHandle_t ui_handle = NULL;
    if (xTaskCreate(ui_task, "quota_ui", 4096, NULL, 3, &ui_handle) != pdPASS) {
        if (sntp_ready) esp_netif_sntp_deinit();
        if (lvgl_port_lock(1000)) {
            ui_home_set_network_action(NULL, NULL);
            lvgl_port_unlock();
        }
        return ESP_ERR_NO_MEM;
    }
    started = true;
    /* The network/settings UI must survive a missing account worker. The
     * manager bounds API validation, so a saved configuration remains usable. */
    if (xTaskCreate(request_task, "account_http", 8192, NULL, 4, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
