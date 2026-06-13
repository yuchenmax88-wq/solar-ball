#ifndef REMOTE_DIAG_H
#define REMOTE_DIAG_H

#include <stdint.h>

/*
 * Remote diagnostic interface via MQTT.
 * Listens for diagnostic commands on /solar/ball/{id}/cmd topic
 * and publishes responses on /solar/ball/{id}/diag.
 */

/* Diagnostic snapshot for remote reporting */
typedef struct {
    char     ball_id[16];
    uint32_t uptime_s;
    uint32_t scan_count;
    uint32_t publish_ok;
    uint32_t publish_fail;
    uint16_t last_error_mask;
    uint8_t  last_confidence;
    uint8_t  last_flags;
    uint32_t last_battery_mv;
    int16_t  last_rssi;
    uint8_t  sensor_fault_count;
    uint8_t  tz_hours;
} remote_diag_snapshot_t;

/* Initialize diagnostic counters */
void remote_diag_init(void);

/* Record a successful MQTT publish */
void remote_diag_record_publish(uint8_t success);

/* Record a sensor scan result count */
void remote_diag_record_scan(void);

#endif /* REMOTE_DIAG_H */
