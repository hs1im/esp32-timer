/*
 * ssd1351.h
 * SSD1351 128x128 SPI OLED driver (based on ESP-IDF spi_master)
 * Target board: Seeed Studio XIAO ESP32-S3
 */
#pragma once

#include <stdint.h>
#include "driver/spi_master.h"

#define SSD1351_WIDTH   128
#define SSD1351_HEIGHT  128

// RGB565 color macros
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

/* Initializes the SPI bus and starts the SSD1351 display. */
void ssd1351_init(ssd1351_t *dev,
                   int pin_sck, int pin_mosi,
                   int pin_cs, int pin_dc, int pin_rst);

/* Fills the whole screen with the given color. */
void ssd1351_fill_screen(ssd1351_t *dev, uint16_t color);

/* Fills the rectangle (x0,y0)-(x1,y1) with the given color. */
void ssd1351_fill_rect(ssd1351_t *dev, int x0, int y0, int x1, int y1, uint16_t color);

/* Draws a single pixel. */
void ssd1351_draw_pixel(ssd1351_t *dev, int x, int y, uint16_t color);

/* Draws into the framebuffer only (not sent immediately, for partial updates) */
void ssd1351_fb_set(int x0, int y0, int x1, int y1, uint16_t color);

/* Sends only the given region to the screen (fast partial refresh) */
void ssd1351_flush_rect(ssd1351_t *dev, int x0, int y0, int x1, int y1);