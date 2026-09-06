/*
 * button.c
 * 4개 버튼 GPIO 인터럽트 처리 구현
 *
 * 배선: 각 버튼의 한쪽 다리 -> 아래 GPIO, 다른쪽 다리 -> GND
 *   BTN_1 -> D0 (GPIO1)
 *   BTN_2 -> D4 (GPIO5)
 *   BTN_3 -> D5 (GPIO6)
 *   BTN_4 -> D9 (GPIO8)
 * 내부 풀업 사용, 눌리면 LOW(0) -> Falling edge 인터럽트
 */
#include "button.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "button";

static const int btn_gpio[BTN_COUNT] = { 1, 5, 6, 8 };

static QueueHandle_t s_evt_queue = NULL;

// 디바운스: 같은 핀에서 너무 짧은 시간 내 재인터럽트는 무시
static volatile int64_t s_last_isr_us[BTN_COUNT] = {0};
#define DEBOUNCE_US 150000  // 150ms

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    int idx = (int)(intptr_t)arg;

    int64_t now = esp_timer_get_time();
    if (now - s_last_isr_us[idx] < DEBOUNCE_US) {
        return; // 디바운스 구간 내 재입력 무시 (ISR 내에서 처리, 가볍게)
    }
    s_last_isr_us[idx] = now;

    button_id_t id = (button_id_t)idx;
    // ISR에서는 큐에 넣는 것만 수행 (블로킹 금지 -> FromISR 버전 사용)
    xQueueSendFromISR(s_evt_queue, &id, NULL);
}

QueueHandle_t button_init(void) {
    s_evt_queue = xQueueCreate(10, sizeof(button_id_t));

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // 눌림 = HIGH->LOW
    };

    uint64_t mask = 0;
    for (int i = 0; i < BTN_COUNT; i++) {
        mask |= (1ULL << btn_gpio[i]);
    }
    io_conf.pin_bit_mask = mask;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);

    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_isr_handler_add(btn_gpio[i], gpio_isr_handler, (void *)(intptr_t)i);
    }

    ESP_LOGI(TAG, "4 buttons initialized (GPIO1, GPIO5, GPIO6, GPIO8), interrupt mode");
    return s_evt_queue;
}
