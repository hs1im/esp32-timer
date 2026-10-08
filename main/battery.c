/*
 * battery.c
 * Battery voltage measurement based on the ESP-IDF adc_oneshot + adc_cali APIs
 */
#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "battery";

static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_cali_handle;
static bool s_cali_ok = false;

void battery_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,   // Close to full range (maximum attenuation on ESP32-S3)
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
        ESP_LOGW(TAG, "ADC calibration failed, using raw value approximation");
    }

    ESP_LOGI(TAG, "battery ADC init done (GPIO%d)", BATTERY_ADC_GPIO);
}

float battery_read_voltage(void) {
    int raw = 0, mv = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw));

    if (s_cali_ok) {
        adc_cali_raw_to_voltage(s_cali_handle, raw, &mv);
    } else {
        mv = (raw * 3300) / 4095; // rough approximation
    }

    return (mv / 1000.0f) * BATTERY_DIVIDER_RATIO;
}

typedef struct {
    float voltage;
    int percent;
} battery_point_t;

static const battery_point_t s_curve[] = { BATTERY_CURVE_TABLE };
#define CURVE_LEN (sizeof(s_curve) / sizeof(s_curve[0]))

int battery_read_percent(void) {
    float v = battery_read_voltage();
    if (v >= s_curve[0].voltage) return s_curve[0].percent;
    if (v <= s_curve[CURVE_LEN - 1].voltage) return s_curve[CURVE_LEN - 1].percent;

    // Find the two table rows around v and interpolate linearly between them
    for (size_t i = 1; i < CURVE_LEN; i++) {
        if (v >= s_curve[i].voltage) {
            float v_hi = s_curve[i - 1].voltage, v_lo = s_curve[i].voltage;
            int p_hi = s_curve[i - 1].percent, p_lo = s_curve[i].percent;
            return p_lo + (int)((v - v_lo) / (v_hi - v_lo) * (p_hi - p_lo));
        }
    }
    return 0;
}
