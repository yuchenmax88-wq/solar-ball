#include "calibrate.h"
#include "sensor_scan.h"
#include "config.h"
#include "sun_calc.h"
#include "mqtt_4g.h"
#include "sensor_positions.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static const char *TAG = "calibrate";

calib_data_t g_calib;
uint8_t g_channel_to_position[SENSOR_COUNT];
static calib_state_t g_calib_state = CALIB_STATE_IDLE;

void calib_load(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    /* Initialize NVS if not already done */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash needs erase, erasing...");
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Set defaults: identity mapping */
    for (int i = 0; i < SENSOR_COUNT; i++) {
        g_channel_to_position[i] = i;
        g_calib.baseline[i] = 0;
        g_calib.channel_map[i] = i;
    }

    err = nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(err));
        ESP_LOGI(TAG, "Using default (identity) calibration");
        return;
    }

    /* Load channel mapping */
    size_t size = sizeof(g_calib.channel_map);
    err = nvs_get_blob(nvs_handle, CALIB_MAPPING_KEY, g_calib.channel_map, &size);
    if (err == ESP_OK) {
        /* Invert channel_map (position->physical) to g_channel_to_position (physical->position) */
        memset(g_channel_to_position, 0xFF, sizeof(g_channel_to_position));
        for (int i = 0; i < SENSOR_COUNT; i++) {
            int phys = g_calib.channel_map[i];
            if (phys >= 0 && phys < SENSOR_COUNT) {
                g_channel_to_position[phys] = i;
            }
        }
        ESP_LOGI(TAG, "Channel mapping loaded from NVS");
    } else {
        ESP_LOGI(TAG, "No mapping in NVS, using identity mapping");
    }

    /* Load baseline values */
    size = sizeof(g_calib.baseline);
    err = nvs_get_blob(nvs_handle, CALIB_BASELINE_KEY, g_calib.baseline, &size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Baseline calibration loaded from NVS");
    } else {
        ESP_LOGI(TAG, "No baseline in NVS");
    }

    nvs_close(nvs_handle);
}

int calib_save(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open for write failed: %s", esp_err_to_name(err));
        return -1;
    }

    err = nvs_set_blob(nvs_handle, CALIB_MAPPING_KEY, g_calib.channel_map, sizeof(g_calib.channel_map));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save mapping: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }

    err = nvs_set_blob(nvs_handle, CALIB_BASELINE_KEY, g_calib.baseline, sizeof(g_calib.baseline));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save baseline: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS commit failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Calibration saved to NVS");
    return 0;
}

/* ================================================================
 *  Sun Auto-Calibration
 *
 *  Uses the sun as a calibration light source. The algorithm:
 *    1. Get GPS location + time from 4G module
 *    2. Compute sun direction from solar ephemeris
 *    3. Scan all 80 physical channels
 *    4. Sort positions by predicted brightness (dot with sun direction)
 *    5. Sort physical channels by actual brightness (raw ADC reading)
 *    6. Map: brightest channel → brightest position (rank-based pairing)
 *    7. Save to NVS
 *
 *  This replaces 80 manual flashlight steps. One outdoor scan is enough.
 *  Direction accuracy: ~0.3-0.5° (comparable to perfect wiring).
 * ================================================================ */

/* Helper: insertion sort for 80-element arrays */
typedef struct {
    float  value;
    int    index;
} ranked_t;

static void rank_sort(ranked_t *items, int count) {
    for (int i = 1; i < count; i++) {
        ranked_t key = items[i];
        int j = i - 1;
        while (j >= 0 && items[j].value < key.value) {
            items[j + 1] = items[j];
            j--;
        }
        items[j + 1] = key;
    }
}

int calib_run_sun_auto(void) {
    int32_t lat_e6 = 0, lon_e6 = 0;
    uint32_t unix_ts = 0;
    vec3_t sun_dir;

    ESP_LOGI(TAG, "=== Sun Auto-Calibration ===");

    /* Step 1: Initialize 4G module */
    if (modem_init() != 0) {
        ESP_LOGE(TAG, "4G modem init failed - cannot auto-calibrate");
        printf("CAL:SUN_ERR 4G modem init failed\n");
        modem_power_off();
        return -1;
    }

    /* Step 2: Sync NTP time (best-effort) */
    modem_sync_time();

    /* Step 3: Get GPS location from cell tower */
    if (modem_get_location(&lat_e6, &lon_e6) != 0) {
        ESP_LOGE(TAG, "Cannot get location - cannot auto-calibrate");
        printf("CAL:SUN_ERR no location\n");
        modem_power_off();
        return -1;
    }

    /* Step 4: Get current time from modem */
    if (modem_get_unix_time(&unix_ts) != 0) {
        ESP_LOGW(TAG, "Cannot get time, using compile-time estimate");
        unix_ts = 1718000000;  /* fallback, not accurate enough */
    }

    /* Step 5: Compute sun direction */
    sun_dir = sun_calc_from_int(lat_e6, lon_e6, 0, unix_ts);
    ESP_LOGI(TAG, "Sun direction: (%.4f, %.4f, %.4f)", sun_dir.x, sun_dir.y, sun_dir.z);

    /* Check if sun is above horizon */
    if (sun_dir.z < 0.05f) {
        ESP_LOGW(TAG, "Sun too low (z=%.4f) - auto-cal may be inaccurate", sun_dir.z);
        printf("CAL:SUN_WARN sun_low=%.4f\n", sun_dir.z);
    }

    /* Step 6: Scan all 80 physical channels (raw values) */
    sensor_scan_init();
    vTaskDelay(pdMS_TO_TICKS(100));

    uint16_t raw_values[SENSOR_COUNT];
    memset(raw_values, 0, sizeof(raw_values));

    for (int phys = 0; phys < SENSOR_COUNT; phys++) {
        int bank = phys / MUX_CHANNELS_PER_BANK;
        int ch = phys % MUX_CHANNELS_PER_BANK;
        raw_values[phys] = sensor_read_raw(bank, ch);
    }

    /* Step 7: Rank positions by predicted brightness */
    ranked_t pos_ranked[SENSOR_COUNT];
    int active_positions = 0;
    for (int pos = 0; pos < SENSOR_COUNT; pos++) {
        float dot = SENSOR_POSITIONS[pos].x * sun_dir.x +
                    SENSOR_POSITIONS[pos].y * sun_dir.y +
                    SENSOR_POSITIONS[pos].z * sun_dir.z;
        float pred = (dot > 0.0f) ? dot : 0.0f;
        if (pred > 0.01f) active_positions++;
        pos_ranked[pos].value = pred;
        pos_ranked[pos].index = pos;
    }
    rank_sort(pos_ranked, SENSOR_COUNT);

    /* Step 8: Rank physical channels by brightness (invert raw ADC).
     * ALS-PT19 inverts: bright → low ADC. Use -raw so brightest = highest value. */
    ranked_t ch_ranked[SENSOR_COUNT];
    for (int phys = 0; phys < SENSOR_COUNT; phys++) {
        ch_ranked[phys].value = -(float)raw_values[phys];
        ch_ranked[phys].index = phys;
    }
    rank_sort(ch_ranked, SENSOR_COUNT);

    /* Step 9: Map brightest → brightest (position → physical channel) */
    for (int rank = 0; rank < SENSOR_COUNT; rank++) {
        int pos = pos_ranked[rank].index;
        int phys = ch_ranked[rank].index;
        g_calib.channel_map[pos] = (uint8_t)phys;
        g_channel_to_position[phys] = (uint8_t)pos;
    }

    /* Step 10: Save to NVS */
    if (calib_save() != 0) {
        printf("CAL:SUN_ERR NVS save failed\n");
        modem_power_off();
        return -1;
    }

    /* Step 11: Shut down 4G */
    modem_power_off();

    ESP_LOGI(TAG, "Sun auto-calibration complete: %d active positions", active_positions);
    printf("CAL:SUN_OK active=%d\n", active_positions);

    return 0;
}

/* Read a line from UART0 (console) with timeout */
static int uart_read_line(char *buf, int max_len, uint32_t timeout_ms) {
    int pos = 0;
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        size_t avail;
        uart_get_buffered_data_len(UART_NUM_0, &avail);
        if (avail > 0) {
            char c;
            if (uart_read_bytes(UART_NUM_0, (uint8_t *)&c, 1, 0) > 0) {
                if (c == '\n' || c == '\r') {
                    buf[pos] = '\0';
                    return pos;
                }
                if (pos < max_len - 1) {
                    buf[pos++] = c;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }
    buf[0] = '\0';
    return -1;
}

void calib_run_auto(void) {
    char line[64];
    int pos, channel;
    int line_len;

    /* Install UART0 driver for interactive calibration */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);

    g_calib_state = CALIB_STATE_WAITING;
    ESP_LOGI(TAG, "Auto-calibration mode started");
    printf("\n=== Solar Ball Auto-Calibration ===\n");
    printf("Commands:\n");
    printf("  CAL:SUN_AUTO  - Automatic sun-based calibration (place ball outdoors)\n");
    printf("  CAL:START     - Manual flashlight calibration (80-step guided)\n");
    printf("  CAL:STATUS    - Show calibration state\n");
    printf("  CAL:QUIT      - Exit calibration mode\n");
    printf("Ready.\n");

    while (1) {
        line_len = uart_read_line(line, sizeof(line), 10000);
        if (line_len < 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (strcmp(line, "CAL:START") == 0) {
            g_calib_state = CALIB_STATE_WAITING;
            printf("CAL:READY\n");
            printf("CAL:POS_COUNT=%d\n", SENSOR_COUNT);
            printf("Next: shine light on position #0 (top of ball)\n");

        } else if (strcmp(line, "CAL:SCAN") == 0) {
            g_calib_state = CALIB_STATE_SAMPLING;
            uint16_t min_val = 0xFFFF;
            int min_channel = -1;

            /* ALS-PT19 inverts: bright light → low ADC. Find LOWEST = brightest. */
            for (int bank = 0; bank < MUX_BANK_COUNT; bank++) {
                for (int ch = 0; ch < MUX_CHANNELS_PER_BANK; ch++) {
                    uint16_t val = sensor_read_raw(bank, ch);
                    if (val < min_val) {
                        min_val = val;
                        min_channel = bank * MUX_CHANNELS_PER_BANK + ch;
                    }
                }
            }

            printf("CAL:RESULT channel=%d value=%d\n", min_channel, min_val);
            g_calib_state = CALIB_STATE_WAITING;

        } else if (sscanf(line, "CAL:MAP %d %d", &pos, &channel) == 2) {
            if (pos >= 0 && pos < SENSOR_COUNT && channel >= 0 && channel < SENSOR_COUNT) {
                g_calib.channel_map[pos] = channel;
                g_channel_to_position[channel] = pos;
                printf("CAL:MAP_OK pos=%d channel=%d\n", pos, channel);
            } else {
                printf("CAL:MAP_ERR pos=%d channel=%d (out of range)\n", pos, channel);
            }

        } else if (strcmp(line, "CAL:SAVE") == 0) {
            if (calib_save() == 0) {
                printf("CAL:SAVE_OK\n");
                g_calib_state = CALIB_STATE_DONE;
            } else {
                printf("CAL:SAVE_ERR NVS write failed\n");
            }

        } else if (strcmp(line, "CAL:SUN_AUTO") == 0) {
            printf("CAL:SUN_START\n");
            int result = calib_run_sun_auto();
            if (result == 0) {
                g_calib_state = CALIB_STATE_DONE;
            } else {
                g_calib_state = CALIB_STATE_WAITING;
            }

        } else if (strcmp(line, "CAL:QUIT") == 0) {
            printf("CAL:QUIT_OK\n");
            g_calib_state = CALIB_STATE_IDLE;
            ESP_LOGI(TAG, "Auto-calibration exited");
            return;

        } else if (strcmp(line, "CAL:STATUS") == 0) {
            printf("CAL:STATE=%d\n", g_calib_state);
            printf("CAL:SENSORS=%d\n", SENSOR_COUNT);
        }
    }
}

calib_state_t calib_get_state(void) {
    return g_calib_state;
}

int calib_has_mapping(void) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (g_calib.channel_map[i] != i) return 1;
    }
    return 0;
}

int calib_has_baseline(void) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (g_calib.baseline[i] > 0) return 1;
    }
    return 0;
}
