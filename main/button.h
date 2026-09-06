/*
 * button.h
 * 4개 버튼을 GPIO 하드웨어 인터럽트로 처리하는 모듈
 * 대상 보드: Seeed Studio XIAO ESP32-S3
 */
#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define BTN_COUNT 4

typedef enum {
    BTN_1 = 0,  // D0 - GPIO1
    BTN_2,      // D4 - GPIO5
    BTN_3,      // D5 - GPIO6
    BTN_4,      // D9 - GPIO8
} button_id_t;

/*
 * 버튼 GPIO를 인터럽트 입력으로 설정하고, 눌림 이벤트를 받을
 * 큐를 생성해서 반환합니다. (큐의 각 아이템 타입: button_id_t)
 */
QueueHandle_t button_init(void);
