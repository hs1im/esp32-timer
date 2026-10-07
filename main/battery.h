/*
 * battery.h
 * Battery voltage / charge level measurement through a resistor divider (ADC oneshot)
 * Wiring: BAT+ -> 100k -> [ADC pin] -> 100k -> GND  (2:1 divider)
 */
#pragma once

#include <stdint.h>
#include "constants.h" // BATTERY_* constants

void battery_init(void);

/* Returns the actual battery voltage in V (divider ratio 2.0 compensated). */
float battery_read_voltage(void);

/* Returns an approximate charge level in the 0-100 range (1S LiPo). */
int battery_read_percent(void);
