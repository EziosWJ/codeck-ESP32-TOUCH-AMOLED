#pragma once
#include "esp_err.h"
#include "quota_data.h"
#include "network_status.h"
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
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip, gw, netmask; } esp_netif_ip_info_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(ip) ((unsigned char *)&(ip)->addr)[0], ((unsigned char *)&(ip)->addr)[1], \
                   ((unsigned char *)&(ip)->addr)[2], ((unsigned char *)&(ip)->addr)[3]
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key);
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *info);
bool esp_netif_is_netif_up(esp_netif_t *netif);
typedef struct { bool start; void (*sync_cb)(struct timeval *); } esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(server) ((esp_sntp_config_t){.start = true})
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config);
esp_err_t esp_netif_sntp_start(void);
void esp_netif_sntp_deinit(void);
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct {
    struct { uint8_t ssid[32], password[64]; } sta;
    struct { uint8_t ssid[32], password[64]; unsigned ssid_len; int authmode, max_connection, channel; } ap;
} wifi_config_t;
typedef struct { uint8_t ssid[33]; int8_t rssi; int authmode; } wifi_ap_record_t;
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap);
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
    esp_err_t (*crt_bundle_attach)(void *);
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
void ui_home_update_network(const network_view_t *view);
const char *esp_err_to_name(esp_err_t error);
void host_log(const char *tag, const char *format, ...);
#define ESP_LOGI host_log
#define ESP_LOGW host_log
#define ESP_LOGE host_log

typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned count, unsigned size);
int xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait);
int xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait);
void vQueueDelete(QueueHandle_t queue);
#define WIFI_MODE_APSTA 2
#define WIFI_IF_AP 2
#define WIFI_AUTH_WPA2_PSK 3
#define ESP_MAC_WIFI_SOFTAP 1
esp_err_t esp_wifi_disconnect(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
esp_err_t esp_netif_dhcps_stop(esp_netif_t *netif);
esp_err_t esp_netif_dhcps_start(esp_netif_t *netif);
esp_err_t esp_netif_set_ip_info(esp_netif_t *netif, const esp_netif_ip_info_t *ip);
typedef struct { struct { int type; union { esp_ip4_addr_t ip4; } u_addr; } ip; } esp_netif_dns_info_t;
#define ESP_IPADDR_TYPE_V4 0
#define ESP_NETIF_DNS_MAIN 0
#define ESP_NETIF_OP_SET 0
#define ESP_NETIF_DOMAIN_NAME_SERVER 0
#define IP4_ADDR(ip,a,b,c,d) ((ip)->addr = ((uint32_t)(d)<<24)|((uint32_t)(c)<<16)|((uint32_t)(b)<<8)|(a))
esp_err_t esp_netif_set_dns_info(esp_netif_t *netif, int type, const esp_netif_dns_info_t *dns);
esp_err_t esp_netif_dhcps_option(esp_netif_t *netif, int op, int id, void *value, unsigned size);
esp_err_t esp_read_mac(uint8_t *mac, int type);
uint32_t esp_random(void);
esp_err_t esp_crt_bundle_attach(void *config);

#define WIFI_AUTH_OPEN 0
typedef struct { bool show_hidden; } wifi_scan_config_t;
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config,bool block);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count,wifi_ap_record_t *records);
esp_err_t esp_wifi_clear_ap_list(void);
esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *netif,char *name);

#define WIFI_EVENT_STA_CONNECTED 4
typedef struct { uint8_t ssid[32]; uint8_t ssid_len; } wifi_event_sta_connected_t;
