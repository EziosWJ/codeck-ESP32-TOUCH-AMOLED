#pragma once

#include <stddef.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

/* LVGL owns this pool for the lifetime of the application. */
static inline void *codeck_lvgl_pool_alloc(size_t size)
{
    void *pool = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pool == NULL) {
        ESP_LOGE("lvgl_memory", "Unable to allocate %u-byte LVGL pool in PSRAM",
                 (unsigned)size);
        abort();
    }
    ESP_LOGI("lvgl_memory", "LVGL pool: %u bytes in PSRAM", (unsigned)size);
    return pool;
}
