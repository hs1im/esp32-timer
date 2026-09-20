/*
 * button.c
 * BTN_1: 시작/일시정지, BTN_2: 초기화(일시정지 중에만), BTN_3: 추후 사용
 * (BTN_4/GPIO8은 배터리 ADC용으로 넘어가서 버튼에서 제외됨)
 */
#include "button.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "button";

static const int btn_gpio[BTN_COUNT] = { 1, 5, 6 };

static QueueHandle_t s_evt_queue = NULL;

static volatile int64_t s_last_isr_us[BTN_COUNT] = {0};
#define DEBOUNCE_US 200000 // 200ms

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    int idx = (int)(intptr_t)arg;
    int64_t now = esp_timer_get_time();
    if (now - s_last_isr_us[idx] < DEBOUNCE_US) return;
    s_last_isr_us[idx] = now;

    button_id_t id = (button_id_t)idx;
    xQueueSendFromISR(s_evt_queue, &id, NULL);
}

QueueHandle_t button_init(void) {
    s_evt_queue = xQueueCreate(10, sizeof(button_id_t));

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    uint64_t mask = 0;
    for (int i = 0; i < BTN_COUNT; i++) mask |= (1ULL << btn_gpio[i]);
    io_conf.pin_bit_mask = mask;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_isr_handler_add(btn_gpio[i], gpio_isr_handler, (void *)(intptr_t)i);
    }

    ESP_LOGI(TAG, "3 buttons ready (GPIO1, GPIO5, GPIO6)");
    return s_evt_queue;
}