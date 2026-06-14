#include "sensor_diag.h"
#include "sensor_scan.h"
#include "calibrate.h"
#include "mqtt_protocol.h"
#include "config.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "sensor_diag";

/* Variance threshold: CV < 0.15 → uniform (overcast) */
#define OVERCAST_CV_THRESHOLD   0.15f

/* Open circuit: raw > 90% of max ADC range (sensor stuck dark) */
#define OPEN_THRESHOLD          (0.90f * 65535.0f)

/* Short circuit: raw < 2% of max ADC range (sensor stuck bright) */
#define SHORT_THRESHOLD         (0.02f * 65535.0f)

/* Saturation: raw < 5% of range in daylight (sensor overloaded) */
#define SATURATION_THRESHOLD    (0.05f * 65535.0f)


uint16_t sensor_diag_check_channel(uint16_t raw_value) {
    if (raw_value >= (uint16_t)OPEN_THRESHOLD) {
        return FAULT_SENSOR_OPEN;
    }
    if (raw_value <= (uint16_t)SHORT_THRESHOLD) {
        return FAULT_SENSOR_SHORT;
    }
    if (raw_value <= (uint16_t)SATURATION_THRESHOLD) {
        return FAULT_SENSOR_SATURATED;
    }
    return 0;
}


void sensor_diag_analyze(const sensor_diag_result_t *diag,
                         uint16_t *error_mask,
                         uint8_t *confidence,
                         uint8_t *flags) {
    uint16_t faults = FAULT_NONE;
    uint8_t conf = 255;  /* start with full confidence */
    uint8_t fl = 0;

    int fault_count = 0;
    int saturated_count = 0;

    /* ---- Check individual sensor faults ---- */
    for (int phys = 0; phys < SENSOR_COUNT; phys++) {
        uint16_t fault = sensor_diag_check_channel(
            diag->raw_values[phys]);

        if (fault == FAULT_SENSOR_OPEN || fault == FAULT_SENSOR_SHORT) {
            faults |= fault;
            fault_count++;
        }
        if (fault == FAULT_SENSOR_SATURATED) {
            faults |= FAULT_SENSOR_SATURATED;
            saturated_count++;
        }
    }

    /* ---- ADC communication errors ---- */
    if (diag->adc_errors > 0) {
        faults |= FAULT_ADC_ERROR;
        conf -= (uint8_t)(diag->adc_errors * 5);
    }

    /* ---- Overcast / uniform illumination detection ---- */
    if (diag->max_raw > 0 && diag->min_raw < diag->max_raw) {
        float mean = (diag->max_raw + diag->min_raw) / 2.0f;
        if (mean > 0.1f) {
            float cv = sqrtf(diag->variance) / mean;
            if (cv < OVERCAST_CV_THRESHOLD) {
                faults |= FAULT_OVERCAST_SKY;
                fl |= FLAG_OVERCAST;
                /* Overcast: reduce confidence significantly */
                conf = (uint8_t)(conf * 0.4f);
            }
        }
    }

    /* ---- Nighttime detection ---- */
    /* At night: all sensors near max raw (dark), so max_raw is very high
     * and the ratio min_raw/max_raw approaches 1.0 (all uniform dark) */
    if (diag->max_raw > 0) {
        float brightness = 1.0f - (float)diag->min_raw / (float)diag->max_raw;
        /* brightness = 0 means all sensors equally dark (night) */
        /* brightness > 0 means some sensors much brighter than others (day) */
        if (brightness < 0.01f && diag->max_raw > (uint16_t)OPEN_THRESHOLD * 0.8f) {
            faults |= FAULT_NO_SIGNAL;
            fl |= FLAG_NIGHT;
            conf = (uint8_t)(conf * 0.1f);
        }
    }

    /* ---- Calibration status ---- */
    if (calib_has_mapping()) {
        fl |= FLAG_CALIBRATED;
    } else {
        faults |= FAULT_NO_CALIBRATION;
    }
    if (calib_has_baseline()) {
        fl |= FLAG_BASELINE_CAL;
    }

    /* ---- Confidence reduction for fault count ---- */
    if (fault_count > 0) {
        conf = (uint8_t)(conf * (1.0f - fault_count * 0.05f));
        if (conf < 10) conf = 10;
    }

    /* ---- Direction validity ---- */
    if (fault_count < 3) {
        fl |= FLAG_DIRECTION_VALID;
    }

    *error_mask = faults;
    *confidence = conf;
    *flags = fl;

    ESP_LOGI(TAG, "Diagnostics: faults=0x%04X conf=%u flags=0x%02X "
             "(faults:%d sat:%d overcast:%d night:%d)",
             faults, conf, fl, fault_count, saturated_count,
             (faults & FAULT_OVERCAST_SKY) ? 1 : 0,
             (faults & FAULT_NO_SIGNAL) ? 1 : 0);
}

int sensor_diag_deep_analyze(const sensor_diag_result_t *diag,
                             char *report, size_t report_len) {
    if (!diag || !report || report_len < 64) return 0;

    int offset = 0;
    int remain = (int)report_len - 1;

#define DEEP_APPEND(fmt, ...) do { \
    if (offset < remain) { \
        int w = snprintf(report + offset, remain - offset, fmt, ##__VA_ARGS__); \
        if (w > 0) offset += w; \
        if (offset >= remain) offset = remain; \
    } \
} while(0)
    int open_channels[16], open_count = 0;
    int shorted_channels[16], shorted_count = 0;
    int saturated_channels[16], saturated_count = 0;
    float sum_raw = 0;

    for (int phys = 0; phys < SENSOR_COUNT; phys++) {
        uint16_t raw = diag->raw_values[phys];
        sum_raw += raw;
        if (raw >= (uint16_t)OPEN_THRESHOLD) {
            if (open_count < 16) open_channels[open_count++] = phys;
        }
        if (raw <= (uint16_t)SHORT_THRESHOLD) {
            if (shorted_count < 16) shorted_channels[shorted_count++] = phys;
        }
        if (raw <= (uint16_t)SATURATION_THRESHOLD && raw > (uint16_t)SHORT_THRESHOLD) {
            if (saturated_count < 16) saturated_channels[saturated_count++] = phys;
        }
    }

    int total_faults = open_count + shorted_count + saturated_count;

    if (open_count > 0) {
        DEEP_APPEND("%d sensor(s) open (ch:", open_count);
        for (int i = 0; i < open_count && i < 4; i++) {
            DEEP_APPEND("%s%d", i > 0 ? "," : "", open_channels[i]);
        }
        if (open_count > 4) { DEEP_APPEND(",..."); }
        DEEP_APPEND("). ");
    }

    if (shorted_count > 0) {
        DEEP_APPEND("%d sensor(s) shorted (ch:", shorted_count);
        for (int i = 0; i < shorted_count && i < 4; i++) {
            DEEP_APPEND("%s%d", i > 0 ? "," : "", shorted_channels[i]);
        }
        if (shorted_count > 4) { DEEP_APPEND(",..."); }
        DEEP_APPEND("). ");
    }

    if (saturated_count > 0) {
        DEEP_APPEND("%d sensor(s) saturated (ch:", saturated_count);
        for (int i = 0; i < saturated_count && i < 4; i++) {
            DEEP_APPEND("%s%d", i > 0 ? "," : "", saturated_channels[i]);
        }
        if (saturated_count > 4) { DEEP_APPEND(",..."); }
        DEEP_APPEND("). ");
    }

    if (diag->adc_errors > 0) {
        DEEP_APPEND("%d I2C/ADC error(s). ", diag->adc_errors);
    }

    bool overcast = false;
    bool night = false;

    if (diag->max_raw > 0 && diag->min_raw < diag->max_raw) {
        float mean = (diag->max_raw + diag->min_raw) / 2.0f;
        if (mean > 0.1f) {
            float cv = sqrtf(diag->variance) / mean;
            if (cv < OVERCAST_CV_THRESHOLD) {
                overcast = true;
                DEEP_APPEND("Overcast sky (CV=%.2f). ", (double)cv);
            }
        }
    }

    if (diag->max_raw > 0) {
        float brightness = 1.0f - (float)diag->min_raw / (float)diag->max_raw;
        if (brightness < 0.01f && diag->max_raw > (uint16_t)(OPEN_THRESHOLD * 0.8f)) {
            night = true;
            DEEP_APPEND("Nighttime (brightness=%.3f). ", (double)brightness);
        }
    }

    float mean_raw = sum_raw / SENSOR_COUNT;
    DEEP_APPEND("Mean ADC=%d. ", (int)mean_raw);

    if (total_faults == 0 && !overcast && !night) {
        DEEP_APPEND("All sensors OK. Clear sky.");
    } else if (overcast && total_faults == 0) {
        DEEP_APPEND("Root cause: uniform cloud cover — point zenith.");
    } else if (night) {
        DEEP_APPEND("Root cause: insufficient light — standby mode.");
    } else if (open_count >= 3) {
        DEEP_APPEND("Root cause: possible physical damage (multiple opens).");
    } else if (total_faults <= 2) {
        DEEP_APPEND("Minor faults (%d sensors). Direction still valid.", total_faults);
    } else {
        DEEP_APPEND("Multiple fault types. Check wiring.");
    }

    report[offset] = '\0';
    return offset;
}
