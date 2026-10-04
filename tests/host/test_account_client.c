/* Run real HTTP response handling against deterministic SDK substitutes. */
#include "../../main/network/account_client.c"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
static esp_http_client_config_t http_config;
static const char *body;
static int http_status=200,http_calls,cleanup_calls;
static esp_err_t transport_error,sntp_error;
static bool oversize,disconnect_during_fetch,connected_test=true;
static unsigned generation_test;
static TickType_t ticks;
static unsigned creates,deletes,fail_create,sntp_deinits;
static device_config_t runtime;
static provision_view_t provision_test;
static unsigned task_mode,task_steps;
static jmp_buf task_exit;

bool network_manager_connected(void){return connected_test;}
unsigned network_manager_generation(void){return generation_test;}
unsigned network_manager_get_config(device_config_t *c,bool *has){*c=runtime;*has=true;return generation_test;}
void network_manager_get_provision(provision_view_t *view){*view=provision_test;}
network_view_t network_manager_get_network(void){return (network_view_t){.connected=connected_test,.configured=true};}
esp_err_t network_manager_action(network_action_t action){(void)action;return ESP_OK;}
void network_manager_api_result(unsigned g,bool ok,const char *m){(void)g;(void)ok;(void)m;provision_test.phase=PROVISION_SAVED;}
TickType_t xTaskGetTickCount(void){return ticks;}
void vTaskDelay(TickType_t delay){
 ticks+=delay;
 if(task_mode){task_steps++;
  if(task_mode==1){if(task_steps==1){connected_test=false;generation_test++;}else if(task_steps==2)connected_test=true;else if(task_steps==3)longjmp(task_exit,1);}
  if(task_mode==2&&task_steps==61)longjmp(task_exit,1);
  if(task_mode==3&&task_steps==2)longjmp(task_exit,1);
 }
}
int xTaskCreate(void (*task)(void *),const char *name,unsigned stack,void *arg,int priority,TaskHandle_t *handle)
{(void)task;(void)name;(void)stack;(void)arg;(void)priority;if(++creates==fail_create)return 0;if(handle)*handle=(void *)1;return pdPASS;}
void vTaskDelete(TaskHandle_t handle){(void)handle;deletes++;}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *c){assert(!c->start);return sntp_error;}
esp_err_t esp_netif_sntp_start(void){return ESP_OK;}
void esp_netif_sntp_deinit(void){sntp_deinits++;}
bool lvgl_port_lock(int timeout){(void)timeout;return true;}
void lvgl_port_unlock(void){}
void ui_home_update(const quota_view_t *v){(void)v;}
void ui_home_update_network(const network_view_t *v){(void)v;}
void ui_home_update_provision(const provision_view_t *v){(void)v;}
void ui_home_set_network_action(ui_network_action_cb_t cb,void *context){(void)cb;(void)context;}
const char *esp_err_to_name(esp_err_t e){(void)e;return "mock";}
void host_log(const char *tag,const char *format,...){(void)tag;(void)format;}
esp_err_t esp_crt_bundle_attach(void *c){(void)c;return ESP_OK;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c)
{
 assert(c->auth_type==HTTP_AUTH_TYPE_BASIC&&c->method==HTTP_METHOD_GET);
 assert(c->timeout_ms==10000&&c->disable_auto_redirect&&c->max_authorization_retries==-1);
 assert(!strcmp(c->user_agent,"Codeck/1.0"));
 assert(!strcmp(c->username,"test-user")&&!strcmp(c->password,"test-secret"));
 assert(c->crt_bundle_attach==esp_crt_bundle_attach);http_config=*c;return &http_config;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *key,const char *value)
{(void)c;assert(!strcmp(key,"Accept")&&!strcmp(value,"application/json"));return ESP_OK;}
esp_err_t esp_http_client_perform(esp_http_client_handle_t c)
{
 http_calls++;esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_DATA,.user_data=c->user_data};
 if(oversize){char chunk[1024];memset(chunk,'x',sizeof(chunk));e.data=chunk;e.data_len=sizeof(chunk);for(unsigned i=0;i<65;i++)c->event_handler(&e);}
 else for(size_t offset=0;body&&offset<strlen(body);offset+=3){e.data=(char *)body+offset;e.data_len=strlen(body)-offset>3?3:(int)(strlen(body)-offset);assert(c->event_handler(&e)==ESP_OK);}
 if(disconnect_during_fetch){connected_test=false;generation_test++;}return transport_error;
}
int esp_http_client_get_status_code(esp_http_client_handle_t c){(void)c;return http_status;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c){(void)c;cleanup_calls++;return ESP_OK;}
static void retained(void){assert(!latest_view.online&&latest_view.has_quota&&latest_view.quota.remaining_percent==95);}
int main(void)
{
 runtime=(device_config_t){.version=1,.ssid="runtime-wifi",.api_url="http://host/api/account",.api_username="test-user",.api_password="test-secret"};
 char message[128];
 const char *sample="{\"ok\":true,\"rate_limits\":{\"rateLimits\":{\"primary\":{\"usedPercent\":5,\"windowDurationMins\":300,\"resetsAt\":1791120915}}}}";
 body=sample;assert(account_client_check(&runtime,generation_test,message,sizeof(message)));assert(latest_view.online&&latest_view.quota.remaining_percent==95);
 const char *weekly="{\"ok\":true,\"rate_limits\":{\"rateLimits\":{\"primary\":{\"usedPercent\":5,\"windowDurationMins\":300,\"resetsAt\":1791120915},\"secondary\":{\"usedPercent\":38,\"windowDurationMins\":10080,\"resetsAt\":1791380115}}}}";
 body=weekly;assert(account_client_check(&runtime,generation_test,message,sizeof(message)));
 assert(latest_view.has_weekly_quota&&latest_view.weekly_quota.remaining_percent==62);
 http_status=401;assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));
 assert(latest_view.has_weekly_quota&&latest_view.weekly_quota.remaining_percent==62);
 http_status=200;body=sample;assert(account_client_check(&runtime,generation_test,message,sizeof(message)));
 assert(!latest_view.has_weekly_quota);
 http_status=401;assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));assert(strstr(message,"401"));retained();assert(network_manager_connected());
 http_status=302;assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));retained();http_status=200;
 transport_error=ESP_ERR_TIMEOUT;assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));retained();transport_error=ESP_OK;
 body="{broken";assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));retained();body="{\"ok\":true}";assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));retained();
 oversize=true;assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));retained();oversize=false;body=sample;
 disconnect_during_fetch=true;assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));assert(!network_manager_connected());assert(latest_view.has_quota);
 disconnect_during_fetch=false;connected_test=true;assert(account_client_check(&runtime,generation_test,message,sizeof(message)));
 strcpy(runtime.api_url,"https://host/api/account");transport_error=ESP_ERR_INVALID_ARG;assert(!account_client_check(&runtime,generation_test,message,sizeof(message)));retained();transport_error=ESP_OK;
 assert(account_client_check(&runtime,generation_test,message,sizeof(message)));time_sync(NULL);assert(synced);assert(http_calls==cleanup_calls);
 fail_create=1;assert(account_client_start()==ESP_ERR_NO_MEM);assert(deletes==0&&sntp_deinits==1&&!started);fail_create=0;
 assert(account_client_start()==ESP_OK);assert(account_client_start()==ESP_ERR_INVALID_STATE);
 task_steps=ticks=0;task_mode=1;int before=http_calls;if(!setjmp(task_exit))request_task(NULL);assert(http_calls-before==2);
 task_steps=ticks=0;task_mode=2;before=http_calls;if(!setjmp(task_exit))request_task(NULL);assert(http_calls-before==2);
 provision_test=(provision_view_t){.active=true,.phase=PROVISION_CHECKING_API};task_steps=ticks=0;task_mode=3;before=http_calls;if(!setjmp(task_exit))request_task(NULL);assert(http_calls-before==1&&provision_test.phase==PROVISION_SAVED);task_mode=0;
 started=false;fail_create=creates+2;unsigned previous_deinits=sntp_deinits;assert(account_client_start()==ESP_ERR_NO_MEM);assert(started&&deletes==0&&sntp_deinits==previous_deinits);assert(account_client_start()==ESP_ERR_INVALID_STATE);
 started=false;fail_create=0;sntp_error=ESP_ERR_NO_MEM;assert(account_client_start()==ESP_OK&&started);sntp_error=ESP_OK;
 puts("Runtime HTTP credentials, HTTPS verification, failures, retention and startup cleanup passed");return 0;
}
