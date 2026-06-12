#include "calibrate.h"
#include "sensor_scan.h"
#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

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
        /* Copy to g_channel_to_position */
        memcpy(g_channel_to_position, g_calib.channel_map, sizeof(g_channel_to_position));
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
    uint16_t max_val;
    int max_channel;
    int line_len;

    /* Configure UART0 for line reading */
    uart_set_baudrate(UART_NUM_0, 115200);

    g_calib_state = CALIB_STATE_WAITING;
    ESP_LOGI(TAG, "Auto-calibration mode started");
    printf("\n=== Solar Ball Auto-Calibration ===\n");
    printf("Ready to receive commands via UART.\n");

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
            max_val = 0;
            max_channel = -1;

            /* Scan all 80 channels to find the brightest one */
            for (int bank = 0; bank < MUX_BANK_COUNT; bank++) {
                for (int ch = 0; ch < MUX_CHANNELS_PER_BANK; ch++) {
                    uint16_t val = sensor_read_raw(bank, ch);
                    if (val > max_val) {
                        max_val = val;
                        max_channel = bank * MUX_CHANNELS_PER_BANK + ch;
                    }
                }
            }

            printf("CAL:RESULT channel=%d value=%d\n", max_channel, max_val);
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
