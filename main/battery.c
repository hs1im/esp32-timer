/*
 * battery.c
 * ESP-IDF adc_oneshot + adc_cali API 기반 배터리 전압 측정
 */
#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "battery";

// GPIO8 -> ADC1 채널 매핑 (ESP32-S3: ADC1_CH0=GPIO1 ... ADC1_CH7=GPIO8)
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_7
#define VOLTAGE_DIVIDER_RATIO 2.0f  // 100k:100k = 1/2 분배 -> 실제 전압은 2배

// 1셀 리튬폴리머 기준 전압 범위 (필요 시 배터리 스펙에 맞게 조정)
#define BATTERY_FULL_V  4.2f
#define BATTERY_EMPTY_V 3.0f

static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_cali_handle;
static bool s_cali_ok = false;

void battery_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,   // 풀 레인지에 가깝게 (ESP32-S3 최대 감쇄)
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle) == ESP_OK) {
        s_cali_ok = true;
    } else {
        ESP_LOGW(TAG, "ADC 캘리브레이션 실패, raw 값 근사치 사용");
    }

    ESP_LOGI(TAG, "battery ADC init done (GPIO%d)", BATTERY_ADC_GPIO);
}

float battery_read_voltage(void) {
    int raw = 0, mv = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw));

    if (s_cali_ok) {
        adc_cali_raw_to_voltage(s_cali_handle, raw, &mv);
    } else {
        mv = (raw * 3300) / 4095; // 대략적인 근사치
    }

    return (mv / 1000.0f) * VOLTAGE_DIVIDER_RATIO;
}

int battery_read_percent(void) {
    float v = battery_read_voltage();
    if (v >= BATTERY_FULL_V) return 100;
    if (v <= BATTERY_EMPTY_V) return 0;
    return (int)(((v - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V)) * 100.0f);
}
