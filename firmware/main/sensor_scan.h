#ifndef SENSOR_SCAN_H
#define SENSOR_SCAN_H

#include <stdint.h>
#include "config.h"

/*
 * Initialize GPIO pins for MUX control and I2C buses for ADS1115.
 * Must be called once at startup before any scan.
 */
void sensor_scan_init(void);

/*
 * Scan all 80 sensors and write normalized readings into out[].
 * out[] is indexed by POSITION index (after channel_map lookup).
 * Each value is [0.0, 1.0] where 1.0 = maximum brightness.
 *
 * Returns 0 on success, -1 on I2C error.
 */
int sensor_scan_all(float out[SENSOR_COUNT]);

/*
 * Read a single raw ADC value from a specific MUX bank + channel.
 * Used primarily during auto-calibration.
 *
 * bank: 0-4 (which MUX chip)
 * channel: 0-15 (which input on that MUX)
 * Returns raw ADC value (0-65535) or 0 on error.
 */
uint16_t sensor_read_raw(int bank, int channel);

#endif /* SENSOR_SCAN_H */
