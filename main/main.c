/*
 * main.c
 * SSD1351 128x128 SPI OLED 예제 - ESP-IDF (VS Code ESP-IDF 확장 기준)
 * 대상 보드: Seeed Studio XIAO ESP32-S3
 *
 * 배선 (XIAO ESP32-S3 GPIO 번호 기준):
 *   OLED VCC -> 3V3
 *   OLED GND -> GND
 *   OLED CLK (SCL) -> GPIO7  (XIAO 라벨 D8)
 *   OLED MOSI(SDA) -> GPIO9  (XIAO 라벨 D10)
 *   OLED RES       -> GPIO3  (XIAO 라벨 D2)
 *   OLED DC        -> GPIO2  (XIAO 라벨 D1)
 *   OLED CS        -> GPIO4  (XIAO 라벨 D3)
 *
 * 주의: GPIO43/44 (D6/D7, USB UART0)와 GPIO19/20 (네이티브 USB)는
 *       사용하지 않습니다.
 */
#include "ssd1351.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define PIN_SCK  7
#define PIN_MOSI 9
#define PIN_CS   4
#define PIN_DC   2
#define PIN_RST  3

static const char *TAG = "main";

void app_main(void) {
    ssd1351_t oled;

    ESP_LOGI(TAG, "SSD1351 OLED 초기화 시작");
    ssd1351_init(&oled, PIN_SCK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST);

    uint16_t colors[] = {
        SSD1351_RED, SSD1351_GREEN, SSD1351_BLUE,
        SSD1351_YELLOW, SSD1351_CYAN, SSD1351_MAGENTA
    };
    int idx = 0;

    while (1) {
        ESP_LOGI(TAG, "fill color idx=%d", idx);
        ssd1351_fill_screen(&oled, colors[idx]);

        // 화면 중앙에 작은 사각형 데모
        ssd1351_fill_rect(&oled, 44, 44, 83, 83, SSD1351_BLACK);

        idx = (idx + 1) % (sizeof(colors) / sizeof(colors[0]));
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}