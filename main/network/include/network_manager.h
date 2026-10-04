#pragma once
#include "device_config.h"
#include "network_status.h"

typedef enum {
    PROVISION_IDLE, PROVISION_WAITING, PROVISION_CONNECTING,
    PROVISION_CHECKING_API, PROVISION_SAVED, PROVISION_FAILED, PROVISION_STOPPING
} provision_phase_t;
typedef struct {
    bool active, configured, saved, api_ok, closing;
    provision_phase_t phase;
    char ap_ssid[33], ap_password[17];
    char message[128];
} provision_view_t;
typedef enum { NETWORK_PROVISION_START, NETWORK_PROVISION_CANCEL, NETWORK_PROVISION_FINISH } network_action_t;

esp_err_t network_manager_start(void);
/* Nonblocking commands, safe from the UI and HTTP server. */
esp_err_t network_manager_action(network_action_t action);
esp_err_t network_manager_submit(const device_config_t *config);
unsigned network_manager_get_config(device_config_t *config, bool *configured);
void network_manager_get_provision(provision_view_t *view);
network_view_t network_manager_get_network(void);
bool network_manager_connected(void);
/* Changes on disconnect/configuration replacement; protects in-flight API results. */
unsigned network_manager_generation(void);

/* Complete API validation only if the configuration/session is still current. */
void network_manager_api_result(unsigned generation, bool ok, const char *message);
