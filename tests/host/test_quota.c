#include "quota_data.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void check_used(const char *used, int expected)
{
    char json[512];
    snprintf(json, sizeof(json), "{\"ok\":true,\"rate_limits\":{\"rateLimits\":{\"primary\":{"
             "\"usedPercent\":%s,\"windowDurationMins\":300,\"resetsAt\":1791120915}}}}", used);
    quota_data_t quota = {0};
    assert(quota_parse_json(json, strlen(json), &quota));
    assert(quota.remaining_percent == expected);
    assert(quota.window_minutes == 300);
    assert(quota.resets_at == 1791120915);
}

static void check_invalid(const char *json)
{
    quota_data_t quota = {.remaining_percent = 95, .window_minutes = 300, .resets_at = 1791120915};
    assert(!quota_parse_json(json, strlen(json), &quota));
    assert(quota.remaining_percent == 95 && quota.window_minutes == 300 && quota.resets_at == 1791120915);
}

int main(void)
{
    check_used("5", 95);
    check_used("0", 100);
    check_used("100", 0);
    check_used("-10", 100);
    check_used("150", 0);
    check_used("5.5", 95);
    check_used("5.6", 94);
    check_invalid("not json");
    check_invalid("{\"ok\":false}");
    check_invalid("{\"ok\":true}");
    check_invalid("{\"ok\":true,\"rate_limits\":{\"rateLimits\":{\"primary\":null}}}");
    const char *invalid_primary[] = {
        "\"usedPercent\":\"5\",\"windowDurationMins\":300,\"resetsAt\":1791120915",
        "\"usedPercent\":5,\"windowDurationMins\":0,\"resetsAt\":1791120915",
        "\"usedPercent\":5,\"windowDurationMins\":1.5,\"resetsAt\":1791120915",
        "\"usedPercent\":5,\"windowDurationMins\":300,\"resetsAt\":null",
        "\"usedPercent\":5,\"windowDurationMins\":300,\"resetsAt\":-1",
        "\"usedPercent\":1e999,\"windowDurationMins\":300,\"resetsAt\":1791120915",
        "\"usedPercent\":5,\"windowDurationMins\":300,\"resetsAt\":1e999",
    };
    char json[512];
    for (size_t i = 0; i < sizeof(invalid_primary) / sizeof(invalid_primary[0]); i++) {
        snprintf(json, sizeof(json), "{\"ok\":true,\"rate_limits\":{\"rateLimits\":{\"primary\":{%s}}}}",
                 invalid_primary[i]);
        check_invalid(json);
    }
    const char *sample = "{\"ok\":true,\"rate_limits\":{\"rateLimits\":{\"primary\":{\"usedPercent\":5,"
        "\"windowDurationMins\":300,\"resetsAt\":1791120915},\"secondary\":{\"usedPercent\":56}}},"
        "\"usage\":{\"ignored\":true}}";
    char *buffer = malloc(QUOTA_RESPONSE_MAX_BYTES + 1);
    assert(buffer);
    quota_response_t response = {.data = buffer};
    for (size_t i = 0; i < strlen(sample); i += 7) {
        size_t length = strlen(sample) - i;
        if (length > 7) length = 7;
        assert(quota_response_append(&response, sample + i, length));
    }
    quota_data_t quota;
    assert(quota_parse_json(response.data, response.length, &quota));
    assert(quota.remaining_percent == 95);
    char deep_json[512];
    size_t deep_length = strlen(sample) - 1;
    memcpy(deep_json, sample, deep_length);
    memcpy(deep_json + deep_length, ",\"extra\":", 9);
    deep_length += 9;
    memset(deep_json + deep_length, '[', 40);
    deep_length += 40;
    deep_json[deep_length++] = '0';
    memset(deep_json + deep_length, ']', 40);
    deep_length += 40;
    deep_json[deep_length++] = '}';
    deep_json[deep_length] = '\0';
    check_invalid(deep_json);
    assert(quota_response_append(&response, " \r\n", 3));
    assert(quota_parse_json(response.data, response.length, &quota));
    assert(quota_response_append(&response, "x", 1));
    assert(!quota_parse_json(response.data, response.length, &quota));
    response.length = strlen(sample);
    memset(buffer + response.length, ' ', QUOTA_RESPONSE_MAX_BYTES - response.length);
    response.length = QUOTA_RESPONSE_MAX_BYTES;
    buffer[response.length] = '\0';
    assert(quota_parse_json(buffer, response.length, &quota));
    assert(!quota_response_append(&response, "x", 1));
    assert(response.overflow && response.length == QUOTA_RESPONSE_MAX_BYTES);
    assert(buffer[QUOTA_RESPONSE_MAX_BYTES] == '\0');
    response.length = 0;
    assert(!quota_response_append(&response, "x", 1));
    assert(!quota_parse_json(sample, QUOTA_RESPONSE_MAX_BYTES + 1, &quota));
    memcpy(buffer, sample, strlen(sample));
    buffer[strlen(sample)] = '\0';
    buffer[strlen(sample) + 1] = 'x';
    assert(!quota_parse_json(buffer, strlen(sample) + 2, &quota));
    free(buffer);

    char window[24];
    quota_format_window(300, window, sizeof(window));
    assert(strcmp(window, "5H") == 0);
    quota_format_window(90, window, sizeof(window));
    assert(strcmp(window, "90M") == 0);
    setenv("TZ", "CST-8", 1);
    tzset();
    quota_view_t view = {.has_quota = true, .time_synced = true,
        .now = 1791117315, .quota = {95, 300, 1791120915}};
    char clock[8], reset[32], countdown[80];
    quota_format_times(&view, clock, sizeof(clock), reset, sizeof(reset), countdown, sizeof(countdown));
    assert(strcmp(clock, "20:35") == 0);
    assert(strcmp(reset, "21:35 重置") == 0);
    assert(strcmp(countdown, "还有 1小时0分") == 0);
    view.now = view.quota.resets_at + 10;
    quota_format_times(&view, clock, sizeof(clock), reset, sizeof(reset), countdown, sizeof(countdown));
    assert(strcmp(countdown, "还有 0小时0分") == 0);
    assert(view.quota.remaining_percent == 95);
    view.time_synced = false;
    quota_format_times(&view, clock, sizeof(clock), reset, sizeof(reset), countdown, sizeof(countdown));
    assert(!clock[0] && !reset[0] && !countdown[0]);
    puts("Quota parsing, chunk/size limits, retention and Beijing time tests passed");
    return 0;
}
