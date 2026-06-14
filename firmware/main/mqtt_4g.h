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
int16_t modem_get_rssi(void);

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

/*
 * Subscribe to MQTT command topic to receive OTA triggers.
 * Returns 0 on success, -1 on failure.
 */
int mqtt_subscribe_cmd(const char *ball_id);

/*
 * Check for pending MQTT messages (non-blocking poll).
 * If a message is received, it is stored in payload/payload_len.
 * Returns 1 if message received, 0 if no message, -1 on error.
 */
int mqtt_poll_message(char *topic, size_t topic_len,
                      char *payload, size_t payload_len);

/*
 * Download a file from an HTTP URL via the SIM7600G modem.
 * url: full URL to download from
 * buf: buffer to store downloaded data
 * buf_size: size of the buffer
 * bytes_read: output, actual bytes downloaded
 * Returns 0 on success, -1 on failure.
 */
int modem_http_download(const char *url, uint8_t *buf, size_t buf_size,
                        size_t *bytes_read);

/*
 * Download a file from HTTP in chunks, writing each chunk via callback.
 * Callback receives (data, len, offset) and returns 0 on success.
 * Returns 0 on success, -1 on failure.
 */
typedef int (*ota_chunk_callback_t)(const uint8_t *data, size_t len, uint32_t offset);
int modem_http_download_chunked(const char *url, ota_chunk_callback_t callback);

/*
 * Get HTTP file size via HEAD request.
 * Returns file size on success, -1 on failure.
 */
int modem_http_head_size(const char *url);

#endif /* MQTT_4G_H */
