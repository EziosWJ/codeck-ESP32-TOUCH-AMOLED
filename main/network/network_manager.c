#include "network_manager.h"
#include "provision_server.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "lwip/inet.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define CMD_SUBMIT 3
#define RETRY_MS 5000
#define CONNECT_MS 30000
#define API_CHECK_MS 30000

typedef struct { int action; device_config_t config; } command_t;
static QueueHandle_t commands;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static device_config_t current, baseline, candidate;
static bool configured, baseline_configured, connected, started, trial, session_changed, associated;
static unsigned generation;
static provision_view_t provision;
static esp_netif_t *station, *access_point;
static TickType_t attempt_start, last_connect, api_check_start;

static void set_phase(provision_phase_t phase, const char *message)
{
    portENTER_CRITICAL(&state_lock);
    provision.phase = phase;
    snprintf(provision.message, sizeof(provision.message), "%s", message);
    portEXIT_CRITICAL(&state_lock);
}

unsigned network_manager_get_config(device_config_t *config, bool *has_config)
{
    portENTER_CRITICAL(&state_lock);
    *config = current;
    *has_config = configured;
    unsigned revision = generation;
    portEXIT_CRITICAL(&state_lock);
    return revision;
}

void network_manager_get_provision(provision_view_t *view)
{
    portENTER_CRITICAL(&state_lock);
    *view = provision;
    view->configured = configured;
    if (view->closing) {
        view->phase = PROVISION_STOPPING;
        snprintf(view->message, sizeof(view->message), "正在关闭热点，请稍候");
    }
    portEXIT_CRITICAL(&state_lock);
}

bool network_manager_connected(void)
{
    portENTER_CRITICAL(&state_lock);
    bool value = connected;
    portEXIT_CRITICAL(&state_lock);
    return value;
}

unsigned network_manager_generation(void)
{
    portENTER_CRITICAL(&state_lock);
    unsigned value = generation;
    portEXIT_CRITICAL(&state_lock);
    return value;
}

network_view_t network_manager_get_network(void)
{
    network_view_t view = {0};
    portENTER_CRITICAL(&state_lock);
    view.configured = configured;
    snprintf(view.ssid, sizeof(view.ssid), "%s", trial ? candidate.ssid : current.ssid);
    bool has_ip = connected;
    portEXIT_CRITICAL(&state_lock);
    wifi_ap_record_t ap;
    esp_netif_ip_info_t ip;
    if (has_ip && esp_wifi_sta_get_ap_info(&ap) == ESP_OK &&
        esp_netif_get_ip_info(station, &ip) == ESP_OK && ip.ip.addr && network_manager_connected()) {
        view.connected = true;
        memcpy(view.ssid, ap.ssid, 32);
        view.ssid[32] = 0;
        view.rssi = ap.rssi;
        snprintf(view.ip, sizeof(view.ip), IPSTR, IP2STR(&ip.ip));
    }
    return view;
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        portENTER_CRITICAL(&state_lock);
        connected = associated = false;
        generation++;
        portEXIT_CRITICAL(&state_lock);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED && data) {
        const wifi_event_sta_connected_t *event = data;
        portENTER_CRITICAL(&state_lock);
        const char *expected = trial ? candidate.ssid : current.ssid;
        associated = event->ssid_len == strlen(expected) &&
                     memcmp(event->ssid, expected, event->ssid_len) == 0;
        portEXIT_CRITICAL(&state_lock);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        /* Ignore delayed IP events from the network being replaced. */
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return;
        portENTER_CRITICAL(&state_lock);
        const char *expected = trial ? candidate.ssid : current.ssid;
        bool match = memcmp(ap.ssid, expected, strlen(expected)) == 0 &&
            (strlen(expected) == 32 || ap.ssid[strlen(expected)] == 0);
        match = match && associated;
        if (match && expected[0]) connected = true;
        portEXIT_CRITICAL(&state_lock);
        if (match) esp_netif_sntp_start();
    }
}

static esp_err_t use_station(const device_config_t *config)
{
    portENTER_CRITICAL(&state_lock);
    connected = associated = false;
    generation++;
    portEXIT_CRITICAL(&state_lock);
    esp_wifi_disconnect();
    wifi_config_t wifi = {0};
    memcpy(wifi.sta.ssid, config->ssid, strlen(config->ssid));
    memcpy(wifi.sta.password, config->wifi_password, strlen(config->wifi_password));
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi);
    last_connect = xTaskGetTickCount();
    if (err == ESP_OK && config->ssid[0]) err = esp_wifi_connect();
    return err;
}

static void restore_current(void)
{
    portENTER_CRITICAL(&state_lock);
    trial = false;
    portEXIT_CRITICAL(&state_lock);
    use_station(&current);
}

static esp_err_t begin_provision(void)
{
    baseline = current;
    baseline_configured = configured;
    session_changed = false;
    wifi_config_t ap = {0};
    memcpy(ap.ap.ssid, provision.ap_ssid, strlen(provision.ap_ssid));
    snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", provision.ap_password);
    ap.ap.ssid_len = strlen(provision.ap_ssid);
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap.ap.max_connection = 2;
    ap.ap.channel = 1;
    /* Configure a secured AP before starting it. Changing a running STA into
     * APSTA would briefly start the driver's default (possibly open) AP. */
    portENTER_CRITICAL(&state_lock);
    connected = associated = false;
    generation++;
    portEXIT_CRITICAL(&state_lock);
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK) err = provision_server_start();
    portENTER_CRITICAL(&state_lock);
    provision.active = err == ESP_OK;
    provision.saved = false;
    provision.api_ok = false;
    portEXIT_CRITICAL(&state_lock);
    if (err != ESP_OK) {
        provision_server_stop();
        esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        if (configured) restore_current();
    }
    set_phase(err == ESP_OK ? PROVISION_WAITING : PROVISION_FAILED,
              err == ESP_OK ? "手机连接热点后打开配网页面" : "热点启动失败，请重试");
    return err;
}

static void end_provision(bool cancel)
{
    if (cancel && session_changed) {
        esp_err_t err = baseline_configured ? device_config_save(&baseline) : device_config_clear();
        if (err != ESP_OK) {
            /* Stop a pending trial even when persistent rollback fails. */
            restore_current();
            portENTER_CRITICAL(&state_lock);
            provision.closing = false;
            portEXIT_CRITICAL(&state_lock);
            set_phase(PROVISION_FAILED, "恢复配置失败，请重试取消");
            return;
        }
        portENTER_CRITICAL(&state_lock);
        current = baseline;
        configured = baseline_configured;
        generation++;
        portEXIT_CRITICAL(&state_lock);
    }
    if (cancel) restore_current();
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        portENTER_CRITICAL(&state_lock);
        provision.closing = false;
        portEXIT_CRITICAL(&state_lock);
        set_phase(PROVISION_FAILED, "关闭热点失败，请重试取消");
        return;
    }
    provision_server_stop();
    portENTER_CRITICAL(&state_lock);
    provision.active = false;
    provision.closing = false;
    provision.saved = false;
    provision.api_ok = false;
    portEXIT_CRITICAL(&state_lock);
    set_phase(PROVISION_IDLE, configured ? "配置已保存，可重新配网" : "尚未配置，请开始配网");
}

esp_err_t network_manager_action(network_action_t action)
{
    if (!commands || action < NETWORK_PROVISION_START || action > NETWORK_PROVISION_FINISH) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&state_lock);
    bool ready = !provision.closing &&
        (action == NETWORK_PROVISION_START ? !provision.active : provision.active) &&
        (action != NETWORK_PROVISION_FINISH || (provision.saved && provision.phase == PROVISION_SAVED));
    if (ready && action != NETWORK_PROVISION_START) provision.closing = true;
    portEXIT_CRITICAL(&state_lock);
    if (!ready) return ESP_ERR_INVALID_STATE;
    command_t command = {.action = action};
    if (xQueueSend(commands, &command, 0) == pdTRUE) return ESP_OK;
    if (action != NETWORK_PROVISION_START) {
        portENTER_CRITICAL(&state_lock);
        provision.closing = false;
        portEXIT_CRITICAL(&state_lock);
    }
    return ESP_ERR_INVALID_STATE;
}

esp_err_t network_manager_submit(const device_config_t *config)
{
    if (!commands || !device_config_valid(config)) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    bool ready = provision.active && !provision.closing && provision.phase != PROVISION_CONNECTING && provision.phase != PROVISION_CHECKING_API;
    provision_phase_t previous = provision.phase;
    if (ready) provision.phase = PROVISION_CONNECTING;
    portEXIT_CRITICAL(&state_lock);
    if (!ready) return ESP_ERR_INVALID_STATE;
    command_t command = {.action = CMD_SUBMIT, .config = *config};
    if (xQueueSend(commands, &command, 0) != pdTRUE) {
        portENTER_CRITICAL(&state_lock);
        if (provision.active && !provision.closing && provision.phase == PROVISION_CONNECTING) provision.phase = previous;
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static void process_command(const command_t *command)
{
    provision_view_t view;
    network_manager_get_provision(&view);
    if (command->action == NETWORK_PROVISION_START && !view.active) begin_provision();
    else if (command->action == NETWORK_PROVISION_CANCEL && view.active) end_provision(true);
    else if (command->action == NETWORK_PROVISION_FINISH && view.active && view.saved && view.closing) end_provision(false);
    else if (command->action == CMD_SUBMIT && view.active) {
        portENTER_CRITICAL(&state_lock);
        candidate = command->config;
        trial = true;
        provision.saved = false;
        provision.api_ok = false;
        portEXIT_CRITICAL(&state_lock);
        attempt_start = xTaskGetTickCount();
        set_phase(PROVISION_CONNECTING, "正在连接 Wi-Fi，请稍候");
        if (use_station(&candidate) != ESP_OK) {
            restore_current();
            set_phase(PROVISION_FAILED, "Wi-Fi 连接失败，请检查配置");
        }
    }
}

static void network_tick(void)
{
    provision_server_poll();
    TickType_t now = xTaskGetTickCount();
    provision_view_t view;
    network_manager_get_provision(&view);
    if (view.closing) return;
    if (trial) {
        if (network_manager_connected()) {
            esp_err_t err = device_config_save(&candidate);
            if (err != ESP_OK) {
                restore_current();
                set_phase(PROVISION_FAILED, "保存失败，已保留原配置");
            } else {
                portENTER_CRITICAL(&state_lock);
                current = candidate;
                configured = true;
                trial = false;
                generation++;
                provision.saved = true;
                portEXIT_CRITICAL(&state_lock);
                session_changed = true;
                api_check_start = now;
                set_phase(PROVISION_CHECKING_API, "联网成功，配置已保存，验证 API 中");
            }
        } else if (now - attempt_start >= pdMS_TO_TICKS(CONNECT_MS)) {
            restore_current();
            set_phase(PROVISION_FAILED, "连接超时，请检查密码和网络");
        } else if (now - last_connect >= pdMS_TO_TICKS(RETRY_MS)) {
            last_connect = now;
            esp_wifi_connect();
        }
    } else if (!view.active && configured && !network_manager_connected() && now - last_connect >= pdMS_TO_TICKS(RETRY_MS)) {
        last_connect = now;
        esp_wifi_connect();
    }
    portENTER_CRITICAL(&state_lock);
    if (provision.active && !provision.closing && provision.phase == PROVISION_CHECKING_API &&
        now - api_check_start >= pdMS_TO_TICKS(API_CHECK_MS)) {
        provision.phase = PROVISION_SAVED;
        provision.api_ok = false;
        snprintf(provision.message, sizeof(provision.message), "配置已保存，API 验证超时");
    }
    portEXIT_CRITICAL(&state_lock);
}

void network_manager_api_result(unsigned checked_generation, bool ok, const char *message)
{
    portENTER_CRITICAL(&state_lock);
    if (generation == checked_generation && provision.active && !provision.closing && provision.phase == PROVISION_CHECKING_API) {
        provision.api_ok = ok;
        provision.phase = PROVISION_SAVED;
        snprintf(provision.message, sizeof(provision.message), "%s", message);
    }
    portEXIT_CRITICAL(&state_lock);
}

static void worker(void *arg)
{
    (void)arg;
    command_t command;
    for (;;) {
        if (xQueueReceive(commands, &command, pdMS_TO_TICKS(50)) == pdTRUE) process_command(&command);
        network_tick();
    }
}

esp_err_t network_manager_start(void)
{
    if (started) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err;
    device_config_defaults(&current);
    err = device_config_load(&current);
    configured = err == ESP_OK;
    if (!configured) device_config_defaults(&current);
    err = esp_netif_init();
    if (err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    station = esp_netif_create_default_wifi_sta();
    access_point = esp_netif_create_default_wifi_ap();
    if (!station || !access_point) {
        if (station) esp_netif_destroy_default_wifi(station);
        if (access_point) esp_netif_destroy_default_wifi(access_point);
        station = access_point = NULL;
        return ESP_ERR_NO_MEM;
    }
    esp_netif_ip_info_t ip = {0};
    IP4_ADDR(&ip.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(access_point);
    err = esp_netif_set_ip_info(access_point, &ip);
    if (err != ESP_OK) goto fail_netif;
    esp_netif_dns_info_t dns = {0};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ip.ip.addr;
    err = esp_netif_set_dns_info(access_point, ESP_NETIF_DNS_MAIN, &dns);
    if (err != ESP_OK) goto fail_netif;
    uint8_t offer_dns = 2;
    err = esp_netif_dhcps_option(access_point, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer_dns, sizeof(offer_dns));
    if (err != ESP_OK) goto fail_netif;
    err = esp_netif_dhcps_start(access_point);
    if (err != ESP_OK) goto fail_netif;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init);
    if (err != ESP_OK) goto fail_netif;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail_wifi;
    err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event, NULL);
    if (err != ESP_OK) goto fail_wifi;
    err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, wifi_event, NULL);
    if (err != ESP_OK) goto fail_handlers;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL);
    if (err != ESP_OK) goto fail_handlers;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) goto fail_handlers;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(provision.ap_ssid, sizeof(provision.ap_ssid), "Codeck-%02X%02X", mac[4], mac[5]);
    /* New unpredictable hotspot password each boot; never logged. */
    snprintf(provision.ap_password, sizeof(provision.ap_password), "%08lx%04lx",
             (unsigned long)esp_random(), (unsigned long)(esp_random() & 0xffff));
    set_phase(PROVISION_IDLE, configured ? "配置已保存，可重新配网" : "尚未配置，请开始配网");
    commands = xQueueCreate(1, sizeof(command_t));
    if (!commands) { err = ESP_ERR_NO_MEM; goto fail_handlers; }
    if (xTaskCreate(worker, "network", 8192, NULL, 4, NULL) != pdPASS) {
        vQueueDelete(commands); commands = NULL; err = ESP_ERR_NO_MEM; goto fail_handlers;
    }
    started = true;
    if (configured) use_station(&current);
    else network_manager_action(NETWORK_PROVISION_START);
    return ESP_OK;
fail_handlers:
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, wifi_event);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event);
fail_wifi:
    esp_wifi_stop();
    esp_wifi_deinit();
fail_netif:
    esp_netif_destroy_default_wifi(station);
    esp_netif_destroy_default_wifi(access_point);
    station = access_point = NULL;
    return err;
}
