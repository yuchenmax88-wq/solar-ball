#include "sensor_scan.h"
#include "sensor_calib.h"
#include "config.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "sensor_scan";

/* External calibration data (defined in calibrate.c) */
extern calib_data_t g_calib;
extern uint8_t g_channel_to_position[SENSOR_COUNT];

/* I2C handles */
#define I2C1_NUM  I2C_NUM_0
#define I2C2_NUM  I2C_NUM_1

/* MUX enable pins array */
static const int mux_en_pins[MUX_BANK_COUNT] = MUX_EN_PINS;
static const int mux_sel_pins[4] = { MUX_S0_GPIO, MUX_S1_GPIO, MUX_S2_GPIO, MUX_S3_GPIO };

/* Which ADS1115 channel reads which MUX bank */
/* Bank 0-3 -> ADS1115 #1 channels 0-3; Bank 4 -> ADS1115 #2 channel 0 */
static const int bank_to_i2c_bus[MUX_BANK_COUNT] = { I2C1_NUM, I2C1_NUM, I2C1_NUM, I2C1_NUM, I2C2_NUM };
static const int bank_to_ads_addr[MUX_BANK_COUNT] = { ADS1115_ADDR_1, ADS1115_ADDR_1, ADS1115_ADDR_1, ADS1115_ADDR_1, ADS1115_ADDR_2 };
static const int bank_to_ads_channel[MUX_BANK_COUNT] = { 0, 1, 2, 3, 0 };

static void select_mux_channel(int channel) {
    gpio_set_level(mux_sel_pins[0], (channel >> 0) & 1);
    gpio_set_level(mux_sel_pins[1], (channel >> 1) & 1);
    gpio_set_level(mux_sel_pins[2], (channel >> 2) & 1);
    gpio_set_level(mux_sel_pins[3], (channel >> 3) & 1);
}

static void enable_mux_bank(int bank) {
    for (int b = 0; b < MUX_BANK_COUNT; b++) {
        gpio_set_level(mux_en_pins[b], (b == bank) ? 0 : 1);
    }
}

static uint16_t ads1115_read_single(i2c_port_t i2c_num, uint8_t addr, int channel) {
    uint8_t config_reg[3];
    uint8_t data[2];
    uint16_t raw;

    /* Config: single-shot, +/-4.096V PGA, 128 SPS */
    uint16_t config = 0;
    config |= (1 << 15);            /* OS: Start single conversion */
    config |= (channel & 3) << 12;  /* MUX: AINp = AIN(channel), AINn = GND */
    config |= (1 << 9);             /* PGA: 001 = +/-4.096V (matches 3.3V circuit) */
    config |= (1 << 8);             /* MODE: 1 = Power-down single-shot mode */
    config |= (4 << 5);             /* DR: 4 = 128 SPS (7.8ms/conversion) */
    config |= (3 << 0);             /* COMP_QUE: 11 = disable comparator */

    config_reg[0] = (config >> 8) & 0xFF;
    config_reg[1] = config & 0xFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, config_reg, 2, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C write config failed (addr=0x%02x ch=%d): %s", addr, channel, esp_err_to_name(ret));
        return 0;
    }

    /* Wait for conversion to complete (at 128 SPS, ~7.8ms; wait 10ms for safety) */
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Read conversion result */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true); /* Read from conversion register */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C read conversion failed (addr=0x%02x): %s", addr, esp_err_to_name(ret));
        return 0;
    }

    raw = ((uint16_t)data[0] << 8) | data[1];
    return raw;
}

static bool s_scan_initialized = false;

void sensor_scan_init(void) {
    if (s_scan_initialized) {
        return;
    }
    s_scan_initialized = true;

    /* Configure MUX address select lines as outputs */
    for (int i = 0; i < 4; i++) {
        gpio_set_direction(mux_sel_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(mux_sel_pins[i], 0);
    }

    /* Configure MUX enable pins as outputs, all disabled (HIGH = disabled) */
    for (int b = 0; b < MUX_BANK_COUNT; b++) {
        gpio_set_direction(mux_en_pins[b], GPIO_MODE_OUTPUT);
        gpio_set_level(mux_en_pins[b], 1);
    }

    /* Initialize I2C buses */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C1_SDA_GPIO,
        .scl_io_num = I2C1_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = ADC_I2C_CLOCK_HZ,
    };
    i2c_param_config(I2C1_NUM, &conf);
    i2c_driver_install(I2C1_NUM, conf.mode, 0, 0, 0);

    conf.sda_io_num = I2C2_SDA_GPIO;
    conf.scl_io_num = I2C2_SCL_GPIO;
    i2c_param_config(I2C2_NUM, &conf);
    i2c_driver_install(I2C2_NUM, conf.mode, 0, 0, 0);

    ESP_LOGI(TAG, "Sensor scan initialized: %d sensors via 2x ADS1115 + 5x CD74HC4067",
             SENSOR_COUNT);
}

uint16_t sensor_read_raw(int bank, int channel) {
    if (bank < 0 || bank >= MUX_BANK_COUNT) return 0;
    if (channel < 0 || channel >= MUX_CHANNELS_PER_BANK) return 0;

    enable_mux_bank(bank);
    select_mux_channel(channel);
    vTaskDelay(pdMS_TO_TICKS(MUX_SETTLE_MS));

    uint16_t value = ads1115_read_single(
        bank_to_i2c_bus[bank],
        bank_to_ads_addr[bank],
        bank_to_ads_channel[bank]
    );

    enable_mux_bank(-1); /* disable all */
    return value;
}

int sensor_scan_all(float out[SENSOR_COUNT]) {
    uint32_t raw_sum[SENSOR_COUNT];
    memset(raw_sum, 0, sizeof(raw_sum));
    uint16_t max_raw = 0;

    /* Scan each sensor with oversampling */
    for (int bank = 0; bank < MUX_BANK_COUNT; bank++) {
        enable_mux_bank(bank);

        for (int ch = 0; ch < MUX_CHANNELS_PER_BANK; ch++) {
            select_mux_channel(ch);
            vTaskDelay(pdMS_TO_TICKS(MUX_SETTLE_MS));

            int sensor_idx = bank * MUX_CHANNELS_PER_BANK + ch;

            /* Oversample */
            for (int s = 0; s < SENSOR_OVERSAMPLE; s++) {
                uint16_t v = ads1115_read_single(
                    bank_to_i2c_bus[bank],
                    bank_to_ads_addr[bank],
                    bank_to_ads_channel[bank]
                );
                raw_sum[sensor_idx] += v;
            }

            /* Track maximum for normalization */
            uint16_t avg = raw_sum[sensor_idx] / SENSOR_OVERSAMPLE;
            if (avg > max_raw) max_raw = avg;
        }
    }

    enable_mux_bank(-1); /* disable all MUX chips */

    if (max_raw == 0) {
        ESP_LOGW(TAG, "All sensors read 0 - check wiring and power");
        for (int i = 0; i < SENSOR_COUNT; i++) {
            out[i] = 0.0f;
        }
        return -1;
    }

    /*
     * Normalize using calibration baselines and apply channel mapping.
     *
     * IMPORTANT: ALS-PT19 is wired common-emitter (3.3V → 10kΩ → collector → MUX).
     * Bright light → more photocurrent → lower collector voltage → lower ADC reading.
     * So we INVERT: bright sensor (low raw) → high normalized weight.
     */
    for (int physical = 0; physical < SENSOR_COUNT; physical++) {
        uint16_t raw_avg = raw_sum[physical] / SENSOR_OVERSAMPLE;
        uint8_t position_idx = g_channel_to_position[physical];

        if (position_idx >= SENSOR_COUNT) {
            continue;  /* unmapped channel, skip */
        }

        if (g_calib.baseline[physical] > 0) {
            float normalized = 1.0f - (float)raw_avg / (float)g_calib.baseline[physical];
            if (normalized < 0.0f) normalized = 0.0f;
            if (normalized > 1.0f) normalized = 1.0f;
            out[position_idx] = normalized;
        } else {
            /* No calibration data: invert raw / max_raw so bright = high weight */
            out[position_idx] = 1.0f - (float)raw_avg / (float)(max_raw > 0 ? max_raw : 1);
        }
    }

    return 0;
}
