#include "direction.h"
#include "sensor_positions.h"
#include <math.h>
#include <stdint.h>

/*
 * Bottom 30 degrees threshold.
 * Sensors with z < cos(150 deg) ~ -0.866 are excluded.
 */
#define Z_EXCLUDE_THRESHOLD  (-0.8660254f)  /* cos(150°) */

int direction_compute(const float norm[SENSOR_COUNT], vec3_t *out_dir) {
    float sum_w = 0.0f;
    float wx = 0.0f, wy = 0.0f, wz = 0.0f;
    int active_count = 0;

    for (int i = 0; i < SENSOR_COUNT; i++) {
        /* Skip sensors on the bottom 30 degrees of the sphere */
        if (SENSOR_POSITIONS[i].z < Z_EXCLUDE_THRESHOLD) {
            continue;
        }

        float w = norm[i];
        if (w < 0.001f) continue;  /* skip near-zero readings for stability */

        wx += w * SENSOR_POSITIONS[i].x;
        wy += w * SENSOR_POSITIONS[i].y;
        wz += w * SENSOR_POSITIONS[i].z;
        sum_w += w;
        active_count++;
    }

    if (active_count < 3 || sum_w < 0.001f) {
        /* Not enough light to determine direction */
        out_dir->x = 0.0f;
        out_dir->y = 0.0f;
        out_dir->z = 1.0f;  /* default: point straight up */
        return -1;
    }

    /* Weighted centroid */
    float cx = wx / sum_w;
    float cy = wy / sum_w;
    float cz = wz / sum_w;

    /* Normalize to unit vector */
    float len = sqrtf(cx * cx + cy * cy + cz * cz);
    if (len < 0.001f) {
        out_dir->x = 0.0f;
        out_dir->y = 0.0f;
        out_dir->z = 1.0f;
        return -1;
    }

    out_dir->x = cx / len;
    out_dir->y = cy / len;
    out_dir->z = cz / len;

    return 0;
}
