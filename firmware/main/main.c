/*
 * Solar Ball Firmware v1.1
 * ========================
 *
 * Main entry point. Flow:
 *   1. Boot, initialize peripherals, load calibration
 *   2. Scan all 80 sensors + acquire raw values for diagnostics
 *   3. Analyze sensor faults, weather conditions, compute confidence
 *   4. Compute brightest direction vector
 *   5. Update display with status
 *   6. Initialize 4G module and publish via MQTT
 *   7. Enter deep sleep
 *
 * On BOOT button hold during startup: enters calibration mode via UART.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "config.h"
#include "sensor_positions.h"
#include "sensor_calib.h"
#include "sensor_scan.h"
#include "sensor_diag.h"
#include "direction.h"
#include "mqtt_4g.h"
#include "power.h"
#include "calibrate.h"
#include "display.h"
#include "remote_diag.h"

static const char *TAG = "solar-ball";

static int is_calibration_mode(void) {
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_0, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(50));
    return (gpio_get_level(GPIO_NUM_0) == 0);
}

void app_main(void) {
    ESP_LOGI(TAG, "Solar Ball v%s booting...", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "Ball ID: %s", BALL_ID);

    /* ---- Init ---- */
    power_init();
    calib_load();
    remote_diag_init();
    sensor_scan_init();  /* must init I2C before display */
    display_init();

    /* ---- Calibration mode check ---- */
    if (is_calibration_mode()) {
        ESP_LOGI(TAG, "BOOT button held - entering calibration mode");
        display_show_calib("Calibration mode", 0, 1);
        calib_run_auto();
        display_show_calib("Done", 1, 1);
        ESP_LOGI(TAG, "Calibration complete, proceeding to normal operation");
    }

    int has_calibration = calib_has_mapping() || calib_has_baseline();
    if (!has_calibration) {
        ESP_LOGW(TAG, "No calibration data found. Using default normalization.");
    }

    /* ---- Sensor scan (single pass: normalized + raw diagnostics) ---- */
    float sensor_values[SENSOR_COUNT];
    uint16_t raw_avgs[SENSOR_COUNT];
    int scan_ok = sensor_scan_all_with_raw(sensor_values, raw_avgs);

    /* Build diagnostic result from the same scan */
    sensor_diag_result_t diag;
    memset(&diag, 0, sizeof(diag));
    diag.max_raw = 0;
    diag.min_raw = 0xFFFF;
    float raw_mean = 0.0f;

    for (int phys = 0; phys < SENSOR_COUNT; phys++) {
        diag.raw_values[phys] = raw_avgs[phys];
        if (raw_avgs[phys] > diag.max_raw) diag.max_raw = raw_avgs[phys];
        if (raw_avgs[phys] < diag.min_raw) diag.min_raw = raw_avgs[phys];
        raw_mean += (float)raw_avgs[phys];
    }
    raw_mean /= SENSOR_COUNT;

    float variance_sum = 0.0f;
    for (int phys = 0; phys < SENSOR_COUNT; phys++) {
        float diff = (float)raw_avgs[phys] - raw_mean;
        variance_sum += diff * diff;
    }
    diag.variance = variance_sum / SENSOR_COUNT;

    if (scan_ok != 0) {
        diag.adc_errors = 1;
    }
    remote_diag_record_scan();

    /* ---- Sensor diagnostics ---- */
    uint16_t error_mask = FAULT_NONE;
    uint8_t confidence = 255;
    uint8_t flags = 0;
    sensor_diag_analyze(&diag, &error_mask, &confidence, &flags);

    /* ---- Direction computation ---- */
    vec3_t direction;
    int direction_ok = direction_compute(sensor_values, &direction);

    /* If overcast or night, force direction to (0,0,1) with low confidence */
    if ((flags & FLAG_OVERCAST) || (flags & FLAG_NIGHT)) {
        direction.x = 0.0f;
        direction.y = 0.0f;
        direction.z = 1.0f;
        direction_ok = -1;
    }

    /* ---- Battery ---- */
    uint32_t battery_mv = power_get_millivolts();
    uint8_t soc = power_get_soc();
    if (soc < 10) {
        error_mask |= FAULT_LOW_BATTERY;
    }

    /* ---- 4G and MQTT ---- */
    int16_t rssi = -999;
    int publish_ok = -1;
    uint32_t unix_ts = 0;

    if (modem_init() == 0) {
        modem_sync_time();
        rssi = modem_get_rssi();
        modem_get_unix_time(&unix_ts);

        direction_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        strncpy(pkt.id, BALL_ID, sizeof(pkt.id) - 1);
        pkt.ts = unix_ts;
        pkt.dx = direction.x;
        pkt.dy = direction.y;
        pkt.dz = direction.z;
        pkt.soc = soc;
        pkt.rssi = rssi;
        pkt.error_mask = error_mask;
        pkt.confidence = confidence;
        pkt.flags = flags;

        publish_ok = mqtt_publish_direction(&pkt);
        if (publish_ok < 0) {
            error_mask |= FAULT_MQTT_ERROR;
        }
        modem_power_off();
    } else {
        ESP_LOGE(TAG, "4G modem initialization failed");
        error_mask |= FAULT_MODEM_ERROR;
    }

    /* ---- Record stats ---- */
    remote_diag_record_publish(publish_ok > 0 ? 1 : 0);

    /* ---- Display update (MIP reflective: image persists during sleep) ---- */
    display_show_status(direction.x, direction.y, direction.z,
                        soc, rssi, error_mask, confidence);

    /* ---- Log summary ---- */
    ESP_LOGI(TAG, "=== Solar Ball Report ===");
    ESP_LOGI(TAG, "Direction: (%.4f, %.4f, %.4f)  valid=%d  conf=%u",
             direction.x, direction.y, direction.z, direction_ok == 0, confidence);
    ESP_LOGI(TAG, "Battery: %umV (%u%%)  RSSI: %d dBm",
             battery_mv, soc, rssi);
    ESP_LOGI(TAG, "Faults: 0x%04X  Flags: 0x%02X  Published: %s",
             error_mask, flags, publish_ok > 0 ? "YES" : "NO");

    /* ---- Sleep ---- */
    ESP_LOGI(TAG, "Cycle complete. Sleeping for %d seconds.", DEEP_SLEEP_WAKEUP_S);
    vTaskDelay(pdMS_TO_TICKS(100));
    power_deep_sleep();
}
