#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/*
 * Sharp Memory LCD driver (LS013B7DH03, 128x128 MIP reflective).
 *
 * MIP = Memory-In-Pixel: each pixel holds its state as a tiny capacitor.
 * Zero power to maintain image. Sunlight-readable. No backlight needed.
 *
 * Interface: 3-wire SPI (software bit-bang)
 *   MOSI = GPIO 2  (was unused MODEM_RESET)
 *   SCLK = GPIO 4  (free)
 *   CS   = GPIO 5  (free)
 *
 * No conflicts with MUX, I2C, 4G, or any existing peripheral.
 * Image persists during ESP32 deep sleep.
 */

void display_init(void);

/* Show main status screen. Image stays after ESP32 sleeps. */
void display_show_status(float dx, float dy, float dz,
                         uint8_t soc, int16_t rssi,
                         uint16_t error_mask, uint8_t confidence);

/* Show calibration progress */
void display_show_calib(const char *status, int progress, int total);

/* Clear screen (all white) */
void display_clear(void);

#endif /* DISPLAY_H */
