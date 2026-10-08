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
 *  Display refresh: triggered at DISPLAY_REFRESH_HZ by a hardware timer (gptimer) interrupt
 *  (falls back to 30Hz on failure), only the digit area is sent (partial update)
 *  Time measurement: based on esp_timer_get_time(), fully independent of the refresh rate
 *
 *  Top bar: battery level (refreshed once every 10 seconds, based on esp_timer)
 *  Bottom bar: shown in green only while the stopwatch is running
 */
#include "constants.h"
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
#include "nvs_flash.h"

static const char *TAG = "main";

static int area_x0, area_x1;

static void draw_time(int64_t elapsed_ms, bool force_full) {
    int total_sec = (int)(elapsed_ms / 1000);
    int sec = total_sec % 60;
    int min = (total_sec / 60) % 60;
    int hour = (total_sec / 3600) % TIME_HOURS_MOD;

    int digits[6] = { hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10 };

    int x = area_x0;
    for (int i = 0; i < 6; i++) {
        digit_draw(x, TIME_AREA_Y, DIGIT_W, DIGIT_H, DIGIT_TH, digits[i], SSD1351_WHITE, SSD1351_BLACK);
        x += DIGIT_W + DIGIT_GAP;
        // Colon after the hour pair and after the minute pair
        if (i == 1 || i == 3) {
            colon_draw(x, TIME_AREA_Y, DIGIT_H, COLON_DOT_SIZE, SSD1351_WHITE, SSD1351_BLACK);
            x += COLON_W + DIGIT_GAP;
        }
    }
}

static void display_task(void *arg) {
    ssd1351_t oled;
    ESP_LOGI(TAG, "SSD1351 OLED init start");
    ssd1351_init(&oled, PIN_OLED_SCK, PIN_OLED_MOSI, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);
    ssd1351_fill_screen(&oled, SSD1351_BLACK);

    // 6 digits + 2 colons = 8 cells -> 7 gaps between them
    int total_w = DIGIT_W * 6 + COLON_W * 2 + DIGIT_GAP * 7;
    area_x0 = (SSD1351_WIDTH - total_w) / 2;
    area_x1 = area_x0 + total_w - 1;

    SemaphoreHandle_t tick_sem = stopwatch_init(DISPLAY_REFRESH_HZ);

    draw_time(stopwatch_get_elapsed_ms(), true); // shows the restored time after a power-off
    ssd1351_flush_rect(&oled, area_x0, TIME_AREA_Y, area_x1, TIME_AREA_Y + DIGIT_H - 1);

    int64_t last_batt_us = -BATTERY_UPDATE_INTERVAL_US; // draw once right on the first tick
    sw_state_t last_state = SW_STOPPED;
    bool first_run_draw = true;

    while (1) {
        if (xSemaphoreTake(tick_sem, portMAX_DELAY) == pdTRUE) {
            // 1) Update the time display (every tick, for smooth updates)
            int64_t elapsed = stopwatch_get_elapsed_ms();
            draw_time(elapsed, false);
            ssd1351_flush_rect(&oled, area_x0, TIME_AREA_Y, area_x1, TIME_AREA_Y + DIGIT_H - 1);

            // 2) Battery bar (updated only once per BATTERY_UPDATE_INTERVAL_US, based on esp_timer)
            int64_t now_us = esp_timer_get_time();
            if (now_us - last_batt_us >= BATTERY_UPDATE_INTERVAL_US) {
                last_batt_us = now_us;
                int pct = battery_read_percent();
                int bar_width = (pct * SSD1351_WIDTH) / 100;

                ssd1351_fb_set(0, BATT_BAR_Y0, SSD1351_WIDTH - 1, BATT_BAR_Y1, SSD1351_BLACK);
                ssd1351_fb_set(0, BATT_BAR_Y0, bar_width - 1, BATT_BAR_Y1, SSD1351_GREEN);
                ssd1351_flush_rect(&oled, 0, BATT_BAR_Y0, SSD1351_WIDTH - 1, BATT_BAR_Y1);
            }

            // 3) Persist the elapsed time (only does something once per STOPWATCH_SAVE_INTERVAL_US)
            stopwatch_save_if_due();

            // 4) Running indicator bar (redrawn only when the state changes -> avoids needless transfers/flicker)
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
    // NVS must be ready before stopwatch_init() restores the saved time
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    battery_init();
    QueueHandle_t btn_evt_queue = button_init();

    xTaskCreate(display_task, "display_task", DISPLAY_TASK_STACK, NULL, DISPLAY_TASK_PRIORITY, NULL);
    xTaskCreate(button_task, "button_task", BUTTON_TASK_STACK, (void *)btn_evt_queue, BUTTON_TASK_PRIORITY, NULL);
}
