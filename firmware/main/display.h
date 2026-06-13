#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/*
 * Low-power I2C display driver (SSD1306 OLED, 128x64).
 * Shares I2C1 bus with ADS1115 #1 (different address: 0x3C vs 0x48).
 * Power: <1µA when off, ~5mA during refresh (~100ms per update).
 *
 * Pin mapping (uses existing I2C1):
 *   SDA = GPIO 21 (shared with ADS1115)
 *   SCL = GPIO 22 (shared with ADS1115)
 *   I2C address = 0x3C
 */

void display_init(void);

/* Show main status screen: direction vector, SOC, signal, faults */
void display_show_status(float dx, float dy, float dz,
                         uint8_t soc, int16_t rssi,
                         uint16_t error_mask, uint8_t confidence);

/* Show calibration progress */
void display_show_calib(const char *status, int progress, int total);

/* Clear and power down display between updates */
void display_sleep(void);

/* Wake up for next update */
void display_wake(void);

#endif /* DISPLAY_H */
