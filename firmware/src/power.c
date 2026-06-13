#include "power.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "power";

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle;
static bool adc_cali_done = false;

static const adc_channel_t ADC_CHANNEL = ADC_CHANNEL_0;

void power_init(void) {
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&unit_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration scheme creation failed, using raw readings");
    }
    adc_cali_done = (ret == ESP_OK);

    ESP_LOGI(TAG, "Power management initialized");
}

uint32_t power_get_millivolts(void) {
    uint32_t adc_reading = 0;

    for (int i = 0; i < 16; i++) {
        int raw;
        if (adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw) == ESP_OK) {
            adc_reading += raw;
        }
    }
    adc_reading /= 16;

    uint32_t voltage_mv;
    if (adc_cali_done) {
        adc_cali_raw_to_voltage(adc_cali_handle, adc_reading, &voltage_mv);
    } else {
        voltage_mv = (adc_reading * 3300) / 4095;
    }
    voltage_mv = (uint32_t)(voltage_mv * BATTERY_DIVIDER_RATIO);

    return voltage_mv;
}

uint8_t power_get_soc(void) {
    uint32_t mv = power_get_millivolts();
    uint8_t soc;

    if (mv >= BATTERY_FULL_MV) {
        soc = 100;
    } else if (mv <= BATTERY_EMPTY_MV) {
        soc = 0;
    } else {
        /* Linear interpolation between empty and full */
        soc = (uint8_t)(100 * (mv - BATTERY_EMPTY_MV) / (float)(BATTERY_FULL_MV - BATTERY_EMPTY_MV));
    }

    return soc;
}

void power_deep_sleep(void) {
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", DEEP_SLEEP_WAKEUP_S);

    /* Disable all other wakeup sources, then enable timer only */
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_WAKEUP_S * 1000000ULL);

    /* Go to sleep */
    esp_deep_sleep_start();
}
