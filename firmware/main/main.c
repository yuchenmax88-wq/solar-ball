/*
 * Solar Ball Firmware v1.0
 * ========================
 *
 * Main entry point. Flow:
 *   1. Boot
 *   2. Initialize peripherals (I2C, GPIO, ADC)
 *   3. Load calibration from NVS
 *   4. Scan all 80 sensors
 *   5. Compute brightest direction vector
 *   6. Initialize 4G module and publish via MQTT
 *   7. Enter deep sleep
 *
 * On first boot (no calibration), or if GPIO 0 (BOOT) is held during startup,
 * enters auto-calibration mode via UART.
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
#include "direction.h"
#include "mqtt_4g.h"
#include "power.h"
#include "calibrate.h"

static const char *TAG = "solar-ball";

/* Check if BOOT button is held to enter calibration mode */
static int is_calibration_mode(void) {
    /* GPIO 0 (BOOT button) is pulled up; LOW when pressed */
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_0, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(50));
    int level = gpio_get_level(GPIO_NUM_0);
    return (level == 0);  /* LOW = button held */
}

void app_main(void) {
    ESP_LOGI(TAG, "Solar Ball v%s booting...", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "Ball ID: %s", BALL_ID);

    /* Initialize power monitoring */
    power_init();

    /* Load calibration data from NVS */
    calib_load();

    /* Check if we should enter calibration mode */
    if (is_calibration_mode()) {
        ESP_LOGI(TAG, "BOOT button held - entering calibration mode");
        sensor_scan_init();
        calib_run_auto();
        ESP_LOGI(TAG, "Calibration complete, proceeding to normal operation");
    }

    /* Check if calibration is valid */
    int has_calibration = calib_has_mapping() || calib_has_baseline();

    if (!has_calibration) {
        ESP_LOGW(TAG, "No calibration data found. Using default normalization.");
        ESP_LOGW(TAG, "Hold BOOT button at startup to run auto-calibration.");
    }

    /* Initialize sensor scanning */
    sensor_scan_init();

    /* Scan sensors */
    float sensor_values[SENSOR_COUNT];
    int scan_ok = sensor_scan_all(sensor_values);
    if (scan_ok != 0) {
        ESP_LOGW(TAG, "Sensor scan returned errors, continuing anyway");
    }

    /* Compute direction vector */
    vec3_t direction;
    int direction_ok = direction_compute(sensor_values, &direction);

    /* Read battery status */
    uint32_t battery_mv = power_get_millivolts();
    uint8_t soc = power_get_soc();

    /* Initialize 4G and publish */
    int8_t rssi = -999;
    int publish_ok = -1;

    if (modem_init() == 0) {
        /* Sync time (best-effort) */
        modem_sync_time();

        /* Get signal strength */
        rssi = modem_get_rssi();

        /* Get actual Unix time from modem (corrected for timezone) */
        uint32_t unix_ts = 0;
        modem_get_unix_time(&unix_ts);

        /* Build and publish direction packet */
        direction_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        strncpy(pkt.id, BALL_ID, sizeof(pkt.id) - 1);
        pkt.ts = unix_ts;
        pkt.dx = direction.x;
        pkt.dy = direction.y;
        pkt.dz = direction.z;
        pkt.soc = soc;
        pkt.rssi = rssi;

        publish_ok = mqtt_publish_direction(&pkt);

        /* Power off modem */
        modem_power_off();
    } else {
        ESP_LOGE(TAG, "4G modem initialization failed");
    }

    /* Log summary */
    ESP_LOGI(TAG, "=== Solar Ball Report ===");
    ESP_LOGI(TAG, "Direction: (%.4f, %.4f, %.4f)  valid=%d",
             direction.x, direction.y, direction.z, direction_ok == 0);
    ESP_LOGI(TAG, "Battery: %umV (%u%%)", battery_mv, soc);
    ESP_LOGI(TAG, "4G RSSI: %d dBm  Published: %s",
             rssi, publish_ok > 0 ? "YES" : "NO");

    /* Enter deep sleep */
    ESP_LOGI(TAG, "Cycle complete. Sleeping for %d seconds.", DEEP_SLEEP_WAKEUP_S);
    vTaskDelay(pdMS_TO_TICKS(100));
    power_deep_sleep();
}
