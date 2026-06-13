#ifndef CONFIG_H
#define CONFIG_H

/* ============================================================
 *  Configuration header for Solar Ball firmware v1.0
 *  Edit these values to match your hardware setup.
 * ============================================================ */

/* ---------- Ball Identity ---------- */
#define BALL_ID                 "ball-001"
#define FIRMWARE_VERSION        "1.1.0"

/* ---------- Sensor Sampling ---------- */
#define SENSOR_COUNT            80
#define ADC_I2C_CLOCK_HZ        400000       /* 400 kHz fast mode */
#define MUX_SETTLE_MS           2            /* MUX channel switch settle time */
#define SENSOR_OVERSAMPLE       8            /* oversampling factor per sensor */
#define SENSOR_SCAN_INTERVAL_S  5            /* seconds between scans */

/* ---------- GPIO Pin Mapping ---------- */
/* MUX address lines (shared across all 5 MUX chips) */
#define MUX_S0_GPIO             32
#define MUX_S1_GPIO             33
#define MUX_S2_GPIO             25
#define MUX_S3_GPIO             26

/* MUX enable pins (individual chip select, active low) */
#define MUX_EN0_GPIO            27   /* MUX #1: sensors  0-15  -> ADS1115 #1 CH0 */
#define MUX_EN1_GPIO            14   /* MUX #2: sensors 16-31  -> ADS1115 #1 CH1 */
#define MUX_EN2_GPIO            12   /* MUX #3: sensors 32-47  -> ADS1115 #2 CH0 */
#define MUX_EN3_GPIO            13   /* MUX #4: sensors 48-63  -> ADS1115 #2 CH1 */
#define MUX_EN4_GPIO            15   /* MUX #5: sensors 64-79  -> ADS1115 #2 CH2 */

#define MUX_EN_PINS             { MUX_EN0_GPIO, MUX_EN1_GPIO, MUX_EN2_GPIO, MUX_EN3_GPIO, MUX_EN4_GPIO }
#define MUX_BANK_COUNT          5
#define MUX_CHANNELS_PER_BANK   16

/* I2C buses */
#define I2C1_SDA_GPIO           21
#define I2C1_SCL_GPIO           22
#define I2C2_SDA_GPIO           18
#define I2C2_SCL_GPIO           19

/* ADS1115 I2C addresses */
#define ADS1115_ADDR_1          0x48
#define ADS1115_ADDR_2          0x49

/* ADS1115 channel mapping (which ADS input reads which MUX output) */
/* ADS1115 #1 (addr 0x48): MUX bank 0-3 on AIN0-AIN3 */
/* ADS1115 #2 (addr 0x49): MUX bank 4 on AIN0, rest unused */

/* ---------- Display (Sharp Memory LCD, SPI bit-bang) ---------- */
#define DISPLAY_MOSI_GPIO       2
#define DISPLAY_SCLK_GPIO       4
#define DISPLAY_CS_GPIO         5

/* ---------- 4G Module (SIM7600G) ---------- */
#define MODEM_UART_TX_GPIO      17
#define MODEM_UART_RX_GPIO      16
#define MODEM_PWR_KEY_GPIO      23  /* power on/off key */
#define MODEM_RESET_GPIO        2   /* NOTE: GPIO 2 shared with display MOSI.
                                       Hardware modem reset not available when
                                       display is present. Display takes priority. */
#define MODEM_BAUDRATE          115200

#define MQTT_BROKER_HOST        "broker.emqx.io"
#define MQTT_BROKER_PORT        1883
#define MQTT_CLIENT_ID          BALL_ID
#define MQTT_TOPIC_PREFIX       "/solar/ball/"
#define MQTT_QOS                1
#define MQTT_KEEPALIVE_S        60

/* APN for IoT SIM (China Mobile OneLink) */
#define MODEM_APN               "cmiot"
#define MODEM_APN_USER          ""
#define MODEM_APN_PASS          ""

/* ---------- Power Management ---------- */
#define BATTERY_ADC_GPIO        36   /* ADC1_CH0, voltage divider input */
#define BATTERY_DIVIDER_RATIO   2.0f /* R1=R2=100k, so Vbat = ADC * 2 */
#define BATTERY_FULL_MV         4200 /* 18650 full charge voltage */
#define BATTERY_EMPTY_MV        3200 /* 18650 cutoff voltage */
#define DEEP_SLEEP_WAKEUP_S     SENSOR_SCAN_INTERVAL_S

/* ---------- Calibration ---------- */
#define CALIB_NVS_NAMESPACE     "solar-ball"
#define CALIB_BASELINE_KEY      "baseline"
#define CALIB_MAPPING_KEY       "ch_map"
#define CALIB_SAMPLE_COUNT      100  /* samples per sensor during calibration */

/* ---------- NVS (Non-Volatile Storage) ---------- */
#define NVS_PARTITION           "nvs"

#endif /* CONFIG_H */
