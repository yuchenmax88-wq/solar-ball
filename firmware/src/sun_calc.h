#ifndef SUN_CALC_H
#define SUN_CALC_H

#include <stdint.h>
#include "sensor_positions.h"

/*
 * Solar position calculator using simplified NOAA algorithm.
 * Accuracy: ~0.5° for years 2000-2100.
 *
 * Input:
 *   lat, lon  - observer latitude/longitude in degrees
 *   alt_m     - observer altitude in meters (approximate, optional)
 *   unix_ts   - Unix timestamp (seconds since 1970-01-01 UTC)
 *
 * Output:
 *   Returns a unit vector in the ball's coordinate system:
 *     x = east, y = north, z = up (zenith)
 *
 * Usage for auto-calibration:
 *   1. Get lat/lon from 4G cell tower location
 *   2. Get unix_ts from NTP
 *   3. Compute sun direction
 *   4. One scan of all 80 channels → match readings to positions
 */

vec3_t sun_calc_direction(float lat, float lon, float alt_m, uint32_t unix_ts);

/*
 * Simplified: lat/lon from int (degrees * 1e6, as GPS modules often provide).
 */
vec3_t sun_calc_from_int(int32_t lat_e6, int32_t lon_e6, int32_t alt_m, uint32_t unix_ts);

#endif /* SUN_CALC_H */
