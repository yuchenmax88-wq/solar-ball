#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H

/*
 * Solar Ball MQTT Protocol Definition
 * ====================================
 *
 * Topic:  /solar/ball/{ball_id}/direction
 * QOS:    1 (at least once delivery)
 * Format: JSON (UTF-8)
 * Interval: every 5 seconds during normal operation
 *
 * Payload fields:
 *   id    - Ball identifier string (e.g. "ball-001")
 *   ts    - Unix timestamp (seconds since epoch, uint32)
 *   dx    - Direction vector X component (normalized, float)
 *   dy    - Direction vector Y component (normalized, float)
 *   dz    - Direction vector Z component (normalized, float)
 *   soc   - Battery state of charge in percent (0-100, uint8)
 *   rssi  - 4G signal strength in dBm (int8)
 *
 * Notes:
 *   - The direction vector always has unit length (dx^2 + dy^2 + dz^2 == 1.0)
 *   - Vector points TOWARD the brightest direction in the sky
 *   - The central computer uses this vector to compute array orientation
 *   - In full cloud cover / darkness, vector may be noisy; check rssi and soc
 *     to determine if ball is operating normally
 */

#include <stdint.h>
#include <stdio.h>
#include "config.h"

#define MQTT_TOPIC_MAX_LEN      64
#define MQTT_PAYLOAD_MAX_LEN    256

/* JSON payload structure, 1:1 mapping to wire format */
typedef struct __attribute__((packed)) {
    char     id[16];        /* ball identifier, null-terminated */
    uint32_t ts;            /* unix timestamp */
    float    dx;            /* direction unit vector X */
    float    dy;            /* direction unit vector Y */
    float    dz;            /* direction unit vector Z */
    uint8_t  soc;           /* battery state of charge [0-100] */
    int16_t  rssi;          /* 4G signal strength [dBm] */
} direction_packet_t;

/*
 * Build MQTT topic string for this ball.
 * Example: "/solar/ball/ball-001/direction"
 */
static inline void build_mqtt_topic(const char *ball_id, char *out, size_t out_len) {
    snprintf(out, out_len, "%s%s/direction", MQTT_TOPIC_PREFIX, ball_id);
}

/*
 * Serialize direction_packet_t to JSON string.
 * Returns number of bytes written (excluding null terminator).
 */
static inline int serialize_direction(const direction_packet_t *pkt, char *buf, size_t buf_len) {
    return snprintf(buf, buf_len,
        "{\"id\":\"%s\",\"ts\":%lu,\"dx\":%.4f,\"dy\":%.4f,\"dz\":%.4f,\"soc\":%u,\"rssi\":%d}",
        pkt->id, (unsigned long)pkt->ts,
        (double)pkt->dx, (double)pkt->dy, (double)pkt->dz,
        (unsigned)pkt->soc, (int)pkt->rssi);
}

#endif /* MQTT_PROTOCOL_H */
