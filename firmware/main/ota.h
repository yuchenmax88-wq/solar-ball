#ifndef OTA_H
#define OTA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define OTA_IMAGE_MAX_SIZE       (3 * 1024 * 1024)  /* 3 MB max firmware */
#define OTA_CHUNK_SIZE           4096
#define OTA_URL_MAX_LEN          256
#define OTA_FW_VERSION_MAX_LEN   32

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_READY,
    OTA_STATE_FAILED
} ota_state_t;

typedef struct {
    ota_state_t state;
    uint32_t    total_bytes;
    uint32_t    written_bytes;
    char        new_version[OTA_FW_VERSION_MAX_LEN];
    int         progress_pct;
    int         last_error;
} ota_status_t;

void ota_init(void);

const ota_status_t *ota_get_status(void);

int ota_begin(const char *new_version, uint32_t total_size);

int ota_write_chunk(const uint8_t *data, size_t len, uint32_t offset);

int ota_finish(void);

void ota_abort(void);

bool ota_is_ota_partition_valid(void);

void ota_mark_valid_and_reboot(void);

int ota_handle_serial_protocol(void);

#endif /* OTA_H */
