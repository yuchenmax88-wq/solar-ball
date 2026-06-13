#include "remote_diag.h"

static uint32_t s_scan_count = 0;
static uint32_t s_publish_ok = 0;
static uint32_t s_publish_fail = 0;

void remote_diag_init(void) {
    s_scan_count = 0;
    s_publish_ok = 0;
    s_publish_fail = 0;
}

void remote_diag_record_publish(uint8_t success) {
    if (success) {
        s_publish_ok++;
    } else {
        s_publish_fail++;
    }
}

void remote_diag_record_scan(void) {
    s_scan_count++;
}
