#ifndef REMOTE_CONFIG_H
#define REMOTE_CONFIG_H

#include <stdint.h>
#include "config.h"
#include "mqtt_protocol.h"

#define REMOTE_CFG_KEY_MAX_LEN    32
#define REMOTE_CFG_VALUE_MAX_LEN  64

typedef enum {
    CFG_OK = 0,
    CFG_ERR_KEY_UNKNOWN = -1,
    CFG_ERR_VALUE_INVALID = -2,
    CFG_ERR_NVS_WRITE = -3,
} remote_cfg_result_t;

int remote_config_init(void);

remote_cfg_result_t remote_config_set(const char *key, const char *value);

remote_cfg_result_t remote_config_get(const char *key, char *value, size_t max_len);

int remote_config_handle_mqtt(const char *json_payload, char *response, size_t resp_len);

void remote_config_get_all(char *json_buf, size_t buf_len);

#endif /* REMOTE_CONFIG_H */
