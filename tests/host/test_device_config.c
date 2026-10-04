#include "../../main/network/device_config.c"
#include <assert.h>
#include <stdio.h>
static device_config_t disk, pending;
static bool exists, pending_exists;
static size_t disk_size;
static esp_err_t commit_error;
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *h)
{ assert(!strcmp(name,"codeck")); if (!exists && mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND; *h=1; pending=disk; pending_exists=exists; return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *value, size_t *size)
{ (void)h; assert(!strcmp(key,"config")); if(!exists)return ESP_ERR_NVS_NOT_FOUND; memcpy(value,&disk,*size<disk_size?*size:disk_size);*size=disk_size;return ESP_OK; }
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *value,size_t size)
{ (void)h;assert(!strcmp(key,"config")&&size==sizeof(pending));memcpy(&pending,value,size);pending_exists=true;return ESP_OK; }
esp_err_t nvs_erase_key(nvs_handle_t h,const char *key){(void)h;(void)key;pending_exists=false;return exists?ESP_OK:ESP_ERR_NVS_NOT_FOUND;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;if(commit_error)return commit_error;disk=pending;exists=pending_exists;disk_size=sizeof(disk);return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
int main(void)
{
 device_config_t c, read;device_config_defaults(&c);
 assert(device_config_load(&read)==ESP_ERR_NVS_NOT_FOUND);
 assert(!device_config_valid(&c));
 strcpy(c.ssid,"new-network");strcpy(c.wifi_password,"password123");strcpy(c.api_username,"user");strcpy(c.api_password,"secret");
 assert(device_config_valid(&c));assert(device_config_save(&c)==ESP_OK);assert(device_config_load(&read)==ESP_OK);assert(!memcmp(&c,&read,sizeof(c)));
 strcpy(c.ssid,"replacement");commit_error=ESP_ERR_NO_MEM;assert(device_config_save(&c)==ESP_ERR_NO_MEM);assert(device_config_load(&read)==ESP_OK);assert(!strcmp(read.ssid,"new-network"));
 commit_error=ESP_OK;assert(device_config_save(&c)==ESP_OK);assert(device_config_load(&read)==ESP_OK);assert(!strcmp(read.ssid,"replacement"));
 c.wifi_password[0]=0;assert(device_config_valid(&c));strcpy(c.wifi_password,"short");assert(!device_config_valid(&c));memset(c.wifi_password,'a',64);c.wifi_password[64]=0;assert(device_config_valid(&c));c.wifi_password[0]='z';assert(!device_config_valid(&c));c.wifi_password[0]=0;
 memset(c.ssid,'s',sizeof(c.ssid));assert(!device_config_valid(&c));strcpy(c.ssid,"ssid");
 strcpy(c.api_url,"https://example.org/api/account");assert(device_config_valid(&c));strcpy(c.api_url,"https://");assert(!device_config_valid(&c));strcpy(c.api_url,"https://user@host/");assert(!device_config_valid(&c));strcpy(c.api_url,"http://host/ bad");assert(!device_config_valid(&c));
 disk.version++;assert(device_config_load(&read)==ESP_ERR_INVALID_ARG);disk.version--;disk_size--;assert(device_config_load(&read)==ESP_ERR_INVALID_ARG);
 assert(device_config_clear()==ESP_OK);assert(device_config_load(&read)==ESP_ERR_NVS_NOT_FOUND);
 puts("Runtime config validation, atomic commit failure, versioning and reboot loading passed");return 0;
}
