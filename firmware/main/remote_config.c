#include "remote_config.h"
#include "config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "remote_cfg";

static const char *VALID_KEYS[] = {
    "ball_id",
    "mqtt_broker",
    "mqtt_port",
    "apn",
    "apn_user",
    "apn_pass",
    "scan_interval_s",
    "bat_full_mv",
    "bat_empty_mv",
    NULL
};

static bool is_valid_key(const char *key) {
    for (int i = 0; VALID_KEYS[i] != NULL; i++) {
        if (strcmp(key, VALID_KEYS[i]) == 0) return true;
    }
    return false;
}

int remote_config_init(void) {
    ESP_LOGI(TAG, "Remote config initialized");
    return 0;
}

remote_cfg_result_t remote_config_set(const char *key, const char *value) {
    if (!key || !value || strlen(key) == 0 || strlen(value) == 0) {
        return CFG_ERR_VALUE_INVALID;
    }

    if (!is_valid_key(key)) {
        ESP_LOGW(TAG, "Unknown config key: %s", key);
        return CFG_ERR_KEY_UNKNOWN;
    }

    if (strcmp(key, "scan_interval_s") == 0) {
        int v = atoi(value);
        if (v < 1 || v > 3600) return CFG_ERR_VALUE_INVALID;
    }
    if (strcmp(key, "mqtt_port") == 0) {
        int v = atoi(value);
        if (v < 1 || v > 65535) return CFG_ERR_VALUE_INVALID;
    }
    if (strcmp(key, "bat_full_mv") == 0 || strcmp(key, "bat_empty_mv") == 0) {
        int v = atoi(value);
        if (v < 1000 || v > 5000) return CFG_ERR_VALUE_INVALID;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed");
        return CFG_ERR_NVS_WRITE;
    }

    char nvs_key[REMOTE_CFG_KEY_MAX_LEN + 8];
    snprintf(nvs_key, sizeof(nvs_key), "rcfg_%s", key);
    err = nvs_set_str(nvs, nvs_key, value);
    if (err != ESP_OK) {
        nvs_close(nvs);
        return CFG_ERR_NVS_WRITE;
    }

    nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Config set: %s = %s", key, value);
    return CFG_OK;
}

remote_cfg_result_t remote_config_get(const char *key, char *value, size_t max_len) {
    if (!key || !value || max_len == 0) {
        return CFG_ERR_VALUE_INVALID;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CALIB_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return CFG_ERR_NVS_WRITE;
    }

    char nvs_key[REMOTE_CFG_KEY_MAX_LEN + 8];
    snprintf(nvs_key, sizeof(nvs_key), "rcfg_%s", key);
    size_t len = max_len;
    err = nvs_get_str(nvs, nvs_key, value, &len);
    nvs_close(nvs);

    if (err != ESP_OK) {
        value[0] = '\0';
        return CFG_ERR_KEY_UNKNOWN;
    }

    return CFG_OK;
}

int remote_config_handle_mqtt(const char *json_payload, char *response, size_t resp_len) {
    if (!json_payload || !response || resp_len == 0) return -1;

    char *cmd_ptr = strstr(json_payload, "\"cmd\":\"config\"");
    if (!cmd_ptr) return -1;

    char *key_ptr = strstr(json_payload, "\"key\":\"");
    char *val_ptr = strstr(json_payload, "\"value\":\"");

    if (key_ptr && val_ptr) {
        char key[REMOTE_CFG_KEY_MAX_LEN] = {0};
        char value[REMOTE_CFG_VALUE_MAX_LEN] = {0};

        key_ptr += 7;
        char *key_end = strchr(key_ptr, '"');
        if (key_end) {
            size_t klen = key_end - key_ptr;
            if (klen < sizeof(key)) {
                memcpy(key, key_ptr, klen);
                key[klen] = '\0';
            }
        }

        val_ptr += 9;
        char *val_end = strchr(val_ptr, '"');
        if (val_end) {
            size_t vlen = val_end - val_ptr;
            if (vlen < sizeof(value)) {
                memcpy(value, val_ptr, vlen);
                value[vlen] = '\0';
            }
        }

        if (key[0] && value[0]) {
            remote_cfg_result_t r = remote_config_set(key, value);
            if (r == CFG_OK) {
                bool needs_reboot = (
                    strcmp(key, "scan_interval_s") == 0 ||
                    strcmp(key, "bat_full_mv") == 0 ||
                    strcmp(key, "bat_empty_mv") == 0
                );
                snprintf(response, resp_len,
                    "{\"ok\":true,\"key\":\"%s\",\"value\":\"%s\",\"reboot\":%s}",
                    key, value, needs_reboot ? "true" : "false");
                return 0;
            } else {
                snprintf(response, resp_len,
                    "{\"ok\":false,\"error\":%d,\"key\":\"%s\"}",
                    (int)r, key);
                return -1;
            }
        }
    }

    char *get_ptr = strstr(json_payload, "\"get\":\"");
    if (get_ptr) {
        char key_buf[REMOTE_CFG_KEY_MAX_LEN] = {0};
        get_ptr += 7;
        char *ge = strchr(get_ptr, '"');
        if (ge) {
            size_t gl = ge - get_ptr;
            if (gl < sizeof(key_buf)) {
                memcpy(key_buf, get_ptr, gl);
                key_buf[gl] = '\0';
            }
        }

        char val_buf[REMOTE_CFG_VALUE_MAX_LEN];
        remote_cfg_result_t r = remote_config_get(key_buf, val_buf, sizeof(val_buf));
        snprintf(response, resp_len,
            "{\"ok\":true,\"key\":\"%s\",\"value\":\"%s\"}",
            key_buf, (r == CFG_OK) ? val_buf : "(unset)");
        return 0;
    }

    snprintf(response, resp_len, "{\"ok\":false,\"error\":\"invalid_cmd\"}");
    return -1;
}

void remote_config_get_all(char *json_buf, size_t buf_len) {
    if (buf_len < 4) return;
    int offset = 0;
    offset += snprintf(json_buf + offset, buf_len - offset, "{");

    for (int i = 0; VALID_KEYS[i] != NULL; i++) {
        char val[REMOTE_CFG_VALUE_MAX_LEN];
        remote_cfg_result_t r = remote_config_get(VALID_KEYS[i], val, sizeof(val));

        if (offset > 1) {
            int w = snprintf(json_buf + offset, buf_len - offset, ",");
            if (w < 0) break;
            offset += w;
            if (offset >= (int)buf_len) break;
        }

        if (r == CFG_OK) {
            int w = snprintf(json_buf + offset, buf_len - offset,
                "\"%s\":\"%s\"", VALID_KEYS[i], val);
            if (w < 0) break;
            offset += w;
            if (offset >= (int)buf_len) break;
        } else {
            int w = snprintf(json_buf + offset, buf_len - offset,
                "\"%s\":null", VALID_KEYS[i]);
            if (w < 0) break;
            offset += w;
            if (offset >= (int)buf_len) break;
        }
    }

    snprintf(json_buf + offset, buf_len - offset, "}");
}
