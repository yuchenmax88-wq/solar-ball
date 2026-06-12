#ifndef DIRECTION_H
#define DIRECTION_H

#include <stdint.h>
#include "sensor_positions.h"

/*
 * Compute the weighted centroid direction vector from
 * normalized sensor readings.
 *
 * Input:  norm[SENSOR_COUNT] - normalized sensor values [0.0, 1.0]
 * Output: dir - unit vector pointing toward brightest direction
 *
 * Algorithm:
 *   For each sensor i with z >= -0.866, multiply its position unit vector
 *   by its normalized reading, sum all, then normalize the result.
 *
 *   Sensors below z = -0.866 (bottom 30 degrees) are excluded.
 *   At least 3 sensors must have non-zero readings for a valid result.
 *
 * Returns: 0 on success
 *         -1 if no valid readings (all sensors near zero)
 */
int direction_compute(const float norm[SENSOR_COUNT], vec3_t *out_dir);

#endif /* DIRECTION_H */
