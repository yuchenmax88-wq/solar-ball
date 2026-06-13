#ifndef CALIBRATE_H
#define CALIBRATE_H

#include <stdint.h>
#include "sensor_calib.h"

/*
 * Auto-Calibration System
 *
 * Problem:
 *   During assembly, 80 ALS-PT19 sensors are soldered to specific
 *   PCB channels (MUX bank + MUX channel). This gives each sensor a
 *   PHYSICAL ID (0-79). But we don't know which physical position
 *   on the ball it corresponds to.
 *
 * Solution - Sequential Illumination Calibration:
 *   1. The auto-calibrate.py tool connects to the ball via UART
 *   2. It prompts the user: "Shine a bright flashlight at position #0
 *      (the TOP of the ball)"
 *   3. The ball scans all 80 channels and reports which physical channel
 *      has the highest reading
 *   4. The tool records: Position #0 -> Physical Channel X
 *   5. Repeat for all 80 positions
 *   6. The mapping is saved to NVS
 *
 * During normal operation, g_channel_to_position maps each physical
 * sensor channel to its position index in SENSOR_POSITIONS[].
 */

/*
 * Load calibration data from NVS.
 * If no calibration data exists, initializes with default
 * identity mapping (channel i -> position i) and baseline = 0.
 * Must be called once at startup.
 */
void calib_load(void);

/*
 * Save current calibration data to NVS.
 * Returns 0 on success, -1 on failure.
 */
int calib_save(void);

/*
 * Run automatic calibration from PC tool.
 * Listens on UART for commands from auto_calibrate.py:
 *   "CAL:START"           - Begin calibration sequence
 *   "CAL:SCAN"            - Scan all sensors, return highest channel
 *   "CAL:MAP pos chan"    - Set mapping: position pos = physical channel chan
 *   "CAL:SAVE"            - Save calibration to NVS
 *   "CAL:QUIT"            - Exit calibration mode
 *
 * This function runs in a loop until CAL:QUIT is received.
 */
void calib_run_auto(void);

/*
 * Run autonomous sun-based calibration.
 * Uses 4G location + NTP time + solar ephemeris to compute sun direction,
 * then maps all 80 channels via brightness-ranking in a single outdoor scan.
 * No manual flashlight alignment needed.
 *
 * Returns 0 on success, -1 on failure.
 */
int calib_run_sun_auto(void);

/*
 * Get the current calibration state.
 */
calib_state_t calib_get_state(void);

/*
 * Check if calibration data has been loaded.
 * Returns 1 if a non-identity channel-to-position mapping exists.
 */
int calib_has_mapping(void);

/*
 * Check if baseline normalization data exists.
 * Returns 1 if any sensor has a non-zero baseline value.
 */
int calib_has_baseline(void);

/* Global calibration data */
extern calib_data_t g_calib;

/* Channel-to-position mapping (defined in sensor_positions.h) */
extern uint8_t g_channel_to_position[SENSOR_COUNT];

#endif /* CALIBRATE_H */
