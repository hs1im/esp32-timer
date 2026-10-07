/*
 * button.h
 * Module that handles 3 buttons with GPIO hardware interrupts
 * Target board: Seeed Studio XIAO ESP32-S3
 */
#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define BTN_COUNT 3

typedef enum {
    BTN_1 = 0,  // D0 - GPIO1
    BTN_2,      // D4 - GPIO5
    BTN_3,      // D5 - GPIO6
} button_id_t;

/*
 * Configures the button GPIOs as interrupt inputs and creates and returns
 * the queue that receives press events. (queue item type: button_id_t)
 */
QueueHandle_t button_init(void);
