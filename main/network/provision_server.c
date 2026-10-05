#include "provision_server.h"
#include "network_manager.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "lwip/sockets.h"
#include <net/if.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define AP_READY_MS 5000
static const char *TAG = "provision_server";

extern const char page_start[] asm("_binary_provision_html_start");
static httpd_handle_t server;
static int dns_socket = -1;
static char session_token[33];
static struct ifreq ap_interface;

static esp_err_t get_ready_ap_interface(void)
{
    memset(&ap_interface, 0, sizeof(ap_interface));
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) return ESP_ERR_INVALID_STATE;
    /* Wi-Fi start is asynchronous. Before AP_START adds the AP to lwIP,
     * its unassigned interface index can resolve to the STA interface. */
    TickType_t start = xTaskGetTickCount();
    while (!esp_netif_is_netif_up(ap)) {
        if (xTaskGetTickCount() - start >= pdMS_TO_TICKS(AP_READY_MS)) {
            ESP_LOGE(TAG, "Timed out waiting for the AP network interface");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return esp_netif_get_netif_impl_name(ap, ap_interface.ifr_name);
}

static esp_err_t json_reply(httpd_req_t *req, cJSON *json)
{
    char *body = json ? cJSON_PrintUnformatted(json) : NULL;
    cJSON_Delete(json);
    if (!body) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    free(body);
    return err;
}

static esp_err_t error_reply(httpd_req_t *req, const char *status, const char *message)
{
    httpd_resp_set_status(req, status);
    cJSON *json = cJSON_CreateObject();
    if (json) cJSON_AddStringToObject(json, "error", message);
    return json_reply(req, json);
}

static bool authorized(httpd_req_t *req)
{
    char token[40], origin[64];
    if (httpd_req_get_hdr_value_str(req, "X-Codeck-Token", token, sizeof(token)) != ESP_OK ||
        strcmp(token, session_token)) return false;
    size_t n = httpd_req_get_hdr_value_len(req, "Origin");
    if (n && (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) != ESP_OK ||
              strcmp(origin, "http://192.168.4.1"))) return false;
    return true;
}

static esp_err_t redirect(httpd_req_t *req, httpd_err_code_t code);

static esp_err_t root_get(httpd_req_t *req)
{
    char host[64];
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK &&
        strcmp(host, "192.168.4.1") && strcmp(host, "192.168.4.1:80")) return redirect(req, HTTPD_404_NOT_FOUND);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    return httpd_resp_send(req, page_start, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t config_get(httpd_req_t *req)
{
    device_config_t config;
    bool configured;
    network_manager_get_config(&config, &configured);
    cJSON *json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "ssid", config.ssid);
        cJSON_AddStringToObject(json, "api_url", config.api_url);
        cJSON_AddStringToObject(json, "api_username", config.api_username);
        cJSON_AddBoolToObject(json, "has_wifi_password", configured && config.wifi_password[0]);
        cJSON_AddBoolToObject(json, "has_api_password", configured && config.api_password[0]);
        cJSON_AddStringToObject(json, "token", session_token);
    }
    return json_reply(req, json);
}

static esp_err_t status_get(httpd_req_t *req)
{
    provision_view_t view;
    network_manager_get_provision(&view);
    cJSON *json = cJSON_CreateObject();
    if (json) {
        cJSON_AddNumberToObject(json, "phase", view.phase);
        cJSON_AddBoolToObject(json, "saved", view.saved);
        cJSON_AddBoolToObject(json, "connected", network_manager_connected());
        cJSON_AddBoolToObject(json, "api_ok", view.api_ok);
        cJSON_AddStringToObject(json, "message", view.message);
    }
    return json_reply(req, json);
}

static esp_err_t scan_get(httpd_req_t *req)
{
    if (!authorized(req)) return error_reply(req, "403 Forbidden", "请刷新配网页面");
    provision_view_t view;
    network_manager_get_provision(&view);
    if (view.closing || view.phase == PROVISION_CONNECTING || view.phase == PROVISION_CHECKING_API)
        return error_reply(req, "409 Conflict", "连接中，请稍候再扫描");
    wifi_scan_config_t scan = {.show_hidden = true};
    esp_err_t err = esp_wifi_scan_start(&scan, true);
    if (err != ESP_OK) return error_reply(req, "503 Service Unavailable", "扫描失败，请手动输入网络名称");
    uint16_t count = 20;
    wifi_ap_record_t *records = calloc(count, sizeof(*records));
    if (!records) {
        esp_wifi_clear_ap_list();
        return error_reply(req, "500 Internal Server Error", "内存不足");
    }
    err = esp_wifi_scan_get_ap_records(&count, records);
    cJSON *json = cJSON_CreateArray();
    if (err == ESP_OK && json) for (unsigned i = 0; i < count; i++) {
        char ssid[33];
        memcpy(ssid, records[i].ssid, 32); ssid[32] = 0;
        if (!ssid[0]) continue;
        cJSON *entry = cJSON_CreateObject();
        if (!entry) continue;
        cJSON_AddStringToObject(entry, "ssid", ssid);
        cJSON_AddNumberToObject(entry, "rssi", records[i].rssi);
        cJSON_AddBoolToObject(entry, "secure", records[i].authmode != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(json, entry);
    }
    free(records);
    if (err != ESP_OK) { cJSON_Delete(json); return error_reply(req, "503 Service Unavailable", "扫描失败"); }
    return json_reply(req, json);
}

static bool copy_field(cJSON *json, const char *key, char *target, size_t size)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!cJSON_IsString(item) || strlen(item->valuestring) >= size) return false;
    snprintf(target, size, "%s", item->valuestring);
    return true;
}

static esp_err_t submit_post(httpd_req_t *req)
{
    if (!authorized(req)) return error_reply(req, "403 Forbidden", "请刷新配网页面");
    if (req->content_len <= 0 || req->content_len > 4096)
        return error_reply(req, "400 Bad Request", "配置长度无效");
    char *body = calloc(1, req->content_len + 1);
    if (!body) return error_reply(req, "500 Internal Server Error", "内存不足");
    int offset = 0;
    while (offset < req->content_len) {
        int n = httpd_req_recv(req, body + offset, req->content_len - offset);
        if (n <= 0) { free(body); return error_reply(req, "408 Request Timeout", "提交超时，请重试"); }
        offset += n;
    }
    cJSON *json = cJSON_ParseWithLengthOpts(body, req->content_len + 1, NULL, true);
    free(body);
    device_config_t config;
    bool configured;
    network_manager_get_config(&config, &configured);
    bool keep_wifi = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "keep_wifi_password"));
    bool keep_api = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "keep_api_password"));
    bool valid = json && copy_field(json, "ssid", config.ssid, sizeof(config.ssid)) &&
        copy_field(json, "api_url", config.api_url, sizeof(config.api_url)) &&
        copy_field(json, "api_username", config.api_username, sizeof(config.api_username)) &&
        (keep_wifi ? configured : copy_field(json, "wifi_password", config.wifi_password, sizeof(config.wifi_password))) &&
        (keep_api ? configured : copy_field(json, "api_password", config.api_password, sizeof(config.api_password))) &&
        device_config_valid(&config);
    cJSON_Delete(json);
    if (!valid) return error_reply(req, "400 Bad Request", "检查网络名称、密码、API 地址和账号；Wi-Fi 密码可留空或填写 8–63 字节密码");
    esp_err_t err = network_manager_submit(&config);
    if (err != ESP_OK) return error_reply(req, "409 Conflict", "正在处理配置，请稍候");
    httpd_resp_set_status(req, "202 Accepted");
    return json_reply(req, cJSON_CreateObject());
}

static esp_err_t finish_post(httpd_req_t *req)
{
    if (!authorized(req)) return error_reply(req, "403 Forbidden", "请刷新配网页面");
    if (network_manager_action(NETWORK_PROVISION_FINISH) != ESP_OK)
        return error_reply(req, "409 Conflict", "请等待配置保存和 API 验证结束");
    return json_reply(req, cJSON_CreateObject());
}

static esp_err_t redirect(httpd_req_t *req, httpd_err_code_t code)
{
    (void)code;
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, "Open http://192.168.4.1/ to configure Codeck", HTTPD_RESP_USE_STRLEN);
}

esp_err_t provision_server_start(void)
{
    if (server) return ESP_ERR_INVALID_STATE;
    snprintf(session_token, sizeof(session_token), "%08lx%08lx%08lx%08lx",
        (unsigned long)esp_random(), (unsigned long)esp_random(),
        (unsigned long)esp_random(), (unsigned long)esp_random());
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.server_port = 80;
    /* Leave sockets for DNS, time synchronization and the API check. */
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    esp_err_t err = get_ready_ap_interface();
    if (err != ESP_OK) return err;
    if (!ap_interface.ifr_name[0]) return ESP_ERR_INVALID_STATE;
    config.if_name = &ap_interface;
    err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP start failed: %s", esp_err_to_name(err));
        return err;
    }
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get},
        {.uri = "/api/config", .method = HTTP_GET, .handler = config_get},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get},
        {.uri = "/api/scan", .method = HTTP_GET, .handler = scan_get},
        {.uri = "/api/config", .method = HTTP_POST, .handler = submit_post},
        {.uri = "/api/finish", .method = HTTP_POST, .handler = finish_post},
    };
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK) { provision_server_stop(); return err; }
    }
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, redirect);
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(53), .sin_addr.s_addr = inet_addr("192.168.4.1")};
    if (dns_socket < 0 || bind(dns_socket, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        fcntl(dns_socket, F_SETFL, O_NONBLOCK) != 0) {
        provision_server_stop(); return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Provisioning HTTP ready on %s:%u", ap_interface.ifr_name, config.server_port);
    return ESP_OK;
}

void provision_server_stop(void)
{
    if (server) { httpd_stop(server); server = NULL; }
    if (dns_socket >= 0) { close(dns_socket); dns_socket = -1; }
    memset(session_token, 0, sizeof(session_token));
}

void provision_server_poll(void)
{
    if (dns_socket < 0) return;
    /* Answer one uncompressed question, with a bounded packet and no amplification. */
    unsigned char packet[512];
    struct sockaddr_in source;
    socklen_t source_len = sizeof(source);
    int n = recvfrom(dns_socket, packet, sizeof(packet) - 16, 0, (struct sockaddr *)&source, &source_len);
    if (n < 17 || (packet[2] & 0x80) || packet[4] != 0 || packet[5] != 1) return;
    int end = 12;
    while (end < n && packet[end]) {
        unsigned label = packet[end];
        if (label > 63 || end + 1 + (int)label >= n) return;
        end += 1 + label;
    }
    if (end + 5 > n) return;
    end++;
    bool ipv4 = packet[end] == 0 && packet[end + 1] == 1 && packet[end + 2] == 0 && packet[end + 3] == 1;
    end += 4;
    packet[2] = 0x81; packet[3] = 0x80;
    packet[6] = 0; packet[7] = ipv4 ? 1 : 0;
    memset(packet + 8, 0, 4);
    if (ipv4) {
        const unsigned char answer[] = {0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0, 30, 0, 4, 192, 168, 4, 1};
        memcpy(packet + end, answer, sizeof(answer));
        end += sizeof(answer);
    }
    sendto(dns_socket, packet, end, 0, (struct sockaddr *)&source, source_len);
}
