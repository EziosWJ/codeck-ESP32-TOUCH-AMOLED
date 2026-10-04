/* Exercise the real state machine and storage, with a fake radio and clock. */
#define main config_test_entry
#include "test_device_config.c"
#undef main
#include "../../main/network/network_manager.c"
#include <assert.h>
#include <stdio.h>
esp_event_base_t WIFI_EVENT="wifi",IP_EVENT="ip";
static TickType_t ticks;
static wifi_config_t radio;
static esp_err_t radio_error,server_error;
static unsigned connects,server_stops;
static bool radio_started, ap_secured, complete_on_connect;
static void got_ip(void);
static int radio_mode;
static command_t queued;
static bool has_command;
static esp_netif_t sta_netif,ap_netif;
QueueHandle_t xQueueCreate(unsigned count,unsigned size){assert(count==1&&size==sizeof(command_t));return (void *)1;}
int xQueueSend(QueueHandle_t q,const void *item,TickType_t wait){(void)q;(void)wait;if(has_command)return 0;queued=*(const command_t *)item;has_command=true;return pdTRUE;}
int xQueueReceive(QueueHandle_t q,void *item,TickType_t wait){(void)q;(void)wait;if(!has_command)return 0;*(command_t *)item=queued;has_command=false;return pdTRUE;}
void vQueueDelete(QueueHandle_t q){(void)q;}
TickType_t xTaskGetTickCount(void){return ticks;}
int xTaskCreate(void (*task)(void *),const char *name,unsigned stack,void *arg,int priority,TaskHandle_t *h)
{(void)task;(void)name;(void)stack;(void)arg;(void)priority;(void)h;return pdPASS;}
esp_err_t esp_event_loop_create_default(void){return ESP_OK;}
esp_err_t esp_event_handler_register(esp_event_base_t b,int id,esp_event_handler_t f,void *a){(void)b;(void)id;(void)f;(void)a;return ESP_OK;}
esp_err_t esp_event_handler_unregister(esp_event_base_t b,int id,esp_event_handler_t f){(void)b;(void)id;(void)f;return ESP_OK;}
esp_err_t esp_netif_init(void){return ESP_OK;}
esp_netif_t *esp_netif_create_default_wifi_sta(void){return &sta_netif;}
esp_netif_t *esp_netif_create_default_wifi_ap(void){return &ap_netif;}
void esp_netif_destroy_default_wifi(esp_netif_t *n){(void)n;}
esp_err_t esp_netif_dhcps_stop(esp_netif_t *n){(void)n;return ESP_OK;}
esp_err_t esp_netif_dhcps_start(esp_netif_t *n){(void)n;return ESP_OK;}
esp_err_t esp_netif_set_ip_info(esp_netif_t *n,const esp_netif_ip_info_t *ip){(void)n;assert(ip->ip.addr==0x0104a8c0);return ESP_OK;}
esp_err_t esp_netif_set_dns_info(esp_netif_t *n,int t,const esp_netif_dns_info_t *dns){(void)n;(void)t;assert(dns->ip.u_addr.ip4.addr==0x0104a8c0);return ESP_OK;}
esp_err_t esp_netif_dhcps_option(esp_netif_t *n,int op,int id,void *v,unsigned size){(void)n;(void)op;(void)id;assert(size==1&&*(uint8_t *)v==2);return ESP_OK;}
esp_err_t esp_netif_get_ip_info(esp_netif_t *n,esp_netif_ip_info_t *ip){(void)n;IP4_ADDR(&ip->ip,192,168,1,42);return ESP_OK;}
esp_err_t esp_netif_sntp_start(void){return ESP_OK;}
esp_err_t esp_wifi_init(const wifi_init_config_t *c){(void)c;return ESP_OK;}
esp_err_t esp_wifi_set_storage(int s){assert(s==WIFI_STORAGE_RAM);return ESP_OK;}
esp_err_t esp_wifi_set_mode(int mode){assert(mode==WIFI_MODE_STA||mode==WIFI_MODE_APSTA);radio_mode=mode;return ESP_OK;}
esp_err_t esp_wifi_set_config(int iface,wifi_config_t *c){if(iface==WIFI_IF_STA)radio=*c;else {assert(!radio_started&&c->ap.authmode==WIFI_AUTH_WPA2_PSK&&strlen((char *)c->ap.password)>=8);ap_secured=true;}return radio_error;}
esp_err_t esp_wifi_start(void){if(radio_mode==WIFI_MODE_APSTA)assert(ap_secured);radio_started=true;return ESP_OK;}
esp_err_t esp_wifi_stop(void){radio_started=false;connected=associated=false;return ESP_OK;}
esp_err_t esp_wifi_deinit(void){return ESP_OK;}
esp_err_t esp_wifi_disconnect(void){connected=false;return ESP_OK;}
esp_err_t esp_wifi_connect(void){connects++;if(complete_on_connect)got_ip();return radio_error;}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap){memset(ap,0,sizeof(*ap));memcpy(ap->ssid,radio.sta.ssid,32);ap->rssi=-57;return ESP_OK;}
esp_err_t esp_read_mac(uint8_t *mac,int type){(void)type;memset(mac,0x12,6);return ESP_OK;}
uint32_t esp_random(void){return 0x12345678;}
esp_err_t nvs_flash_init(void){return ESP_OK;}
esp_err_t provision_server_start(void){return server_error;}
void provision_server_stop(void){server_stops++;}
void provision_server_poll(void){}
static void drain(void){command_t cmd;assert(xQueueReceive(commands,&cmd,0));process_command(&cmd);}
static device_config_t make_config(const char *ssid){device_config_t c;device_config_defaults(&c);snprintf(c.ssid,sizeof(c.ssid),"%s",ssid);strcpy(c.wifi_password,"password123");strcpy(c.api_username,"user");strcpy(c.api_password,"secret");return c;}
static void got_ip(void){
 wifi_event_sta_connected_t event={0};memcpy(event.ssid,radio.sta.ssid,32);event.ssid_len=strnlen((char *)radio.sta.ssid,32);
 wifi_event(NULL,WIFI_EVENT,WIFI_EVENT_STA_CONNECTED,&event);
 wifi_event(NULL,IP_EVENT,IP_EVENT_STA_GOT_IP,NULL);
}
int main(void)
{
 assert(network_manager_start()==ESP_OK);assert(!configured);drain();assert(provision.active&&provision.phase==PROVISION_WAITING);
 device_config_t first=make_config("first"),second=make_config("second");
 assert(network_manager_submit(&first)==ESP_OK);assert(network_manager_submit(&second)==ESP_ERR_INVALID_STATE);drain();assert(trial&&!exists);
 wifi_event(NULL,IP_EVENT,IP_EVENT_STA_GOT_IP,NULL);network_tick();assert(trial&&!exists&&!connected);
 ticks=29999;network_tick();assert(!exists&&trial);ticks=30000;network_tick();assert(!trial&&provision.phase==PROVISION_FAILED&&!exists);
 assert(network_manager_submit(&first)==ESP_OK);drain();got_ip();network_tick();network_manager_api_result(generation,false,"API 401");assert(exists&&configured&&provision.saved&&!provision.api_ok);assert(!strcmp(disk.ssid,"first"));
 assert(network_manager_action(NETWORK_PROVISION_FINISH)==ESP_OK);
 provision_view_t closing_view;network_manager_get_provision(&closing_view);assert(closing_view.closing&&closing_view.phase==PROVISION_STOPPING);
 command_t closing;assert(xQueueReceive(commands,&closing,0));
 assert(network_manager_submit(&second)==ESP_ERR_INVALID_STATE);assert(network_manager_action(NETWORK_PROVISION_CANCEL)==ESP_ERR_INVALID_STATE);
 process_command(&closing);assert(!provision.active&&configured&&network_manager_connected());
 network_manager_api_result(generation-1,true,"stale result");assert(provision.phase==PROVISION_IDLE);
 assert(network_manager_action(NETWORK_PROVISION_START)==ESP_OK);drain();
 unsigned waiting_connects=connects;ticks+=5000;network_tick();assert(connects==waiting_connects);
 assert(network_manager_submit(&second)==ESP_OK);drain();
 commit_error=ESP_ERR_NO_MEM;got_ip();network_tick();assert(provision.phase==PROVISION_FAILED&&!strcmp(disk.ssid,"first")&&!strcmp(current.ssid,"first"));commit_error=ESP_OK;
 assert(network_manager_submit(&second)==ESP_OK);drain();got_ip();network_tick();network_manager_api_result(generation,true,"API OK");assert(provision.saved&&provision.api_ok&&!strcmp(disk.ssid,"second"));
 unsigned checked=generation;
 assert(network_manager_submit(&second)==ESP_OK);drain();
 wifi_event(NULL,IP_EVENT,IP_EVENT_STA_GOT_IP,NULL);network_tick();assert(trial&&!connected);
 got_ip();network_tick();assert(provision.phase==PROVISION_CHECKING_API);
 ticks+=30000;network_tick();assert(provision.phase==PROVISION_SAVED&&provision.saved);
 network_manager_api_result(generation,true,"late API result");assert(!provision.api_ok);
 assert(network_manager_submit(&first)==ESP_OK);drain();got_ip();
 commit_error=ESP_ERR_NO_MEM;assert(network_manager_action(NETWORK_PROVISION_CANCEL)==ESP_OK);drain();
 assert(!trial&&!provision.closing&&provision.phase==PROVISION_FAILED&&!strcmp(current.ssid,"second"));commit_error=ESP_OK;
 complete_on_connect=true;assert(network_manager_action(NETWORK_PROVISION_CANCEL)==ESP_OK);drain();assert(connected);complete_on_connect=false;assert(!strcmp(current.ssid,"first")&&!strcmp(disk.ssid,"first")&&!provision.active);
 network_manager_api_result(checked,true,"stale result");assert(provision.phase==PROVISION_IDLE);
 got_ip();network_view_t view=network_manager_get_network();assert(view.connected&&view.configured&&!strcmp(view.ip,"192.168.1.42"));
 unsigned old_generation=network_manager_generation();wifi_event(NULL,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,NULL);assert(!network_manager_connected()&&network_manager_generation()!=old_generation);
 unsigned count=connects;ticks+=4999;last_connect=ticks;network_tick();assert(connects==count);ticks+=5000;network_tick();assert(connects==count+1);
 assert(network_manager_action(NETWORK_PROVISION_START)==ESP_OK);drain();assert(network_manager_submit(&second)==ESP_OK);drain();assert(network_manager_action(NETWORK_PROVISION_FINISH)==ESP_ERR_INVALID_STATE);assert(network_manager_action(NETWORK_PROVISION_CANCEL)==ESP_OK);drain();assert(!trial&&!strcmp(current.ssid,"first"));
 /* Cancel a first-boot session even after its config was persisted. */
 configured=false;device_config_defaults(&current);device_config_clear();assert(network_manager_action(NETWORK_PROVISION_START)==ESP_OK);drain();assert(network_manager_submit(&first)==ESP_OK);drain();got_ip();network_tick();assert(exists);assert(network_manager_action(NETWORK_PROVISION_CANCEL)==ESP_OK);drain();assert(!exists&&!configured);
 /* Persisted configuration is loaded, never taken from compile-time secrets. */
 assert(device_config_save(&first)==ESP_OK);started=false;commands=NULL;assert(network_manager_start()==ESP_OK);assert(configured&&!strcmp(current.ssid,"first")&&!has_command);assert(network_manager_start()==ESP_ERR_INVALID_STATE);
 puts("First boot, timeout, duplicate submit, save failure, API failure, cancel rollback and reconnect passed");return 0;
}
