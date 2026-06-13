#include "mqtt_4g.h"
#include "mqtt_protocol.h"
#include "config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "mqtt_4g";

#define UART_NUM                UART_NUM_2
#define BUF_SIZE                1024
#define AT_TIMEOUT_MS           5000
#define NET_TIMEOUT_MS          30000

/* AT command response buffer */
static char resp_buf[BUF_SIZE];

static void uart_send(const char *cmd) {
    uart_write_bytes(UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(UART_NUM, "\r\n", 2);
}

static int uart_wait_response(const char *expect, uint32_t timeout_ms) {
    int len = 0;
    uint32_t elapsed = 0;

    memset(resp_buf, 0, sizeof(resp_buf));

    while (elapsed < timeout_ms) {
        int avail = 0;
        uart_get_buffered_data_len(UART_NUM, (size_t *)&avail);
        if (avail > 0) {
            int read_len = uart_read_bytes(UART_NUM,
                (uint8_t *)resp_buf + len,
                sizeof(resp_buf) - len - 1,
                pdMS_TO_TICKS(100));
            if (read_len > 0) {
                len += read_len;
                resp_buf[len] = '\0';
                if (strstr(resp_buf, expect) != NULL) {
                    return 0;  /* found expected response */
                }
                if (strstr(resp_buf, "ERROR") != NULL) {
                    ESP_LOGW(TAG, "AT command error: %s", resp_buf);
                    return -1;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        elapsed += 100;
    }

    ESP_LOGW(TAG, "Timeout waiting for '%s', last response: %s", expect, resp_buf);
    return -1;
}

static int at_send_cmd(const char *cmd, const char *expect, uint32_t timeout_ms) {
    uart_send(cmd);
    return uart_wait_response(expect, timeout_ms);
}

int modem_init(void) {
    ESP_LOGI(TAG, "Initializing SIM7600G 4G module...");

    /* Configure UART */
    uart_config_t uart_config = {
        .baud_rate = MODEM_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, MODEM_UART_TX_GPIO, MODEM_UART_RX_GPIO,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE, 0, 0, NULL, 0);

    /* Power on the module via PWR_KEY pulse */
    gpio_set_direction(MODEM_PWR_KEY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(MODEM_PWR_KEY_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(MODEM_PWR_KEY_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_set_level(MODEM_PWR_KEY_GPIO, 0);

    /* Wait for module to boot */
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* Flush any boot messages */
    uart_flush(UART_NUM);
    vTaskDelay(pdMS_TO_TICKS(500));
    uart_flush(UART_NUM);

    /* Check communication with module */
    if (at_send_cmd("AT", "OK", 2000) != 0) {
        ESP_LOGE(TAG, "SIM7600G not responding");
        return -1;
    }

    /* Wait for network registration */
    ESP_LOGI(TAG, "Waiting for network registration...");
    int registered = 0;
    for (int retry = 0; retry < 10; retry++) {
        uart_send("AT+CREG?");
        if (uart_wait_response("+CREG:", 3000) == 0) {
            if (strstr(resp_buf, "+CREG: 0,1") != NULL) {
                ESP_LOGI(TAG, "Network registered (home network)");
                registered = 1;
                break;
            }
            if (strstr(resp_buf, "+CREG: 0,5") != NULL) {
                ESP_LOGI(TAG, "Network registered (roaming)");
                registered = 1;
                break;
            }
        }
        ESP_LOGI(TAG, "Waiting for network... attempt %d/10", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    if (!registered) {
        ESP_LOGW(TAG, "Network registration timeout, continuing anyway");
    }

    /* Attach to GPRS */
    if (at_send_cmd("AT+CGATT=1", "OK", 5000) != 0) {
        ESP_LOGE(TAG, "GPRS attach failed");
        return -1;
    }

    /* Set APN and connect */
    char apn_cmd[64];
    snprintf(apn_cmd, sizeof(apn_cmd), "AT+CSTT=\"%s\",\"%s\",\"%s\"",
             MODEM_APN, MODEM_APN_USER, MODEM_APN_PASS);
    if (at_send_cmd(apn_cmd, "OK", 5000) != 0) return -1;

    if (at_send_cmd("AT+CIICR", "OK", 15000) != 0) {
        ESP_LOGE(TAG, "Network connection failed");
        return -1;
    }

    /* Get local IP */
    ESP_LOGI(TAG, "Getting IP address...");
    uart_send("AT+CIFSR");
    uart_wait_response(".", 5000);
    ESP_LOGI(TAG, "IP obtained: %s", resp_buf);

    /* Start MQTT */
    if (at_send_cmd("AT+CMQTTSTART", "OK", 5000) != 0) {
        ESP_LOGE(TAG, "MQTT start failed");
        return -1;
    }

    /* Connect to MQTT broker */
    char mqtt_conn[128];
    snprintf(mqtt_conn, sizeof(mqtt_conn),
             "AT+CMQTTACC=0,\"%s\",%d,0",
             MQTT_CLIENT_ID, MQTT_KEEPALIVE_S);
    if (at_send_cmd(mqtt_conn, "OK", 5000) != 0) return -1;

    char mqtt_connect[128];
    snprintf(mqtt_connect, sizeof(mqtt_connect),
             "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",%d,1",
             MQTT_BROKER_HOST, MQTT_BROKER_PORT, MQTT_KEEPALIVE_S);
    if (at_send_cmd(mqtt_connect, "OK", 15000) != 0) {
        ESP_LOGE(TAG, "MQTT broker connection failed");
        return -1;
    }

    ESP_LOGI(TAG, "4G module initialized, MQTT connected");
    return 0;
}

int16_t modem_get_rssi(void) {
    uart_send("AT+CSQ");
    if (uart_wait_response("+CSQ:", 3000) == 0) {
        int rssi_raw;
        if (sscanf(resp_buf, "+CSQ: %d,", &rssi_raw) == 1) {
            if (rssi_raw == 99) return -999;
            return (int16_t)(-113 + rssi_raw * 2);
        }
    }
    return -999;
}

int modem_sync_time(void) {
    /* Sync via NTP using AT+CNTP */
    if (at_send_cmd("AT+CNTP=\"pool.ntp.org\",0,0", "OK", 5000) != 0) return -1;
    if (uart_wait_response("+CNTP:", 10000) == 0) {
        if (strstr(resp_buf, "+CNTP: 1") != NULL) {
            ESP_LOGI(TAG, "NTP time synchronized");
            return 0;
        }
    }
    ESP_LOGW(TAG, "NTP sync failed, continuing with local time");
    return -1;
}

int mqtt_publish_direction(const direction_packet_t *pkt) {
    char topic[MQTT_TOPIC_MAX_LEN];
    char payload[MQTT_PAYLOAD_MAX_LEN];

    build_mqtt_topic(pkt->id, topic, sizeof(topic));
    int payload_len = serialize_direction(pkt, payload, sizeof(payload));

    /* Set topic */
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%d", (int)strlen(topic));
    if (at_send_cmd(cmd, ">", 2000) != 0) return -1;

    uart_send(topic);
    if (uart_wait_response("OK", 3000) != 0) return -1;

    /* Set payload */
    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%d", payload_len);
    if (at_send_cmd(cmd, ">", 2000) != 0) return -1;

    uart_send(payload);
    if (uart_wait_response("OK", 3000) != 0) return -1;

    /* Publish */
    if (at_send_cmd("AT+CMQTTPUB=0,1,60", "OK", 5000) != 0) {
        ESP_LOGW(TAG, "MQTT publish failed");
        return -1;
    }

    ESP_LOGI(TAG, "Published to %s: %s", topic, payload);
    return payload_len;
}

int modem_get_location(int32_t *lat_e6, int32_t *lon_e6) {
    /* AT+CIPGSMLOC=1,1: get cell tower location (network-based positioning)
     * Response: +CIPGSMLOC: 0,<result>,<latitude>,<longitude>,<datetime>
     * Latitude/longitude are in degrees * 1e6, as signed integers.
     */
    uart_send("AT+CIPGSMLOC=1,1");
    if (uart_wait_response("+CIPGSMLOC:", 15000) == 0) {
        long lat_raw = 0, lon_raw = 0;
        if (sscanf(resp_buf, "+CIPGSMLOC: 0,%*d,%ld,%ld", &lat_raw, &lon_raw) >= 2) {
            *lat_e6 = (int32_t)lat_raw;
            *lon_e6 = (int32_t)lon_raw;
            ESP_LOGI(TAG, "Cell location: lat=%.4f lon=%.4f",
                     *lat_e6 / 1000000.0, *lon_e6 / 1000000.0);
            return 0;
        }
    }
    ESP_LOGW(TAG, "Cell location query failed");
    return -1;
}

int modem_get_unix_time(uint32_t *unix_ts) {
    /* AT+CCLK? returns current time: +CCLK: "yy/MM/dd,HH:mm:ss±TZ"
     * After NTP sync, this reflects the synced time in LOCAL time.
     * TZ is the timezone offset (e.g. +08 for China, -05 for EST).
     * We must subtract TZ to get UTC before computing Unix timestamp.
     */
    uart_send("AT+CCLK?");
    if (uart_wait_response("+CCLK:", 3000) == 0) {
        int yy, MM, dd, hh, mm, ss;
        int tz_hours = 0;

        /* Parse date/time part first */
        if (sscanf(resp_buf, "+CCLK: \"%d/%d/%d,%d:%d:%d", &yy, &MM, &dd, &hh, &mm, &ss) < 6) {
            ESP_LOGW(TAG, "CCLK time parse failed");
            return -1;
        }

        /* Find the timezone offset: it's the LAST + or - in the response
         * (strchr finds the +CCLK: prefix first, which is wrong). */
        char *tz_ptr = NULL;
        char *last_plus = strrchr(resp_buf, '+');
        char *last_minus = strrchr(resp_buf, '-');
        if (last_plus && last_minus) {
            tz_ptr = (last_plus > last_minus) ? last_plus : last_minus;
        } else if (last_plus) {
            tz_ptr = last_plus;
        } else if (last_minus) {
            tz_ptr = last_minus;
        }
        if (tz_ptr) {
            tz_hours = atoi(tz_ptr);
        } else {
            tz_hours = 8;  /* fallback: assume China UTC+8 */
        }

        /* Convert LOCAL time to UTC by subtracting the offset */
        hh -= tz_hours;
        if (hh < 0) { hh += 24; dd -= 1; }
        if (hh >= 24) { hh -= 24; dd += 1; }

        /* Convert to Unix timestamp (simple algorithm for 2000-2099) */
        int year = 2000 + yy;
        int days = (year - 1970) * 365;
        days += (year - 1969) / 4;  /* leap years */
        static const int month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        days += month_days[MM - 1] + dd - 1;
        if (MM > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
            days += 1;
        }
        *unix_ts = (uint32_t)(days * 86400 + hh * 3600 + mm * 60 + ss);
        ESP_LOGI(TAG, "Unix time: %lu (tz=%+d)", (unsigned long)*unix_ts, tz_hours);
        return 0;
    }
    ESP_LOGW(TAG, "CCLK time query failed");
    return -1;
}

void modem_power_off(void) {
    ESP_LOGI(TAG, "Powering off 4G module...");

    /* Graceful shutdown */
    at_send_cmd("AT+CPOWD=1", "OK", 5000);

    /* UART cleanup */
    uart_driver_delete(UART_NUM);

    ESP_LOGI(TAG, "4G module powered off");
}
