#pragma once
#include "esp_err.h"
#include "quota_data.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

#define ESP_ERR_TIMEOUT 0x107
#define BIT0 1U
#define BIT1 2U
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
typedef uint32_t TickType_t;
typedef uint32_t EventBits_t;
typedef uint32_t *EventGroupHandle_t;
typedef void *TaskHandle_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupGetBits(EventGroupHandle_t group);
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits);
EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits);
void vEventGroupDelete(EventGroupHandle_t group);
void xTaskNotifyGive(TaskHandle_t task);
TickType_t xTaskGetTickCount(void);
uint32_t ulTaskNotifyTake(int clear, TickType_t timeout);
void vTaskDelay(TickType_t ticks);
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, int priority, TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t handle);

typedef const char *esp_event_base_t;
extern esp_event_base_t WIFI_EVENT, IP_EVENT;
#define WIFI_EVENT_STA_DISCONNECTED 1
#define IP_EVENT_STA_GOT_IP 2
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_register(esp_event_base_t base, int id, esp_event_handler_t handler, void *arg);
esp_err_t esp_event_handler_unregister(esp_event_base_t base, int id, esp_event_handler_t handler);
typedef int esp_netif_t;
esp_err_t esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
void esp_netif_destroy_default_wifi(esp_netif_t *netif);
typedef struct { bool start; void (*sync_cb)(struct timeval *); } esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(server) ((esp_sntp_config_t){.start = true})
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config);
esp_err_t esp_netif_sntp_start(void);
void esp_netif_sntp_deinit(void);
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct { struct { uint8_t ssid[32], password[64]; } sta; } wifi_config_t;
#define WIFI_STORAGE_RAM 1
#define WIFI_MODE_STA 1
#define WIFI_IF_STA 1
esp_err_t esp_wifi_init(const wifi_init_config_t *config);
esp_err_t esp_wifi_set_storage(int storage);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_set_config(int interface, wifi_config_t *config);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_connect(void);
esp_err_t nvs_flash_init(void);

typedef struct {
    int event_id;
    void *user_data;
    char *data;
    int data_len;
} esp_http_client_event_t;
typedef struct {
    const char *url, *username, *password, *user_agent;
    int auth_type, method, timeout_ms, max_authorization_retries;
    bool disable_auto_redirect;
    esp_err_t (*event_handler)(esp_http_client_event_t *);
    void *user_data;
} esp_http_client_config_t;
typedef esp_http_client_config_t *esp_http_client_handle_t;
#define HTTP_AUTH_TYPE_BASIC 1
#define HTTP_METHOD_GET 0
#define HTTP_EVENT_ON_DATA 3
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key, const char *value);
esp_err_t esp_http_client_perform(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);
bool lvgl_port_lock(int timeout);
void lvgl_port_unlock(void);
void ui_home_update(const quota_view_t *view);
const char *esp_err_to_name(esp_err_t error);
void host_log(const char *tag, const char *format, ...);
#define ESP_LOGI host_log
#define ESP_LOGW host_log
