/*
 * stopwatch.c
 * 시간 측정(esp_timer) + 화면 갱신 트리거(gptimer 하드웨어 인터럽트) 구현
 */
#include "stopwatch.h"
#include "driver/gptimer.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "stopwatch";

static gptimer_handle_t s_gptimer = NULL;
static SemaphoreHandle_t s_tick_sem = NULL;

static sw_state_t s_state = SW_STOPPED;
static int64_t s_start_us = 0;
static int64_t s_accumulated_us = 0;

// 타이머 인터럽트 콜백: 무거운 작업 없이 세마포어만 올립니다.
static bool IRAM_ATTR gptimer_alarm_cb(gptimer_handle_t timer,
                                       const gptimer_alarm_event_data_t *edata,
                                       void *user_ctx) {
    BaseType_t high_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_tick_sem, &high_task_woken);
    return high_task_woken == pdTRUE;
}

static bool try_start_timer(int hz) {
    if (s_gptimer) {
        gptimer_disable(s_gptimer);
        gptimer_del_timer(s_gptimer);
        s_gptimer = NULL;
    }

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz -> 1틱 = 1us
    };
    if (gptimer_new_timer(&timer_config, &s_gptimer) != ESP_OK) return false;

    gptimer_event_callbacks_t cbs = { .on_alarm = gptimer_alarm_cb };
    gptimer_register_event_callbacks(s_gptimer, &cbs, NULL);

    uint64_t period_us = 1000000ULL / hz;
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = period_us,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    if (gptimer_set_alarm_action(s_gptimer, &alarm_config) != ESP_OK) return false;

    if (gptimer_enable(s_gptimer) != ESP_OK) return false;
    if (gptimer_start(s_gptimer) != ESP_OK) return false;

    ESP_LOGI(TAG, "gptimer started at %d Hz (period %lluus)", hz, (unsigned long long)period_us);
    return true;
}

SemaphoreHandle_t stopwatch_init(int target_hz) {
    s_tick_sem = xSemaphoreCreateBinary();

    if (!try_start_timer(target_hz)) {
        ESP_LOGW(TAG, "%dHz 설정 실패, 30Hz로 재시도", target_hz);
        if (!try_start_timer(30)) {
            ESP_LOGE(TAG, "gptimer 초기화 실패");
        }
    }
    return s_tick_sem;
}

void stopwatch_toggle(void) {
    int64_t now = esp_timer_get_time();
    if (s_state == SW_STOPPED || s_state == SW_PAUSED) {
        s_start_us = now;
        s_state = SW_RUNNING;
    } else if (s_state == SW_RUNNING) {
        s_accumulated_us += now - s_start_us;
        s_state = SW_PAUSED;
    }
}

void stopwatch_reset(void) {
    s_state = SW_STOPPED;
    s_accumulated_us = 0;
    s_start_us = 0;
}

int64_t stopwatch_get_elapsed_ms(void) {
    int64_t elapsed_us = s_accumulated_us;
    if (s_state == SW_RUNNING) {
        elapsed_us += esp_timer_get_time() - s_start_us;
    }
    return elapsed_us / 1000;
}

sw_state_t stopwatch_get_state(void) {
    return s_state;
}
