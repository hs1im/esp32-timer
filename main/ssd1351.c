/*
 * ssd1351_dma.c
 * SSD1351 128x128 SPI OLED 드라이버 - GDMA(DMA) 최적화 버전
 * 기존 ssd1351.c를 이 파일로 교체하면 됩니다.
 *
 * 변경점:
 *  1) 라인 단위 전송 -> 프레임버퍼 통째로 1회 DMA 전송
 *  2) heap_caps_malloc(MALLOC_CAP_DMA)로 DMA 캐퍼블 메모리에 버퍼 할당
 *  3. spi_device_polling_transmit -> spi_device_transmit
 *     (큰 전송은 드라이버가 자동으로 GDMA 경로를 사용)
 */
#include <string.h>
#include "ssd1351.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ssd1351";

#define CMD_SETCOLUMN      0x15
#define CMD_SETROW         0x75
#define CMD_WRITERAM       0x5C
#define CMD_SETREMAP       0xA0
#define CMD_STARTLINE      0xA1
#define CMD_DISPLAYOFFSET  0xA2
#define CMD_NORMALDISPLAY  0xA6
#define CMD_FUNCTIONSELECT 0xAB
#define CMD_DISPLAYOFF     0xAE
#define CMD_DISPLAYON      0xAF
#define CMD_PRECHARGE      0xB1
#define CMD_CLOCKDIV       0xB3
#define CMD_VSL            0xB4
#define CMD_SETGPIO        0xB5
#define CMD_PRECHARGE2     0xB6
#define CMD_VCOMH          0xBE
#define CMD_CONTRASTABC    0xC1
#define CMD_CONTRASTMASTER 0xC7
#define CMD_MUXRATIO       0xCA
#define CMD_COMMANDLOCK    0xFD

// DMA 캐퍼블 프레임버퍼 (128*128*2 bytes = 32KB)
static uint8_t *fb = NULL;

static void gpio_low(int pin)  { gpio_set_level(pin, 0); }
static void gpio_high(int pin) { gpio_set_level(pin, 1); }

// 작은 전송(명령/짧은 데이터)은 polling, 큰 전송(프레임버퍼)은 DMA 경유 transmit 사용
static void spi_write_polling(ssd1351_t *dev, const uint8_t *data, size_t len) {
    if (len == 0) return;
    spi_transaction_t t = { .length = len * 8, .tx_buffer = data };
    ESP_ERROR_CHECK(spi_device_polling_transmit(dev->spi, &t));
}

static void spi_write_dma(ssd1351_t *dev, const uint8_t *data, size_t len) {
    if (len == 0) return;
    spi_transaction_t t = { .length = len * 8, .tx_buffer = data };
    // 큰 버퍼는 spi_device_transmit이 내부적으로 GDMA 경로를 사용
    ESP_ERROR_CHECK(spi_device_transmit(dev->spi, &t));
}

static void send_cmd(ssd1351_t *dev, uint8_t cmd) {
    gpio_low(dev->pin_dc);
    spi_write_polling(dev, &cmd, 1);
}

static void send_data(ssd1351_t *dev, const uint8_t *data, size_t len) {
    gpio_high(dev->pin_dc);
    spi_write_polling(dev, data, len);
}

static void send_data_byte(ssd1351_t *dev, uint8_t b) {
    send_data(dev, &b, 1);
}

static void set_addr_window(ssd1351_t *dev, int x0, int y0, int x1, int y1) {
    send_cmd(dev, CMD_SETCOLUMN);
    uint8_t col[2] = { (uint8_t)x0, (uint8_t)x1 };
    send_data(dev, col, 2);
    send_cmd(dev, CMD_SETROW);
    uint8_t row[2] = { (uint8_t)y0, (uint8_t)y1 };
    send_data(dev, row, 2);
    send_cmd(dev, CMD_WRITERAM);
}

void ssd1351_init(ssd1351_t *dev,
                   int pin_sck, int pin_mosi,
                   int pin_cs, int pin_dc, int pin_rst) {
    dev->pin_dc = pin_dc;
    dev->pin_rst = pin_rst;
    dev->pin_cs = pin_cs;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin_dc) | (1ULL << pin_rst),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    spi_bus_config_t buscfg = {
        .mosi_io_num = pin_mosi,
        .miso_io_num = -1,
        .sclk_io_num = pin_sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SSD1351_WIDTH * SSD1351_HEIGHT * 2, // 프레임버퍼 통째 전송 허용
    };
    // SPI_DMA_CH_AUTO -> GDMA 채널 자동 할당
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000, // DMA 활용 시 클럭을 더 올려도 안정적
        .mode = 0,
        .spics_io_num = pin_cs,
        .queue_size = 2,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &dev->spi));

    // DMA 캐퍼블 메모리에 프레임버퍼 할당 (필수: MALLOC_CAP_DMA)
    fb = heap_caps_malloc(SSD1351_WIDTH * SSD1351_HEIGHT * 2, MALLOC_CAP_DMA);
    if (fb == NULL) {
        ESP_LOGE(TAG, "프레임버퍼 DMA 메모리 할당 실패");
    }

    gpio_high(dev->pin_rst);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_low(dev->pin_rst);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_high(dev->pin_rst);
    vTaskDelay(pdMS_TO_TICKS(10));

    send_cmd(dev, CMD_COMMANDLOCK); send_data_byte(dev, 0x12);
    send_cmd(dev, CMD_COMMANDLOCK); send_data_byte(dev, 0xB1);
    send_cmd(dev, CMD_DISPLAYOFF);
    send_cmd(dev, CMD_CLOCKDIV);   send_data_byte(dev, 0xF1);
    send_cmd(dev, CMD_MUXRATIO);   send_data_byte(dev, 127);
    send_cmd(dev, CMD_SETREMAP);   send_data_byte(dev, 0x74);
    send_cmd(dev, CMD_SETCOLUMN);  { uint8_t d[2]={0,127}; send_data(dev,d,2); }
    send_cmd(dev, CMD_SETROW);     { uint8_t d[2]={0,127}; send_data(dev,d,2); }
    send_cmd(dev, CMD_STARTLINE);  send_data_byte(dev, 0x00);
    send_cmd(dev, CMD_DISPLAYOFFSET); send_data_byte(dev, 0x00);
    send_cmd(dev, CMD_SETGPIO);    send_data_byte(dev, 0x00);
    send_cmd(dev, CMD_FUNCTIONSELECT); send_data_byte(dev, 0x01);
    send_cmd(dev, CMD_PRECHARGE);  send_data_byte(dev, 0x32);
    send_cmd(dev, CMD_VCOMH);      send_data_byte(dev, 0x05);
    send_cmd(dev, CMD_NORMALDISPLAY);
    send_cmd(dev, CMD_CONTRASTABC); { uint8_t d[3]={0xC8,0x80,0xC8}; send_data(dev,d,3); }
    send_cmd(dev, CMD_CONTRASTMASTER); send_data_byte(dev, 0x0F);
    send_cmd(dev, CMD_VSL);        { uint8_t d[3]={0xA0,0xB5,0x55}; send_data(dev,d,3); }
    send_cmd(dev, CMD_PRECHARGE2); send_data_byte(dev, 0x01);
    send_cmd(dev, CMD_DISPLAYON);

    ESP_LOGI(TAG, "SSD1351 init complete (DMA framebuffer ready)");
}

// 프레임버퍼에만 그리는 함수 (아직 화면에 전송 안 함)
void ssd1351_fb_fill_rect(int x0, int y0, int x1, int y1, uint16_t color) {
    if (fb == NULL) return;
    uint8_t hi = color >> 8, lo = color & 0xFF;
    for (int y = y0; y <= y1 && y < SSD1351_HEIGHT; y++) {
        for (int x = x0; x <= x1 && x < SSD1351_WIDTH; x++) {
            int i = (y * SSD1351_WIDTH + x) * 2;
            fb[i] = hi;
            fb[i + 1] = lo;
        }
    }
}

// 프레임버퍼 전체를 GDMA로 한 번에 전송 (핵심: 128*128*2 바이트를 1회 전송)
void ssd1351_flush(ssd1351_t *dev) {
    if (fb == NULL) return;
    set_addr_window(dev, 0, 0, SSD1351_WIDTH - 1, SSD1351_HEIGHT - 1);
    gpio_high(dev->pin_dc);
    spi_write_dma(dev, fb, SSD1351_WIDTH * SSD1351_HEIGHT * 2);
}

void ssd1351_fill_screen(ssd1351_t *dev, uint16_t color) {
    ssd1351_fb_fill_rect(0, 0, SSD1351_WIDTH - 1, SSD1351_HEIGHT - 1, color);
    ssd1351_flush(dev);
}

void ssd1351_fill_rect(ssd1351_t *dev, int x0, int y0, int x1, int y1, uint16_t color) {
    ssd1351_fb_fill_rect(x0, y0, x1, y1, color);
    ssd1351_flush(dev);
}

void ssd1351_draw_pixel(ssd1351_t *dev, int x, int y, uint16_t color) {
    ssd1351_fb_fill_rect(x, y, x, y, color);
    ssd1351_flush(dev);
}
