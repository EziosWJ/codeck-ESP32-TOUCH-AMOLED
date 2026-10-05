#pragma once
#include "host_sdk.h"
#include <stddef.h>
#include <sys/types.h>
#include <net/if.h>
typedef struct { int content_len; const char *body, *token, *origin, *host; unsigned offset; } httpd_req_t;
typedef int httpd_err_code_t;
typedef void *httpd_handle_t;
#define HTTP_GET 0
#define HTTP_POST 1
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
#define HTTPD_404_NOT_FOUND 404
#define HTTPD_RESP_USE_STRLEN -1
typedef struct { const char *uri; int method; esp_err_t (*handler)(httpd_req_t *); } httpd_uri_t;
typedef struct { unsigned stack_size; uint16_t server_port; unsigned max_open_sockets; bool lru_purge_enable; int recv_wait_timeout,send_wait_timeout; struct ifreq *if_name; } httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
esp_err_t httpd_resp_send_err(httpd_req_t *req, int status, const char *message);
esp_err_t httpd_resp_set_type(httpd_req_t *req,const char *type);
esp_err_t httpd_resp_set_hdr(httpd_req_t *req,const char *name,const char *value);
esp_err_t httpd_resp_send(httpd_req_t *req,const char *body,ssize_t size);
esp_err_t httpd_resp_set_status(httpd_req_t *req,const char *status);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *req,const char *name,char *value,size_t size);
size_t httpd_req_get_hdr_value_len(httpd_req_t *req,const char *name);
int httpd_req_recv(httpd_req_t *req,char *buffer,size_t size);
esp_err_t httpd_start(httpd_handle_t *server,const httpd_config_t *config);
esp_err_t httpd_stop(httpd_handle_t server);
esp_err_t httpd_register_uri_handler(httpd_handle_t server,const httpd_uri_t *route);
esp_err_t httpd_register_err_handler(httpd_handle_t server,int code,esp_err_t (*fn)(httpd_req_t *,httpd_err_code_t));
