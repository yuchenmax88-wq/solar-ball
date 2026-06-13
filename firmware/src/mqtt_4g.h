#ifndef MQTT_4G_H
#define MQTT_4G_H

#include <stdint.h>
#include "mqtt_protocol.h"

/*
 * Initialize the SIM7600G 4G module.
 * Powers on module, waits for network registration,
 * and establishes MQTT connection to broker.
 *
 * Returns 0 on success, -1 on failure.
 */
int modem_init(void);

/*
 * Publish a direction packet via MQTT.
 * Packet is serialized to JSON and sent to the configured topic.
 *
 * Returns number of bytes published on success, -1 on failure.
 */
int mqtt_publish_direction(const direction_packet_t *pkt);

/*
 * Get current RSSI (signal strength) in dBm.
 * Returns -140 to -44 (dBm), or -999 if unknown.
 */
int8_t modem_get_rssi(void);

/*
 * Synchronize system time via NTP through the 4G connection.
 * Returns 0 on success, -1 on failure.
 */
int modem_sync_time(void);

/*
 * Get cell tower location from SIM7600G (network-based positioning).
 * lat_e6, lon_e6: latitude/longitude in degrees * 1e6.
 * Accuracy: typically 100m-1km. Returns 0 on success, -1 on failure.
 */
int modem_get_location(int32_t *lat_e6, int32_t *lon_e6);

/*
 * Get Unix timestamp from modem clock (synced via NTP).
 * Returns 0 on success, -1 on failure.
 */
int modem_get_unix_time(uint32_t *unix_ts);

/*
 * Power off the 4G module to save energy before deep sleep.
 */
void modem_power_off(void);

#endif /* MQTT_4G_H */
