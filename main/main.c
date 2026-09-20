/*
 * 배선:
 *  OLED VCC -> 3V3     OLED GND -> GND
 *  OLED CLK -> GPIO7 (D8)   OLED MOSI -> GPIO9 (D10)
 *  OLED RES -> GPIO3 (D2)   OLED DC   -> GPIO2 (D1)
 *  OLED CS  -> GPIO4 (D3)
 *
 *  BTN_1 -> GPIO1 (D0)   BTN_2 -> GPIO5 (D4)   BTN_3 -> GPIO6 (D5)
 *  (버튼 반대쪽 다리는 모두 GND)
 *
 *  배터리 ADC -> GPIO8 (D9, 예전 BTN_4 자리) - 100k+100k 전압 분배기 경유
 *
 *  사용 금지 핀: GPIO43/44 (D6/D7, USB UART0), GPIO19/20 (네이티브 USB)
 */
/*
 * main.c - 초시계(Stopwatch) 앱
 * 대상 보드: Seeed Studio XIAO ESP32-S3
 *
 *  BTN_1 (GPIO1, D0): 시작 / 일시정지
 *  BTN_2 (GPIO5, D4): 초기화 (일시정지 상태에서만 동작, 실행 중엔 무시됨)
 *  BTN_3 (GPIO6, D5): 추후 기능 추가 예정 (현재 무시)
 *
 *  화면 갱신: 하드웨어 타이머(gptimer) 인터럽트로 60Hz 트리거
 *  (실패 시 자동 30Hz), 숫자 영역만 부분 전송
 *  시간 측정: esp_timer_get_time() 기반, 갱신 주기와 완전히 독립적으로 계산
 *
 *  상단 바: 배터리 잔량 (1초에 한 번 갱신)
 *  하단 바: 스톱워치 실행 중일 때만 초록색으로 표시
 */
#include "ssd1351.h"
#include "button.h"
#include "stopwatch.h"
#include "digit7seg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "battery.h"

#define PIN_SCK  7
#define PIN_MOSI 9
#define PIN_CS   4
#define PIN_DC   2
#define PIN_RST  3

#define TARGET_HZ 600 // 안되면 stopwatch_init 내부에서 자동 30Hz로 낮춰짐

static const char *TAG = "main";

// 숫자 레이아웃: "MM:SS" 형식, 5칸(digit,digit,colon,digit,digit)
#define DIGIT_W  22
#define DIGIT_H  50
#define DIGIT_TH 6
#define COLON_W  14
#define GAP      4

#define AREA_Y 39 // (128-50)/2 근처, 세로 중앙

// 상단 배터리 바
#define BATT_BAR_Y0 0
#define BATT_BAR_Y1 5

// 하단 실행 표시 바
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
    ESP_LOGI(TAG, "SSD1351 OLED 초기화 시작");
    ssd1351_init(&oled, PIN_SCK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST);
    ssd1351_fill_screen(&oled, SSD1351_BLACK);

    int total_w = DIGIT_W * 4 + COLON_W + GAP * 4;
    area_x0 = (SSD1351_WIDTH - total_w) / 2;
    area_x1 = area_x0 + total_w - 1;

    SemaphoreHandle_t tick_sem = stopwatch_init(TARGET_HZ);

    draw_time(0, true);
    ssd1351_flush_rect(&oled, area_x0, AREA_Y, area_x1, AREA_Y + DIGIT_H - 1);

    int battery_tick = 0;
    sw_state_t last_state = SW_STOPPED;
    bool first_run_draw = true;

    while (1) {
        if (xSemaphoreTake(tick_sem, portMAX_DELAY) == pdTRUE) {
            // 1) 시간 표시 갱신 (매 틱마다, 부드럽게)
            int64_t elapsed = stopwatch_get_elapsed_ms();
            draw_time(elapsed, false);
            ssd1351_flush_rect(&oled, area_x0, AREA_Y, area_x1, AREA_Y + DIGIT_H - 1);

            // 2) 배터리 바 (약 1초에 한 번만 갱신)
            if (++battery_tick >= TARGET_HZ) {
                battery_tick = 0;
                int pct = battery_read_percent();
                int bar_width = (pct * SSD1351_WIDTH) / 100;

                ssd1351_fb_set(0, BATT_BAR_Y0, SSD1351_WIDTH - 1, BATT_BAR_Y1, SSD1351_BLACK);
                ssd1351_fb_set(0, BATT_BAR_Y0, bar_width - 1, BATT_BAR_Y1, SSD1351_GREEN);
                ssd1351_flush_rect(&oled, 0, BATT_BAR_Y0, SSD1351_WIDTH - 1, BATT_BAR_Y1);
            }

            // 3) 실행 상태 표시 바 (상태가 바뀔 때만 갱신 -> 불필요한 전송/깜빡임 방지)
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
                    stopwatch_reset(); // 일시정지 상태에서만 실제로 리셋됨 (stopwatch.c에서 처리)
                    break;
                case BTN_3:
                    // 추후 기능 추가 예정
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