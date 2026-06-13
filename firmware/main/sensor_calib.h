#ifndef SENSOR_CALIB_H
#define SENSOR_CALIB_H

/*
 * Sensor Calibration Data Structures
 *
 * Each of the 80 sensors has two calibration parameters:
 *   1. baseline[i] - the raw ADC reading under uniform reference light
 *      (used to normalize sensor-to-sensor sensitivity differences)
 *   2. channel_map[i] - mapping from hardware channel index to position index.
 *      Default: channel_map[i] = i (physical channel i = position i).
 *      After auto-calibration, this maps each physical sensor to its
 *      actual position on the ball.
 */

#include <stdint.h>
#include "sensor_positions.h"

/* Per-sensor calibration data stored in NVS */
typedef struct {
    uint16_t baseline[SENSOR_COUNT];   /* baseline raw ADC values */
    uint8_t  channel_map[SENSOR_COUNT];/* position index -> physical channel */
} calib_data_t;

/*
 * Calibration states for the auto-calibration state machine.
 *
 * AUTO_CALIB_IDLE:     Normal operation, mapping is loaded
 * AUTO_CALIB_WAITING:  Waiting for user to shine light on position #n
 * AUTO_CALIB_SAMPLING: Taking readings to identify which channel lights up
 * AUTO_CALIB_VERIFY:   Verifying the mapping is correct
 * AUTO_CALIB_DONE:     Mapping complete, save to NVS
 */
typedef enum {
    CALIB_STATE_IDLE = 0,
    CALIB_STATE_WAITING,
    CALIB_STATE_SAMPLING,
    CALIB_STATE_VERIFY,
    CALIB_STATE_DONE
} calib_state_t;

#endif /* SENSOR_CALIB_H */
