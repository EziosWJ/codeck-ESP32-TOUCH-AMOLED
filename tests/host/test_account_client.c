/* Compile the production task/event logic with deterministic SDK substitutes. */
#include "../../main/network/account_client.c"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

esp_event_base_t WIFI_EVENT = "wifi", IP_EVENT = "ip";
static esp_http_client_config_t http_config;
static const char *body;
static int http_status = 200, http_calls, cleanup_calls, connect_calls;
static esp_err_t transport_error;
static bool oversize, disconnect_during_fetch, reconnect_success = true;
static TickType_t ticks, waits[8], connect_ticks[8];
static unsigned wait_count;
static int task_scenario;
static jmp_buf task_finished;
static unsigned task_creates, task_deletes, fail_task_create, wifi_stops, sntp_deinits;
static esp_err_t ap_error;

EventGroupHandle_t xEventGroupCreate(void) { return calloc(1, sizeof(EventBits_t)); }
EventBits_t xEventGroupGetBits(EventGroupHandle_t group) { return *group; }
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits) { return *group |= bits; }
EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits) { return *group &= ~bits; }
void vEventGroupDelete(EventGroupHandle_t group) { free(group); }
void xTaskNotifyGive(TaskHandle_t task) { (void)task; }
TickType_t xTaskGetTickCount(void) { return ticks; }
uint32_t ulTaskNotifyTake(int clear, TickType_t timeout)
{
    (void)clear;
    if (!timeout) return 0;
    assert(wait_count < 8);
    waits[wait_count++] = timeout;
    if (task_scenario == 1) {
        if (wait_count == 1) {
            assert(timeout == 60000);
            ticks += 1000;
            wifi_event(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
        } else if (wait_count == 3) longjmp(task_finished, 1);
    } else if (task_scenario == 2) {
        if (connect_calls == 3) longjmp(task_finished, 1);
        ticks += (timeout == 5000 ? 1 : timeout);
    }
    return 1;
}
void vTaskDelay(TickType_t delay) { ticks += delay; }
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, int priority, TaskHandle_t *handle)
{
    (void)task; (void)name; (void)stack; (void)arg; (void)priority;
    if (++task_creates == fail_task_create) return 0;
    if (handle) *handle = (void *)1;
    return pdPASS;
}
void vTaskDelete(TaskHandle_t handle) { (void)handle; task_deletes++; }
esp_err_t esp_event_loop_create_default(void) { return ESP_OK; }
esp_err_t esp_event_handler_register(esp_event_base_t base, int id, esp_event_handler_t handler, void *arg)
{ (void)base; (void)id; (void)handler; (void)arg; return ESP_OK; }
esp_err_t esp_event_handler_unregister(esp_event_base_t base, int id, esp_event_handler_t handler)
{ (void)base; (void)id; (void)handler; return ESP_OK; }
esp_err_t esp_netif_init(void) { return ESP_OK; }
esp_netif_t *esp_netif_create_default_wifi_sta(void) { static esp_netif_t netif; return &netif; }
void esp_netif_destroy_default_wifi(esp_netif_t *netif) { (void)netif; }
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key)
{ assert(strcmp(key, "WIFI_STA_DEF") == 0); return esp_netif_create_default_wifi_sta(); }
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *info)
{
    (void)netif;
    const unsigned char address[] = {192, 168, 1, 42};
    memcpy(&info->ip.addr, address, sizeof(address));
    return ESP_OK;
}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap)
{
    memset(ap, 0, sizeof(*ap));
    memcpy(ap->ssid, "Test Wi-Fi", sizeof("Test Wi-Fi"));
    ap->rssi = -57;
    return ap_error;
}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config) { assert(!config->start); return ESP_OK; }
esp_err_t esp_netif_sntp_start(void) { return ESP_OK; }
void esp_netif_sntp_deinit(void) { sntp_deinits++; }
esp_err_t esp_wifi_init(const wifi_init_config_t *config) { (void)config; return ESP_OK; }
esp_err_t esp_wifi_set_storage(int storage) { assert(storage == WIFI_STORAGE_RAM); return ESP_OK; }
esp_err_t esp_wifi_set_mode(int mode) { assert(mode == WIFI_MODE_STA); return ESP_OK; }
esp_err_t esp_wifi_set_config(int interface, wifi_config_t *config) { (void)interface; (void)config; return ESP_OK; }
esp_err_t esp_wifi_start(void) { return ESP_OK; }
esp_err_t esp_wifi_stop(void) { wifi_stops++; return ESP_OK; }
esp_err_t esp_wifi_deinit(void) { return ESP_OK; }
esp_err_t esp_wifi_connect(void)
{
    assert(connect_calls < 8);
    connect_ticks[connect_calls++] = ticks;
    if (reconnect_success) wifi_event(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    else wifi_event(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
    return ESP_OK;
}
esp_err_t nvs_flash_init(void) { return ESP_OK; }
bool lvgl_port_lock(int timeout) { (void)timeout; return true; }
void lvgl_port_unlock(void) {}
void ui_home_update(const quota_view_t *view) { (void)view; }
void ui_home_update_network(const network_view_t *view) { (void)view; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "mock"; }
void host_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
    assert(config->auth_type == HTTP_AUTH_TYPE_BASIC && config->method == HTTP_METHOD_GET);
    assert(config->timeout_ms == 10000 && config->disable_auto_redirect);
    assert(strcmp(config->user_agent, "Codeck/1.0") == 0);
    assert(config->max_authorization_retries == -1);
    assert(strcmp(config->username, "test-user") == 0);
    assert(strcmp(config->password, "test-secret") == 0);
    http_config = *config;
    return &http_config;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key, const char *value)
{ (void)client; assert(strcmp(key, "Accept") == 0 && strcmp(value, "application/json") == 0); return ESP_OK; }
esp_err_t esp_http_client_perform(esp_http_client_handle_t client)
{
    http_calls++;
    esp_http_client_event_t event = {.event_id = HTTP_EVENT_ON_DATA, .user_data = client->user_data};
    if (oversize) {
        char chunk[1024];
        memset(chunk, 'x', sizeof(chunk));
        event.data = chunk;
        event.data_len = sizeof(chunk);
        for (unsigned i = 0; i < 65; i++) client->event_handler(&event);
    } else {
        for (size_t offset = 0; body && offset < strlen(body); offset += 3) {
            event.data = (char *)body + offset;
            event.data_len = strlen(body) - offset > 3 ? 3 : (int)(strlen(body) - offset);
            assert(client->event_handler(&event) == ESP_OK);
        }
    }
    if (disconnect_during_fetch) wifi_event(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
    return transport_error;
}
int esp_http_client_get_status_code(esp_http_client_handle_t client) { (void)client; return http_status; }
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) { (void)client; cleanup_calls++; return ESP_OK; }

static void assert_retained(void)
{
    assert(!latest_view.online && latest_view.has_quota);
    assert(latest_view.quota.remaining_percent == 95);
    assert(latest_view.quota.resets_at == 1791120915);
}

int main(void)
{
    events = xEventGroupCreate();
    xEventGroupSetBits(events, CONNECTED_BIT);
    const char *sample = "{\"ok\":true,\"rate_limits\":{\"rateLimits\":{\"primary\":{\"usedPercent\":5,"
        "\"windowDurationMins\":300,\"resetsAt\":1791120915}}}}";
    body = sample;
    fetch_account();
    assert(latest_view.online && latest_view.quota.remaining_percent == 95);
    http_status = 401;
    fetch_account();
    assert_retained();
    network_view_t network = read_network_view();
    assert(network.configured && network.connected);
    assert(strcmp(network.ssid, "Test Wi-Fi") == 0);
    assert(strcmp(network.ip, "192.168.1.42") == 0 && network.rssi == -57);
    /* An API failure must not imply Wi-Fi disconnection. */
    ap_error = ESP_ERR_TIMEOUT;
    network = read_network_view();
    assert(!network.connected && !network.ip[0]);
    ap_error = ESP_OK;
    wifi_event(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);
    network = read_network_view();
    assert(network.configured && !network.connected && !network.ip[0]);
    assert(strcmp(network.ssid, CODECK_WIFI_SSID) == 0);
    wifi_event(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    http_status = 302;
    fetch_account();
    assert_retained();
    http_status = 200;
    transport_error = ESP_ERR_TIMEOUT;
    fetch_account();
    assert_retained();
    transport_error = ESP_OK;
    body = "{broken";
    fetch_account();
    assert_retained();
    body = "{\"ok\":true}";
    fetch_account();
    assert_retained();
    oversize = true;
    fetch_account();
    assert_retained();
    oversize = false;
    body = sample;
    disconnect_during_fetch = true;
    fetch_account();
    assert_retained();
    disconnect_during_fetch = false;
    wifi_event(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    fetch_account();
    assert(latest_view.online);
    time_sync(NULL);
    assert(xEventGroupGetBits(events) & TIME_SYNCED_BIT);

    http_calls = connect_calls = 0;
    task_scenario = 1;
    if (!setjmp(task_finished)) request_task(NULL);
    assert(http_calls == 2 && connect_calls == 1);
    assert(waits[0] == 60000 && waits[1] == 5000 && waits[2] == 60000);
    connect_calls = 0;
    wait_count = 0;
    task_scenario = 2;
    reconnect_success = false;
    xEventGroupClearBits(events, CONNECTED_BIT);
    if (!setjmp(task_finished)) request_task(NULL);
    assert(connect_ticks[1] - connect_ticks[0] == 5000);
    assert(connect_ticks[2] - connect_ticks[1] == 5000);
    assert(http_calls + 9 == cleanup_calls);
    vEventGroupDelete(events);
    events = NULL;
    fail_task_create = 2;
    assert(account_client_start() == ESP_ERR_NO_MEM);
    assert(events == NULL && !started);
    assert(task_deletes == 1 && wifi_stops == 1 && sntp_deinits == 1);
    fail_task_create = 0;
    assert(account_client_start() == ESP_OK);
    assert(account_client_start() == ESP_ERR_INVALID_STATE);
    vEventGroupDelete(events);
    puts("Basic Auth settings, 401/redirect/timeout, retention, overflow and reconnect task tests passed");
    return 0;
}
