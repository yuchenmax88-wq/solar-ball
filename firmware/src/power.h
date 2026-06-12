#ifndef POWER_H
#define POWER_H

#include <stdint.h>

/*
 * Initialize ADC for battery voltage monitoring.
 */
void power_init(void);

/*
 * Read battery voltage and compute state of charge.
 * Returns SOC in percent (0-100).
 */
uint8_t power_get_soc(void);

/*
 * Read battery voltage in millivolts.
 */
uint32_t power_get_millivolts(void);

/*
 * Enter deep sleep for the configured interval.
 * The RTC timer will wake the system after DEEP_SLEEP_WAKEUP_S seconds.
 * All peripheral state is lost; system restarts from main().
 */
void power_deep_sleep(void);

#endif /* POWER_H */
