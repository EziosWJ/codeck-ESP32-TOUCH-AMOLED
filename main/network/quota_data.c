#include "quota_data.h"

#include "cJSON.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

bool quota_response_append(quota_response_t *response, const char *data, size_t length)
{
    if (!response || !response->data || (!data && length) || response->overflow) return false;
    if (response->length > QUOTA_RESPONSE_MAX_BYTES ||
        length > QUOTA_RESPONSE_MAX_BYTES - response->length) {
        response->overflow = true;
        return false;
    }
    if (length) memcpy(response->data + response->length, data, length);
    response->length += length;
    response->data[response->length] = '\0';
    return true;
}

static bool parse_window(const cJSON *window_data, quota_data_t *out)
{
    const cJSON *used = cJSON_GetObjectItemCaseSensitive(window_data, "usedPercent");
    const cJSON *window = cJSON_GetObjectItemCaseSensitive(window_data, "windowDurationMins");
    const cJSON *reset = cJSON_GetObjectItemCaseSensitive(window_data, "resetsAt");
    if (!cJSON_IsObject(window_data) || !cJSON_IsNumber(used) || !isfinite(used->valuedouble) ||
        !cJSON_IsNumber(window) || !isfinite(window->valuedouble) ||
        window->valuedouble < 1 || window->valuedouble > INT_MAX ||
        floor(window->valuedouble) != window->valuedouble ||
        !cJSON_IsNumber(reset) || !isfinite(reset->valuedouble) ||
        reset->valuedouble <= 0 || reset->valuedouble > 253402300799.0 ||
        floor(reset->valuedouble) != reset->valuedouble) return false;
    double remaining = fmin(100.0, fmax(0.0, 100.0 - used->valuedouble));
    *out = (quota_data_t) {
        .remaining_percent = (int)floor(remaining + 0.5),
        .window_minutes = (int)window->valuedouble,
        .resets_at = (int64_t)reset->valuedouble,
    };
    return true;
}

bool quota_parse_account_json(const char *json, size_t length, quota_account_data_t *out)
{
    if (!json || !out || !length || length > QUOTA_RESPONSE_MAX_BYTES) return false;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(json, length, &end, false);
    if (!root) return false;
    /* Reject embedded NULs and trailing non-JSON data. */
    while (end < json + length && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t')) end++;
    bool valid = end == json + length && cJSON_IsObject(root) &&
                 cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "ok"));
    const cJSON *limits = cJSON_GetObjectItemCaseSensitive(root, "rate_limits");
    const cJSON *rate = cJSON_GetObjectItemCaseSensitive(limits, "rateLimits");
    const cJSON *primary = cJSON_GetObjectItemCaseSensitive(rate, "primary");
    quota_account_data_t next = {0};
    valid = valid && cJSON_IsObject(limits) && cJSON_IsObject(rate) && parse_window(primary, &next.primary);
    if (valid) {
        next.has_secondary = parse_window(cJSON_GetObjectItemCaseSensitive(rate, "secondary"), &next.secondary);
        *out = next;
    }
    cJSON_Delete(root);
    return valid;
}

bool quota_parse_json(const char *json, size_t length, quota_data_t *out)
{
    quota_account_data_t account;
    if (!out || !quota_parse_account_json(json, length, &account)) return false;
    *out = account.primary;
    return true;
}

void quota_format_window(int minutes, char *out, size_t size)
{
    if (minutes % 60 == 0) snprintf(out, size, "%dH", minutes / 60);
    else snprintf(out, size, "%dM", minutes);
}

void quota_format_times(const quota_view_t *view, char *clock, size_t clock_size,
                        char *reset, size_t reset_size, char *countdown, size_t countdown_size)
{
    if (clock_size) clock[0] = '\0';
    if (reset_size) reset[0] = '\0';
    if (countdown_size) countdown[0] = '\0';
    if (!view->time_synced) return;
    time_t now = (time_t)view->now;
    struct tm local;
    if (localtime_r(&now, &local)) strftime(clock, clock_size, "%H:%M", &local);
    if (!view->has_quota) return;
    time_t resets_at = (time_t)view->quota.resets_at;
    if (localtime_r(&resets_at, &local)) {
        char time_text[8];
        strftime(time_text, sizeof(time_text), "%H:%M", &local);
        snprintf(reset, reset_size, "%s 重置", time_text);
    }
    int64_t remaining = view->quota.resets_at > view->now ? view->quota.resets_at - view->now : 0;
    snprintf(countdown, countdown_size, "还有 %lld小时%lld分",
             (long long)(remaining / 3600), (long long)(remaining % 3600 / 60));
}
