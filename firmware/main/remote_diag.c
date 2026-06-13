#include "remote_diag.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static uint32_t s_scan_count = 0;
static uint32_t s_publish_ok = 0;
static uint32_t s_publish_fail = 0;

void remote_diag_init(void) {
    s_scan_count = 0;
    s_publish_ok = 0;
    s_publish_fail = 0;
}

void remote_diag_record_publish(uint8_t success) {
    if (success) {
        s_publish_ok++;
    } else {
        s_publish_fail++;
    }
}

void remote_diag_record_scan(void) {
    s_scan_count++;
}

void remote_diag_build_snapshot(remote_diag_snapshot_t *snap,
                                const char *ball_id,
                                uint32_t battery_mv,
                                int16_t rssi,
                                uint16_t error_mask,
                                uint8_t confidence,
                                uint8_t flags,
                                uint8_t fault_count,
                                uint8_t tz_hours) {
    memset(snap, 0, sizeof(*snap));
    strncpy(snap->ball_id, ball_id, sizeof(snap->ball_id) - 1);
    snap->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    snap->scan_count = s_scan_count;
    snap->publish_ok = s_publish_ok;
    snap->publish_fail = s_publish_fail;
    snap->last_error_mask = error_mask;
    snap->last_confidence = confidence;
    snap->last_flags = flags;
    snap->last_battery_mv = battery_mv;
    snap->last_rssi = rssi;
    snap->sensor_fault_count = fault_count;
    snap->tz_hours = tz_hours;
}

int remote_diag_serialize(const remote_diag_snapshot_t *snap,
                          char *buf, size_t buf_len) {
    return snprintf(buf, buf_len,
        "{\"id\":\"%s\",\"uptime\":%lu,\"scans\":%lu,"
        "\"pub_ok\":%lu,\"pub_fail\":%lu,"
        "\"err\":%u,\"conf\":%u,\"flags\":%u,"
        "\"bat_mv\":%lu,\"rssi\":%d,\"faults\":%u,\"tz\":%d}",
        snap->ball_id,
        (unsigned long)snap->uptime_s,
        (unsigned long)snap->scan_count,
        (unsigned long)snap->publish_ok,
        (unsigned long)snap->publish_fail,
        (unsigned)snap->last_error_mask,
        (unsigned)snap->last_confidence,
        (unsigned)snap->last_flags,
        (unsigned long)snap->last_battery_mv,
        (int)snap->last_rssi,
        (unsigned)snap->sensor_fault_count,
        (int)snap->tz_hours);
}
