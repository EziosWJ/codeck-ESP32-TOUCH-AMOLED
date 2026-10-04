#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool configured;
    bool connected; /* Associated with an AP and has an IPv4 address. */
    char ssid[33];
    char ip[16];
    int8_t rssi;
} network_view_t;
