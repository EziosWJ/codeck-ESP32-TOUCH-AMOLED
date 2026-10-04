#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define QUOTA_RESPONSE_MAX_BYTES (64U * 1024U)

typedef struct {
    int remaining_percent;
    int window_minutes;
    int64_t resets_at;
} quota_data_t;

typedef struct {
    bool has_quota;
    bool online;
    bool time_synced;
    int64_t now;
    quota_data_t quota;
    bool has_weekly_quota;
    quota_data_t weekly_quota;
} quota_view_t;

typedef struct {
    quota_data_t primary;
    bool has_secondary;
    quota_data_t secondary;
} quota_account_data_t;

typedef struct {
    char *data;
    size_t length;
    bool overflow;
} quota_response_t;

/* Buffer must have QUOTA_RESPONSE_MAX_BYTES + 1 bytes. */
bool quota_response_append(quota_response_t *response, const char *data, size_t length);
/* On failure, the caller's last valid data is unchanged. */
bool quota_parse_json(const char *json, size_t length, quota_data_t *out);
/* Invalid or absent optional secondary data does not discard valid primary data. */
bool quota_parse_account_json(const char *json, size_t length, quota_account_data_t *out);
void quota_format_window(int minutes, char *out, size_t size);
void quota_format_times(const quota_view_t *view, char *clock, size_t clock_size,
                        char *reset, size_t reset_size, char *countdown, size_t countdown_size);
