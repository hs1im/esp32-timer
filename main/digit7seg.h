/*
 * digit7seg.h
 * 7-segment style digit renderer (built from rectangles, no font needed)
 */
#pragma once
#include <stdint.h>
#include "ssd1351.h"

/* Draws a w x h digit (0-9) into the framebuffer with (x,y) as the top-left corner.
   thick: segment thickness, color/bg: lit / unlit color */
void digit_draw(int x, int y, int w, int h, int thick, int digit, uint16_t color, uint16_t bg);

/* Draws a colon (:) */
void colon_draw(int x, int y, int h, int dot_size, uint16_t color, uint16_t bg);

/* Total width of one digit_draw cell (including segment width) */
int digit_total_width(int w, int thick);
