#pragma once
#include "esp_err.h"
esp_err_t provision_server_start(void);
void provision_server_stop(void);
/* Called by the network worker; nonblocking DNS processing. */
void provision_server_poll(void);
