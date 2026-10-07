/*
 * Wiring:
 *  OLED VCC -> 3V3     OLED GND -> GND
 *  OLED CLK -> GPIO7 (D8)   OLED MOSI -> GPIO9 (D10)
 *  OLED RES -> GPIO3 (D2)   OLED DC   -> GPIO2 (D1)
 *  OLED CS  -> GPIO4 (D3)
 *
 *  BTN_1 -> GPIO1 (D0)   BTN_2 -> GPIO5 (D4)   BTN_3 -> GPIO6 (D5)
 *  (the other leg of every button goes to GND)
 *
 *  Battery ADC -> GPIO8 (D9, the former BTN_4 pin) - via a 100k+100k voltage divider
 *
 *  Forbidden pins: GPIO43/44 (D6/D7, USB UART0), GPIO19/20 (native USB)
 */
/*
 * main.c - Stopwatch app
 * Target board: Seeed Studio XIAO ESP32-S3
 *
 *  BTN_1 (GPIO1, D0): start / pause
 *  BTN_2 (GPIO5, D4): reset (only works while paused, ignored while running)
 *  BTN_3 (GPIO6, D5): reserved for a future feature (currently ignored)
 *
 *  Display refresh: triggered at 60Hz by a hardware timer (gptimer) interrupt
 *  (falls back to 30Hz on failure), only the digit area is sent (partial update)
 *  Time measurement: based on esp_timer_get_time(), fully independent of the refresh rate
 *
 *  Top bar: battery level (refreshed once every 10 seconds, based on esp_timer)
 *  Bottom bar: shown in green only while the stopwatch is running
 */
#include "ssd1351.h"
#include "button.h"
#include "stopwatch.h"
#include "digit7seg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "battery.h"

#define PIN_SCK  7
#define PIN_MOSI 9
#define PIN_CS   4
#define PIN_DC   2
#define PIN_RST  3

#define TARGET_HZ 60 // display refresh rate. Falls back to 30Hz inside stopwatch_init on failure

// Battery bar refresh interval (real time from esp_timer_get_time(), independent of the refresh rate)
#define BATT_UPDATE_INTERVAL_US (10 * 1000000LL)

static const char *TAG = "main";

// Digit layout: "MM:SS" format, 5 cells (digit, digit, colon, digit, digit)
#define DIGIT_W  22
#define DIGIT_H  50
#define DIGIT_TH 6
#define COLON_W  14
#define GAP      4

#define AREA_Y 39 // around (128-50)/2, vertically centered

// Top battery bar
#define BATT_BAR_Y0 0
#define BATT_BAR_Y1 5

// Bottom running indicator bar
#define RUN_BAR_Y0 (SSD1351_HEIGHT - 6)
#define RUN_BAR_Y1 (SSD1351_HEIGHT - 1)

static int area_x0, area_x1;

static void draw_time(int64_t elapsed_ms, bool force_full) {
    int total_sec = (int)(elapsed_ms / 1000);
    int sec = total_sec % 60;
    int min = (total_sec / 60) % 100;

    int digits[4] = { min / 10, min % 10, sec / 10, sec % 10 };

    int x = area_x0;
    for (int i = 0; i < 4; i++) {
        digit_draw(x, AREA_Y, DIGIT_W, DIGIT_H, DIGIT_TH, digits[i], SSD1351_WHITE, SSD1351_BLACK);
        x += DIGIT_W + GAP;
        if (i == 1) {
            colon_draw(x, AREA_Y, DIGIT_H, 6, SSD1351_WHITE, SSD1351_BLACK);
            x += COLON_W + GAP;
        }
    }
}

static void display_task(void *arg) {
    ssd1351_t oled;
    ESP_LOGI(TAG, "SSD1351 OLED init start");
    ssd1351_init(&oled, PIN_SCK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST);
    ssd1351_fill_screen(&oled, SSD1351_BLACK);

    int total_w = DIGIT_W * 4 + COLON_W + GAP * 4;
    area_x0 = (SSD1351_WIDTH - total_w) / 2;
    area_x1 = area_x0 + total_w - 1;

    SemaphoreHandle_t tick_sem = stopwatch_init(TARGET_HZ);

    draw_time(0, true);
    ssd1351_flush_rect(&oled, area_x0, AREA_Y, area_x1, AREA_Y + DIGIT_H - 1);

    int64_t last_batt_us = -BATT_UPDATE_INTERVAL_US; // draw once right on the first tick
    sw_state_t last_state = SW_STOPPED;
    bool first_run_draw = true;

    while (1) {
        if (xSemaphoreTake(tick_sem, portMAX_DELAY) == pdTRUE) {
            // 1) Update the time display (every tick, for smooth updates)
            int64_t elapsed = stopwatch_get_elapsed_ms();
            draw_time(elapsed, false);
            ssd1351_flush_rect(&oled, area_x0, AREA_Y, area_x1, AREA_Y + DIGIT_H - 1);

            // 2) Battery bar (updated only once per BATT_UPDATE_INTERVAL_US, based on esp_timer)
            int64_t now_us = esp_timer_get_time();
            if (now_us - last_batt_us >= BATT_UPDATE_INTERVAL_US) {
                last_batt_us = now_us;
                int pct = battery_read_percent();
                int bar_width = (pct * SSD1351_WIDTH) / 100;

                ssd1351_fb_set(0, BATT_BAR_Y0, SSD1351_WIDTH - 1, BATT_BAR_Y1, SSD1351_BLACK);
                ssd1351_fb_set(0, BATT_BAR_Y0, bar_width - 1, BATT_BAR_Y1, SSD1351_GREEN);
                ssd1351_flush_rect(&oled, 0, BATT_BAR_Y0, SSD1351_WIDTH - 1, BATT_BAR_Y1);
            }

            // 3) Running indicator bar (redrawn only when the state changes -> avoids needless transfers/flicker)
            sw_state_t cur_state = stopwatch_get_state();
            if (cur_state != last_state || first_run_draw) {
                last_state = cur_state;
                first_run_draw = false;

                uint16_t color = (cur_state == SW_RUNNING) ? SSD1351_GREEN : SSD1351_BLACK;
                ssd1351_fb_set(0, RUN_BAR_Y0, SSD1351_WIDTH - 1, RUN_BAR_Y1, color);
                ssd1351_flush_rect(&oled, 0, RUN_BAR_Y0, SSD1351_WIDTH - 1, RUN_BAR_Y1);
            }
        }
    }
}

static void button_task(void *arg) {
    QueueHandle_t btn_evt_queue = (QueueHandle_t)arg;
    button_id_t evt;

    while (1) {
        if (xQueueReceive(btn_evt_queue, &evt, portMAX_DELAY)) {
            switch (evt) {
                case BTN_1:
                    stopwatch_toggle();
                    break;
                case BTN_2:
                    stopwatch_reset(); // only actually resets while paused (handled in stopwatch.c)
                    break;
                case BTN_3:
                    // reserved for a future feature
                    break;
                default:
                    break;
            }
        }
    }
}

void app_main(void) {
    battery_init();
    QueueHandle_t btn_evt_queue = button_init();

    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
    xTaskCreate(button_task, "button_task", 2048, (void *)btn_evt_queue, 6, NULL);
}
