/*
 * ssd1351.h
 * SSD1351 128x128 SPI OLED 드라이버 (ESP-IDF spi_master 기반)
 * 대상 보드: Seeed Studio XIAO ESP32-S3
 */
#pragma once

#include <stdint.h>
#include "driver/spi_master.h"

#define SSD1351_WIDTH   128
#define SSD1351_HEIGHT  128

// RGB565 색상 매크로
#define SSD1351_BLACK   0x0000
#define SSD1351_BLUE    0x001F
#define SSD1351_RED     0xF800
#define SSD1351_GREEN   0x07E0
#define SSD1351_CYAN    0x07FF
#define SSD1351_MAGENTA 0xF81F
#define SSD1351_YELLOW  0xFFE0
#define SSD1351_WHITE   0xFFFF

typedef struct {
    spi_device_handle_t spi;
    int pin_dc;
    int pin_rst;
    int pin_cs;
} ssd1351_t;

/* SPI 버스를 초기화하고 SSD1351 디스플레이를 시작합니다. */
void ssd1351_init(ssd1351_t *dev,
                   int pin_sck, int pin_mosi,
                   int pin_cs, int pin_dc, int pin_rst);

/* 화면 전체를 지정한 색으로 채웁니다. */
void ssd1351_fill_screen(ssd1351_t *dev, uint16_t color);

/* (x0,y0)-(x1,y1) 사각 영역을 지정한 색으로 채웁니다. */
void ssd1351_fill_rect(ssd1351_t *dev, int x0, int y0, int x1, int y1, uint16_t color);

/* 단일 픽셀을 그립니다. */
void ssd1351_draw_pixel(ssd1351_t *dev, int x, int y, uint16_t color);

/* 프레임버퍼에만 그립니다 (즉시 전송 안 함, 부분 갱신용) */
void ssd1351_fb_set(int x0, int y0, int x1, int y1, uint16_t color);

/* 지정한 영역만 화면에 전송합니다 (고속 부분 리프레시용) */
void ssd1351_flush_rect(ssd1351_t *dev, int x0, int y0, int x1, int y1);