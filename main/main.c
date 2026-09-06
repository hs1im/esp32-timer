/*
 * main.c (v2)
 * SSD1351 OLED + 4버튼(HW 인터럽트) - FreeRTOS 태스크 기반 구조
 * 대상 보드: Seeed Studio XIAO ESP32-S3
 *
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
#include "ssd1351.h"
#include "button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define PIN_SCK  7
#define PIN_MOSI 9
#define PIN_CS   4
#define PIN_DC   2
#define PIN_RST  3

static const char *TAG = "main";

static const uint16_t COLOR_TABLE[] = {
    SSD1351_RED, SSD1351_GREEN, SSD1351_BLUE,
    SSD1351_YELLOW, SSD1351_CYAN, SSD1351_MAGENTA
};
#define COLOR_COUNT (sizeof(COLOR_TABLE) / sizeof(COLOR_TABLE[0]))

// 디스플레이 태스크에 보내는 명령
typedef enum {
    DISP_CMD_NEXT_COLOR,
    DISP_CMD_PREV_COLOR,
    DISP_CMD_TOGGLE_AUTO,
    DISP_CMD_RESET,
} disp_cmd_t;

static QueueHandle_t s_disp_cmd_queue;

/* ---------------- 버튼 태스크 ---------------- */
// button_init()이 만든 큐에서 원시 눌림 이벤트를 받아
// 디스플레이 명령으로 변환해서 disp_cmd_queue로 전달
static void button_task(void *arg) {
    QueueHandle_t btn_evt_queue = (QueueHandle_t)arg;
    button_id_t evt;

    while (1) {
        if (xQueueReceive(btn_evt_queue, &evt, portMAX_DELAY)) {
            disp_cmd_t cmd;
            switch (evt) {
                case BTN_1: cmd = DISP_CMD_NEXT_COLOR;  ESP_LOGI(TAG, "BTN_1 pressed -> next color"); break;
                case BTN_2: cmd = DISP_CMD_PREV_COLOR;  ESP_LOGI(TAG, "BTN_2 pressed -> prev color"); break;
                case BTN_3: cmd = DISP_CMD_TOGGLE_AUTO; ESP_LOGI(TAG, "BTN_3 pressed -> toggle auto"); break;
                case BTN_4: cmd = DISP_CMD_RESET;       ESP_LOGI(TAG, "BTN_4 pressed -> reset"); break;
                default: continue;
            }
            xQueueSend(s_disp_cmd_queue, &cmd, 0);
        }
    }
}

/* ---------------- 디스플레이 태스크 ---------------- */
// OLED(SPI)는 이 태스크만 접근합니다 (동시 접근 방지)
static void display_task(void *arg) {
    ssd1351_t oled;
    ESP_LOGI(TAG, "SSD1351 OLED 초기화 시작");
    ssd1351_init(&oled, PIN_SCK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST);

    int color_idx = 0;
    bool auto_cycle = true;
    disp_cmd_t cmd;

    TickType_t last_auto_tick = xTaskGetTickCount();

    while (1) {
        // 명령 큐 확인 (최대 100ms 대기, 없으면 자동 애니메이션 진행)
        if (xQueueReceive(s_disp_cmd_queue, &cmd, pdMS_TO_TICKS(100))) {
            switch (cmd) {
                case DISP_CMD_NEXT_COLOR:
                    color_idx = (color_idx + 1) % COLOR_COUNT;
                    break;
                case DISP_CMD_PREV_COLOR:
                    color_idx = (color_idx - 1 + COLOR_COUNT) % COLOR_COUNT;
                    break;
                case DISP_CMD_TOGGLE_AUTO:
                    auto_cycle = !auto_cycle;
                    break;
                case DISP_CMD_RESET:
                    color_idx = 0;
                    auto_cycle = true;
                    break;
            }
            ssd1351_fill_screen(&oled, COLOR_TABLE[color_idx]);
            ssd1351_fill_rect(&oled, 44, 44, 83, 83, SSD1351_BLACK);
            continue;
        }

        // 자동 색상 순환 (버튼3으로 on/off 가능)
        if (auto_cycle && (xTaskGetTickCount() - last_auto_tick) >= pdMS_TO_TICKS(1500)) {
            color_idx = (color_idx + 1) % COLOR_COUNT;
            ssd1351_fill_screen(&oled, COLOR_TABLE[color_idx]);
            ssd1351_fill_rect(&oled, 44, 44, 83, 83, SSD1351_BLACK);
            last_auto_tick = xTaskGetTickCount();
        }
    }
}

void app_main(void) {
    s_disp_cmd_queue = xQueueCreate(10, sizeof(disp_cmd_t));
    QueueHandle_t btn_evt_queue = button_init();

    // 디스플레이 태스크: SPI 통신이 있으니 스택을 넉넉히, 우선순위는 보통
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);

    // 버튼 태스크: 가벼운 처리, 우선순위를 조금 더 높게 둬서 반응성 확보
    xTaskCreate(button_task, "button_task", 2048, (void *)btn_evt_queue, 6, NULL);
}