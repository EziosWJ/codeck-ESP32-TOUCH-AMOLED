#pragma once
#include "host_sdk.h"
#include "network_manager.h"
typedef void (*ui_network_action_cb_t)(network_action_t action, void *context);
void ui_home_update_provision(const provision_view_t *view);
void ui_home_set_network_action(ui_network_action_cb_t callback, void *context);
