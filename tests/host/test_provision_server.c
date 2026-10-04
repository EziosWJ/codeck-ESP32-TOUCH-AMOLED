#include "../../main/network/provision_server.c"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
const char test_page[] asm("_binary_provision_html_start") = "test-page";
static device_config_t saved_config,submitted_config;
static provision_view_t state;
static bool has_config=true;
static unsigned submits,finish_calls;
static esp_err_t submit_error,scan_error;
static char response[8192],response_status[64],location[128];
unsigned network_manager_get_config(device_config_t *c,bool *has){*c=saved_config;*has=has_config;return 1;}
void network_manager_get_provision(provision_view_t *v){*v=state;}
bool network_manager_connected(void){return true;}
esp_err_t network_manager_submit(const device_config_t *c){submits++;submitted_config=*c;return submit_error;}
esp_err_t network_manager_action(network_action_t a){assert(a==NETWORK_PROVISION_FINISH);finish_calls++;return state.saved?ESP_OK:ESP_ERR_INVALID_STATE;}
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *c,bool block){assert(c->show_hidden&&block);return scan_error;}
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count,wifi_ap_record_t *records){*count=1;memset(records,0,sizeof(*records));strcpy((char *)records[0].ssid,"<Wi-Fi>&");records[0].rssi=-55;records[0].authmode=WIFI_AUTH_WPA2_PSK;return ESP_OK;}
esp_err_t esp_wifi_clear_ap_list(void){return ESP_OK;}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key){(void)key;static esp_netif_t n;return &n;}
esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *n,char *name){(void)n;strcpy(name,"ap0");return ESP_OK;}
uint32_t esp_random(void){return 0x12345678;}
esp_err_t httpd_resp_send_err(httpd_req_t *r,int code,const char *m){(void)r;snprintf(response_status,sizeof(response_status),"%d",code);snprintf(response,sizeof(response),"%s",m);return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *t){(void)r;(void)t;return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *n,const char *v){(void)r;if(!strcmp(n,"Location"))snprintf(location,sizeof(location),"%s",v);return ESP_OK;}
esp_err_t httpd_resp_send(httpd_req_t *r,const char *body,ssize_t size){(void)r;if(size<0)size=strlen(body);assert((size_t)size<sizeof(response));memcpy(response,body,size);response[size]=0;return ESP_OK;}
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *s){(void)r;snprintf(response_status,sizeof(response_status),"%s",s);return ESP_OK;}
static const char *header(httpd_req_t *r,const char *name){if(!strcmp(name,"X-Codeck-Token"))return r->token;if(!strcmp(name,"Origin"))return r->origin;if(!strcmp(name,"Host"))return r->host;return NULL;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *name,char *value,size_t size){const char *h=header(r,name);if(!h||strlen(h)>=size)return ESP_ERR_INVALID_ARG;strcpy(value,h);return ESP_OK;}
size_t httpd_req_get_hdr_value_len(httpd_req_t *r,const char *name){const char *h=header(r,name);return h?strlen(h):0;}
int httpd_req_recv(httpd_req_t *r,char *buffer,size_t size){size_t n=strlen(r->body)-r->offset;if(n>size)n=size;if(n>7)n=7;memcpy(buffer,r->body+r->offset,n);r->offset+=n;return n;}
esp_err_t httpd_start(httpd_handle_t *h,const httpd_config_t *c){assert(c->if_name&&!strcmp(c->if_name->ifr_name,"ap0"));*h=(void *)1;return ESP_OK;}
esp_err_t httpd_stop(httpd_handle_t h){(void)h;return ESP_OK;}
esp_err_t httpd_register_uri_handler(httpd_handle_t h,const httpd_uri_t *r){(void)h;(void)r;return ESP_OK;}
esp_err_t httpd_register_err_handler(httpd_handle_t h,int code,esp_err_t (*f)(httpd_req_t *,httpd_err_code_t)){(void)h;(void)code;(void)f;return ESP_OK;}
/* Config persistence is separately covered; use no-op NVS to link validation. */
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){(void)name;(void)mode;*h=1;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *k,void *v,size_t *s){(void)h;(void)k;(void)v;(void)s;return ESP_ERR_NVS_NOT_FOUND;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *k,const void *v,size_t s){(void)h;(void)k;(void)v;(void)s;return ESP_OK;}
esp_err_t nvs_erase_key(nvs_handle_t h,const char *k){(void)h;(void)k;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
static httpd_req_t request(const char *body){response[0]=location[0]=0;strcpy(response_status,"200 OK");return (httpd_req_t){.body=body,.content_len=body?(int)strlen(body):0,.token=session_token,.origin="http://192.168.4.1",.host="192.168.4.1"};}
int main(void)
{
 strcpy(session_token,"test-token");device_config_defaults(&saved_config);strcpy(saved_config.ssid,"network");strcpy(saved_config.wifi_password,"wifi-secret");strcpy(saved_config.api_username,"user");strcpy(saved_config.api_password,"api-secret");
 httpd_req_t r=request(NULL);assert(config_get(&r)==ESP_OK);assert(!strstr(response,"wifi-secret")&&!strstr(response,"api-secret"));assert(strstr(response,"has_wifi_password"));
 const char *keep="{\"ssid\":\"new\",\"api_url\":\"https://example.org/api/account\",\"api_username\":\"new-user\",\"keep_wifi_password\":true,\"keep_api_password\":true}";
 r=request(keep);assert(submit_post(&r)==ESP_OK);assert(!strcmp(response_status,"202 Accepted"));assert(submits==1&&!strcmp(submitted_config.wifi_password,"wifi-secret")&&!strcmp(submitted_config.api_password,"api-secret"));
 submit_error=ESP_ERR_INVALID_STATE;r=request(keep);submit_post(&r);assert(!strcmp(response_status,"409 Conflict"));submit_error=ESP_OK;
 r=request(keep);r.token="wrong";submit_post(&r);assert(!strcmp(response_status,"403 Forbidden"));
 r=request(keep);r.origin="http://evil.example";submit_post(&r);assert(!strcmp(response_status,"403 Forbidden"));
 has_config=false;r=request(keep);submit_post(&r);assert(!strcmp(response_status,"400 Bad Request"));has_config=true;
 r=request("{broken");submit_post(&r);assert(!strcmp(response_status,"400 Bad Request"));r=request(keep);r.content_len=4097;submit_post(&r);assert(!strcmp(response_status,"400 Bad Request"));
 const char *open="{\"ssid\":\"open\",\"wifi_password\":\"\",\"api_url\":\"http://host/api/account\",\"api_username\":\"u\",\"api_password\":\"p\"}";
 r=request(open);submit_post(&r);assert(!strcmp(response_status,"202 Accepted")&&!submitted_config.wifi_password[0]);
 r=request(NULL);scan_get(&r);assert(strstr(response,"<Wi-Fi>&"));state.phase=PROVISION_CONNECTING;r=request(NULL);scan_get(&r);assert(!strcmp(response_status,"409 Conflict"));state.phase=PROVISION_WAITING;state.closing=true;r=request(NULL);scan_get(&r);assert(!strcmp(response_status,"409 Conflict"));state.closing=false;
 scan_error=ESP_ERR_INVALID_STATE;r=request(NULL);scan_get(&r);assert(!strcmp(response_status,"503 Service Unavailable"));scan_error=ESP_OK;
 r=request(NULL);finish_post(&r);assert(!strcmp(response_status,"409 Conflict"));state.saved=true;r=request(NULL);finish_post(&r);assert(finish_calls==2&&!strcmp(response_status,"200 OK"));
 r=request(NULL);r.host="connectivitycheck.example";root_get(&r);assert(!strcmp(location,"http://192.168.4.1/"));
 puts("Web API password redaction/retention, validation, busy states, token/origin protection and captive redirect passed");return 0;
}
