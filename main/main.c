/*
 * 배선:
 *   OLED VCC -> 3V3       OLED GND -> GND
 *   OLED CLK -> GPIO7 (D8)   OLED MOSI -> GPIO9 (D10)
 *   OLED RES -> GPIO3 (D2)   OLED DC   -> GPIO2 (D1)
 *   OLED CS  -> GPIO4 (D3)
 *
 *   BTN_1 -> GPIO1 (D0)   BTN_2 -> GPIO5 (D4)
 *   BTN_3 -> GPIO6 (D5)   BTN_4 -> GPIO8 (D9)
 *   (버튼 반대쪽 다리는 모두 GND)
 *
 * 사용 금지 핀: GPIO43/44 (D6/D7, USB UART0), GPIO19/20 (네이티브 USB)
 */
/*
 * main.c - 초시계(Stopwatch) 앱
 * 대상 보드: Seeed Studio XIAO ESP32-S3
 *
 * BTN_1 (GPIO1, D0): 시작 / 일시정지
 * BTN_2 (GPIO5, D4): 초기화 (정지 + 0으로 리셋)
 * BTN_3, BTN_4: 추후 기능 추가 예정 (현재 무시)
 *
 * 화면 갱신: 하드웨어 타이머(gptimer) 인터럽트로 60Hz 트리거
 *           (실패 시 자동 30Hz), 숫자 영역만 부분 전송
 * 시간 측정: esp_timer_get_time() 기반, 갱신 주기와 완전히 독립적으로 계산
 */
#include "ssd1351.h"
#include "button.h"
#include "stopwatch.h"
#include "digit7seg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define PIN_SCK  7
#define PIN_MOSI 9
#define PIN_CS   4
#define PIN_DC   2
#define PIN_RST  3

#define TARGET_HZ 60   // 안되면 stopwatch_init 내부에서 자동 30Hz로 낮춰짐

static const char *TAG = "main";

// 숫자 레이아웃: "MM:SS" 형식, 5칸(digit,digit,colon,digit,digit)
#define DIGIT_W   22
#define DIGIT_H   50
#define DIGIT_TH  6
#define COLON_W   14
#define GAP       4

#define AREA_Y    39   // (128-50)/2 근처, 세로 중앙

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
    ESP_LOGI(TAG, "SSD1351 OLED 초기화 시작");
    ssd1351_init(&oled, PIN_SCK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST);
    ssd1351_fill_screen(&oled, SSD1351_BLACK);

    int total_w = DIGIT_W * 4 + COLON_W + GAP * 4;
    area_x0 = (SSD1351_WIDTH - total_w) / 2;
    area_x1 = area_x0 + total_w - 1;

    SemaphoreHandle_t tick_sem = stopwatch_init(TARGET_HZ);

    draw_time(0, true);
    ssd1351_flush_rect(&oled, area_x0, AREA_Y, area_x1, AREA_Y + DIGIT_H - 1);

    while (1) {
        if (xSemaphoreTake(tick_sem, portMAX_DELAY) == pdTRUE) {
            int64_t elapsed = stopwatch_get_elapsed_ms();
            draw_time(elapsed, false);
            ssd1351_flush_rect(&oled, area_x0, AREA_Y, area_x1, AREA_Y + DIGIT_H - 1);
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
                    ESP_LOGI(TAG, "BTN_1 -> toggle (state=%d)", stopwatch_get_state());
                    break;
                case BTN_2:
                    stopwatch_reset();
                    ESP_LOGI(TAG, "BTN_2 -> reset");
                    break;
                case BTN_3:
                case BTN_4:
                    ESP_LOGI(TAG, "BTN_3/4 눌림 (아직 기능 없음)");
                    break;
            }
        }
    }
}

void app_main(void) {
    QueueHandle_t btn_evt_queue = button_init();

    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
    xTaskCreate(button_task, "button_task", 2048, (void *)btn_evt_queue, 6, NULL);
}