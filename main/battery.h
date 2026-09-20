/*
 * battery.h
 * 저항 분배기를 통한 배터리 전압/잔량 측정 (ADC oneshot 기반)
 * 배선: BAT+ -> 100k -> [ADC 핀] -> 100k -> GND  (2:1 분배)
 */
#pragma once

#include <stdint.h>

#define BATTERY_ADC_GPIO 8   // D9, BTN_4 자리를 재사용

void battery_init(void);

/* 배터리 실제 전압(V)을 반환합니다. (분배비 2.0 보정 적용) */
float battery_read_voltage(void);

/* 0~100 사이의 대략적인 잔량 퍼센트를 반환합니다. (1S 리튬폴리머 기준) */
int battery_read_percent(void);
