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

#define DISPLAY_BRIGHTNESS 0x03           // Panel brightness: SSD1351 master contrast register, 0x00-0x0F.
                                          // Drive current = (value+1)/16 of the maximum:
                                          // 0x0F = 100%, 0x07 = 50%, 0x03 = 25%, 0x01 = 12.5%, 0x00 = 6%.
                                          // Lower saves battery.

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
#define BATTERY_UPDATE_INTERVAL_US (10 * 1000000LL) // How often the battery icon is refreshed (10s, real time)

// Discharge curve of a typical 1S LiPo: { voltage in V, charge in % }.
// Rows must be sorted from the highest voltage to the lowest. The charge level is
// linearly interpolated between two rows, so more rows near steep parts = more accuracy.
// 4.11V is treated as 100% (not 4.20V): charging to the very top wears the cell faster.
// The curve is flat between ~4.1V and ~3.7V and drops quickly below that, which is why
// a straight linear mapping looks wrong. Tune these rows to your own battery.
#define BATTERY_CURVE_TABLE \
    { 4.11f, 100 }, \
    { 4.02f,  88 }, \
    { 3.95f,  76 }, \
    { 3.87f,  64 }, \
    { 3.84f,  52 }, \
    { 3.80f,  40 }, \
    { 3.77f,  30 }, \
    { 3.73f,  20 }, \
    { 3.69f,  10 }, \
    { 3.61f,   5 }, \
    { 3.30f,   0 }

/* ------------------------------------------------------------------ */
/* Timing                                                              */
/* ------------------------------------------------------------------ */

#define DISPLAY_REFRESH_HZ   8            // Target display refresh rate (gptimer alarm frequency).
                                          // Lower = less power. Pick a divisor of 1,000,000 (e.g. 8, 10, 50, 64)
                                          // so the timer period in microseconds is exact.
#define DISPLAY_FALLBACK_HZ  30           // Rate used if the target rate cannot be set up
#define GPTIMER_RESOLUTION_HZ 1000000     // gptimer counter clock: 1MHz -> 1 tick = 1us

/* ------------------------------------------------------------------ */
/* Persistent storage (NVS flash, survives power loss)                 */
/* ------------------------------------------------------------------ */

#define STOPWATCH_NVS_NAMESPACE   "stopwatch" // NVS namespace that holds the saved elapsed time
#define STOPWATCH_NVS_KEY         "elapsed"   // NVS key of the saved elapsed time (int64, microseconds)
#define STOPWATCH_SAVE_INTERVAL_US (10 * 1000000LL) // While running, save the elapsed time this often (10s).
                                                    // On power loss at most this much time is lost.
                                                    // NVS wear leveling makes this safe for years of use.

/* ------------------------------------------------------------------ */
/* FreeRTOS tasks                                                      */
/* ------------------------------------------------------------------ */

#define DISPLAY_TASK_STACK    6144        // Stack size in bytes (also runs periodic NVS saves)
#define DISPLAY_TASK_PRIORITY 5
#define BUTTON_TASK_STACK     4096        // Stack size in bytes (NVS saves on pause / reset)
#define BUTTON_TASK_PRIORITY  6           // Higher than the display task so button presses are handled promptly

/* ------------------------------------------------------------------ */
/* Screen layout: time "HH:MM:SS" = 8 cells (2 digits, colon, 2 digits, colon, 2 digits) */
/* ------------------------------------------------------------------ */

#define DIGIT_W        14                 // Width of one digit in pixels
#define DIGIT_H        40                 // Height of one digit in pixels
#define DIGIT_TH       3                  // Segment thickness of a digit in pixels
#define COLON_DOT_SIZE 3                  // Side length of one colon dot in pixels
#define COLON_W        (COLON_DOT_SIZE * 2) // Width of the colon cell in pixels
#define DIGIT_GAP      3                  // Space between neighbouring cells in pixels
#define TIME_AREA_Y    ((SSD1351_HEIGHT - DIGIT_H) / 2) // Top y of the time area -> vertically centered
#define TIME_HOURS_MOD 100                // Hours wrap around after this value (99:59:59 -> 00:00:00)

// Battery icon (top-left): outlined body + small terminal nub on the right,
// with BATT_SEG_COUNT filled segments inside showing the charge level.
#define BATT_ICON_X       2               // Left edge of the icon in pixels
#define BATT_ICON_Y       2               // Top edge of the icon in pixels
#define BATT_ICON_BORDER  1               // Outline thickness in pixels
#define BATT_ICON_PAD     1               // Space between the outline and the segments in pixels
#define BATT_SEG_COUNT    4               // Number of level segments (4 steps)
#define BATT_SEG_W        5               // Width of one segment in pixels
#define BATT_SEG_H        6               // Height of one segment in pixels (same as the old 6px bar)
#define BATT_SEG_GAP      1               // Space between neighbouring segments in pixels
#define BATT_NUB_W        2               // Width of the terminal nub on the right in pixels
#define BATT_NUB_H        4               // Height of the terminal nub in pixels
#define BATT_LEVEL_STEP_PCT (100 / BATT_SEG_COUNT) // Charge per segment: 1-25% -> 1 segment ... 76-100% -> 4, 0% -> none
// Derived icon size (do not edit)
#define BATT_ICON_BODY_W (2 * BATT_ICON_BORDER + 2 * BATT_ICON_PAD + BATT_SEG_COUNT * BATT_SEG_W + (BATT_SEG_COUNT - 1) * BATT_SEG_GAP)
#define BATT_ICON_BODY_H (2 * BATT_ICON_BORDER + 2 * BATT_ICON_PAD + BATT_SEG_H)

#define RUN_BAR_Y0     (SSD1351_HEIGHT - 6) // Bottom running bar: first row
#define RUN_BAR_Y1     (SSD1351_HEIGHT - 1) // Bottom running bar: last row
