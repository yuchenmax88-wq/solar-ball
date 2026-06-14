#include "ota.h"
#include "config.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "ota";

static ota_status_t ota_status;
static esp_ota_handle_t ota_handle;
static const esp_partition_t *update_partition;

void ota_init(void) {
    memset(&ota_status, 0, sizeof(ota_status));
    ota_status.state = OTA_STATE_IDLE;
    ota_status.progress_pct = 0;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        ota_status.last_error = -1;
        return;
    }

    ESP_LOGI(TAG, "OTA partition: %s, size: %lu",
             update_partition->label, (unsigned long)update_partition->size);
}

const ota_status_t *ota_get_status(void) {
    return &ota_status;
}

int ota_begin(const char *new_version, uint32_t total_size) {
    if (ota_status.state != OTA_STATE_IDLE) {
        ESP_LOGW(TAG, "OTA already in progress (state=%d)", ota_status.state);
        return -1;
    }

    if (total_size > update_partition->size) {
        ESP_LOGE(TAG, "Firmware too large: %lu > partition %lu",
                 (unsigned long)total_size, (unsigned long)update_partition->size);
        return -1;
    }

    esp_err_t err = esp_ota_begin(update_partition, total_size, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        ota_status.last_error = err;
        return -1;
    }

    ota_status.state = OTA_STATE_DOWNLOADING;
    ota_status.total_bytes = total_size;
    ota_status.written_bytes = 0;
    ota_status.progress_pct = 0;
    ota_status.last_error = 0;
    strncpy(ota_status.new_version, new_version, sizeof(ota_status.new_version) - 1);
    ota_status.new_version[sizeof(ota_status.new_version) - 1] = '\0';

    ESP_LOGI(TAG, "OTA begin: version=%s, size=%lu", new_version, (unsigned long)total_size);
    return 0;
}

int ota_write_chunk(const uint8_t *data, size_t len, uint32_t offset) {
    (void)offset;
    if (ota_status.state != OTA_STATE_DOWNLOADING) {
        ESP_LOGW(TAG, "OTA not in downloading state (state=%d)", ota_status.state);
        return -1;
    }

    esp_err_t err = esp_ota_write(ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        ota_status.state = OTA_STATE_FAILED;
        ota_status.last_error = err;
        return -1;
    }

    ota_status.written_bytes += len;
    ota_status.progress_pct = (int)(((uint64_t)ota_status.written_bytes * 100) / ota_status.total_bytes);

    if (ota_status.progress_pct % 10 == 0 && ota_status.written_bytes % (OTA_CHUNK_SIZE * 10) < OTA_CHUNK_SIZE) {
        ESP_LOGI(TAG, "OTA progress: %d%% (%lu/%lu)",
                 ota_status.progress_pct,
                 (unsigned long)ota_status.written_bytes,
                 (unsigned long)ota_status.total_bytes);
    }

    return 0;
}

int ota_finish(void) {
    if (ota_status.state != OTA_STATE_DOWNLOADING) {
        ESP_LOGW(TAG, "OTA not in downloading state (state=%d)", ota_status.state);
        return -1;
    }

    if (ota_status.written_bytes != ota_status.total_bytes) {
        ESP_LOGE(TAG, "OTA size mismatch: written %lu != expected %lu",
                 (unsigned long)ota_status.written_bytes,
                 (unsigned long)ota_status.total_bytes);
        ota_status.state = OTA_STATE_FAILED;
        ota_status.last_error = -1;
        esp_ota_abort(ota_handle);
        return -1;
    }

    ota_status.state = OTA_STATE_VERIFYING;

    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        ota_status.state = OTA_STATE_FAILED;
        ota_status.last_error = err;
        return -1;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        ota_status.state = OTA_STATE_FAILED;
        ota_status.last_error = err;
        return -1;
    }

    ota_status.state = OTA_STATE_READY;
    ota_status.progress_pct = 100;

    ESP_LOGI(TAG, "OTA complete. New firmware v%s ready, reboot to apply.", ota_status.new_version);

    nvs_handle_t nvs;
    esp_err_t nvs_err = nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (nvs_err == ESP_OK) {
        nvs_set_u8(nvs, "ota_valid", 1);
        nvs_set_str(nvs, "ota_version", ota_status.new_version);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    return 0;
}

void ota_abort(void) {
    if (ota_status.state == OTA_STATE_DOWNLOADING) {
        esp_ota_abort(ota_handle);
    }
    memset(&ota_status, 0, sizeof(ota_status));
    ota_status.state = OTA_STATE_IDLE;
    ESP_LOGI(TAG, "OTA aborted");
}

bool ota_is_ota_partition_valid(void) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *configured = esp_ota_get_boot_partition();

    if (running != configured) {
        ESP_LOGW(TAG, "Running partition differs from boot partition");
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CALIB_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        uint8_t valid = 0;
        nvs_get_u8(nvs, "ota_valid", &valid);
        nvs_close(nvs);
        if (!valid) {
            return false;
        }
    }

    return true;
}

void ota_mark_valid_and_reboot(void) {
    ESP_LOGI(TAG, "Marking OTA image as valid and rebooting...");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CALIB_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        nvs_set_u8(nvs, "ota_valid", 1);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

int ota_handle_serial_protocol(void) {
    ESP_LOGW(TAG, "Serial OTA not yet implemented — use MQTT OTA path instead.");
    return -1;
}
