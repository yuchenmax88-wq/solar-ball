#include "sensor_diag.h"
#include "sensor_scan.h"
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


uint16_t sensor_diag_check_channel(uint16_t raw_value, uint16_t min_raw, uint16_t max_raw) {
    (void)min_raw;
    (void)max_raw;

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
            diag->raw_values[phys], diag->min_raw, diag->max_raw);

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
