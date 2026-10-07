/*
 * constants.h
 * Central place for every tunable constant of the project.
 * Change values here instead of searching through the source files.
 * Target board: Seeed Studio XIAO ESP32-S3
 */
#pragma once

#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"

/* ------------------------------------------------------------------ */
/* OLED display (SSD1351, SPI)                                         */
/* ------------------------------------------------------------------ */

#define SSD1351_WIDTH     128             // Panel width in pixels
#define SSD1351_HEIGHT    128             // Panel height in pixels

#define PIN_OLED_SCK      7               // SPI clock  (XIAO D8)
#define PIN_OLED_MOSI     9               // SPI data out (XIAO D10)
#define PIN_OLED_CS       4               // SPI chip select (XIAO D3)
#define PIN_OLED_DC       2               // Data/command select (XIAO D1)
#define PIN_OLED_RST      3               // Hardware reset (XIAO D2)

#define OLED_SPI_HOST     SPI2_HOST       // SPI peripheral used by the display
#define OLED_SPI_CLOCK_HZ (8 * 1000 * 1000) // SPI clock; 20MHz showed noise stripes with jumper wires

/* ------------------------------------------------------------------ */
/* Buttons (GPIO interrupt, internal pull-up, active low)              */
/* ------------------------------------------------------------------ */

#define PIN_BTN_1         1               // Start / pause (XIAO D0)
#define PIN_BTN_2         5               // Reset, only while paused (XIAO D4)
#define PIN_BTN_3         6               // Unassigned (XIAO D5)

#define BUTTON_DEBOUNCE_US 200000         // Ignore presses closer than this to the previous one (200ms)
#define BUTTON_QUEUE_LEN   10             // Max pending button events

/* ------------------------------------------------------------------ */
/* Battery measurement (ADC through a 100k + 100k divider)             */
/* ------------------------------------------------------------------ */

#define BATTERY_ADC_GPIO          8                // XIAO D9 (the former BTN_4 pin)
#define BATTERY_ADC_CHANNEL       ADC_CHANNEL_7    // ADC1_CH7 = GPIO8 on ESP32-S3
#define BATTERY_DIVIDER_RATIO     2.0f             // 100k:100k divider -> real voltage is 2x the pin voltage
#define BATTERY_FULL_V            4.2f             // Voltage treated as 100% (1S LiPo)
#define BATTERY_EMPTY_V           3.0f             // Voltage treated as 0% (1S LiPo)
#define BATTERY_UPDATE_INTERVAL_US (10 * 1000000LL) // How often the battery bar is refreshed (10s, real time)

/* ------------------------------------------------------------------ */
/* Timing                                                              */
/* ------------------------------------------------------------------ */

#define DISPLAY_REFRESH_HZ   60           // Target display refresh rate (gptimer alarm frequency)
#define DISPLAY_FALLBACK_HZ  30           // Rate used if the target rate cannot be set up
#define GPTIMER_RESOLUTION_HZ 1000000     // gptimer counter clock: 1MHz -> 1 tick = 1us

/* ------------------------------------------------------------------ */
/* FreeRTOS tasks                                                      */
/* ------------------------------------------------------------------ */

#define DISPLAY_TASK_STACK    4096        // Stack size in bytes
#define DISPLAY_TASK_PRIORITY 5
#define BUTTON_TASK_STACK     2048        // Stack size in bytes
#define BUTTON_TASK_PRIORITY  6           // Higher than the display task so button presses are handled promptly

/* ------------------------------------------------------------------ */
/* Screen layout: time "MM:SS" = 5 cells (digit, digit, colon, digit, digit) */
/* ------------------------------------------------------------------ */

#define DIGIT_W        22                 // Width of one digit in pixels
#define DIGIT_H        50                 // Height of one digit in pixels
#define DIGIT_TH       6                  // Segment thickness of a digit in pixels
#define COLON_DOT_SIZE 6                  // Side length of one colon dot in pixels
#define COLON_W        14                 // Width of the colon cell in pixels
#define DIGIT_GAP      4                  // Space between neighbouring cells in pixels
#define TIME_AREA_Y    39                 // Top y of the time area, about (128-50)/2 -> vertically centered

#define BATT_BAR_Y0    0                  // Top battery bar: first row
#define BATT_BAR_Y1    5                  // Top battery bar: last row

#define RUN_BAR_Y0     (SSD1351_HEIGHT - 6) // Bottom running bar: first row
#define RUN_BAR_Y1     (SSD1351_HEIGHT - 1) // Bottom running bar: last row
