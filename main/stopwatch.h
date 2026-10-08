/*
 * stopwatch.h
 * Precise time measurement based on esp_timer + periodic display refresh trigger based on gptimer
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

/* Sets up the hardware timer (gptimer) and returns the semaphore used as the
   display refresh signal.
   target_hz: 60 means 60Hz; falls back to 30Hz automatically on failure. */
SemaphoreHandle_t stopwatch_init(int target_hz);

/* BTN1 action: STOPPED/PAUSED -> RUNNING, RUNNING -> PAUSED */
void stopwatch_toggle(void);

/* BTN2 action: resets to 0 only in the STOPPED/PAUSED state.
   Ignored while RUNNING (pause first). */
void stopwatch_reset(void);

/* While RUNNING, saves the elapsed time to flash (NVS) once per
   STOPWATCH_SAVE_INTERVAL_US. Call this regularly from the display loop. */
void stopwatch_save_if_due(void);

/* Returns the elapsed time in ms (computed live while RUNNING) */
int64_t stopwatch_get_elapsed_ms(void);

sw_state_t stopwatch_get_state(void);
