#ifndef SENSOR_DIAG_H
#define SENSOR_DIAG_H

#include <stdint.h>
#include "mqtt_protocol.h"

/*
 * Sensor diagnostic and weather classification.
 * Detects:
 *   - Open/short sensor faults
 *   - Sensor saturation (too bright)
 *   - Overcast sky (uniform illumination → low confidence)
 *   - Nighttime (all sensors near zero)
 */

/* Raw sensor scan result used by diagnostics */
typedef struct {
    uint16_t raw_values[80];     /* raw ADC per physical channel */
    uint16_t max_raw;            /* highest raw (darkest sensor) */
    uint16_t min_raw;            /* lowest raw (brightest sensor) */
    float    variance;           /* variance of raw readings */
    int      adc_errors;         /* count of I2C failures */
} sensor_diag_result_t;

/*
 * Analyze a scan for faults and weather conditions.
 * Returns error_mask + sets confidence + flags.
 * Call after sensor_scan_all().
 */
void sensor_diag_analyze(const sensor_diag_result_t *diag,
                         uint16_t *error_mask,
                         uint8_t *confidence,
                         uint8_t *flags);

/*
 * Check if a single sensor appears faulty.
 * Returns FAULT_SENSOR_OPEN, FAULT_SENSOR_SHORT, or 0.
 */
uint16_t sensor_diag_check_channel(uint16_t raw_value, uint16_t min_raw, uint16_t max_raw);

#endif /* SENSOR_DIAG_H */
