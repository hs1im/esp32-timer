/*
 * stopwatch.h
 * esp_timer 기반 정밀 시간 측정 + gptimer 기반 주기적 화면 갱신 트리거
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum {
    SW_STOPPED,
    SW_RUNNING,
    SW_PAUSED,
} sw_state_t;

/* 하드웨어 타이머(gptimer)를 설정하고, 화면 갱신 신호용 세마포어를 반환합니다.
   target_hz: 60을 넣으면 60Hz, 실패 시 자동으로 30Hz로 낮춥니다. */
SemaphoreHandle_t stopwatch_init(int target_hz);

/* BTN1 동작: STOPPED/PAUSED -> RUNNING, RUNNING -> PAUSED */
void stopwatch_toggle(void);

/* BTN2 동작: 정지 후 0으로 초기화 */
void stopwatch_reset(void);

/* 현재 경과 시간을 ms 단위로 반환 (RUNNING 중이면 실시간 계산) */
int64_t stopwatch_get_elapsed_ms(void);

sw_state_t stopwatch_get_state(void);
