/*
 * digit7seg.c
 * 7-segment style digit renderer implementation
 *
 * Segment layout (a-g):
 *   ---a---
 *  f       b
 *   ---g---
 *  e       c
 *   ---d---
 */
#include "digit7seg.h"

// Segments lit for each digit (order a,b,c,d,e,f,g, 1 = lit)
static const uint8_t seg_table[10][7] = {
    /*0*/ {1,1,1,1,1,1,0},
    /*1*/ {0,1,1,0,0,0,0},
    /*2*/ {1,1,0,1,1,0,1},
    /*3*/ {1,1,1,1,0,0,1},
    /*4*/ {0,1,1,0,0,1,1},
    /*5*/ {1,0,1,1,0,1,1},
    /*6*/ {1,0,1,1,1,1,1},
    /*7*/ {1,1,1,0,0,0,0},
    /*8*/ {1,1,1,1,1,1,1},
    /*9*/ {1,1,1,1,0,1,1},
};

void digit_draw(int x, int y, int w, int h, int thick, int digit, uint16_t color, uint16_t bg) {
    if (digit < 0 || digit > 9) return;
    const uint8_t *seg = seg_table[digit];
    int half_h = h / 2;

    // Clear the background first (whole digit box)
    ssd1351_fb_set(x, y, x + w - 1, y + h - 1, bg);

    // a: top horizontal
    if (seg[0]) ssd1351_fb_set(x + thick, y, x + w - thick - 1, y + thick - 1, color);
    // b: upper right vertical
    if (seg[1]) ssd1351_fb_set(x + w - thick, y + thick, x + w - 1, y + half_h - 1, color);
    // c: lower right vertical
    if (seg[2]) ssd1351_fb_set(x + w - thick, y + half_h + 1, x + w - 1, y + h - thick - 1, color);
    // d: bottom horizontal
    if (seg[3]) ssd1351_fb_set(x + thick, y + h - thick, x + w - thick - 1, y + h - 1, color);
    // e: lower left vertical
    if (seg[4]) ssd1351_fb_set(x, y + half_h + 1, x + thick - 1, y + h - thick - 1, color);
    // f: upper left vertical
    if (seg[5]) ssd1351_fb_set(x, y + thick, x + thick - 1, y + half_h - 1, color);
    // g: middle horizontal
    if (seg[6]) ssd1351_fb_set(x + thick, y + half_h - thick / 2, x + w - thick - 1, y + half_h + thick / 2 - 1, color);
}

void colon_draw(int x, int y, int h, int dot_size, uint16_t color, uint16_t bg) {
    ssd1351_fb_set(x, y, x + dot_size * 2 - 1, y + h - 1, bg);
    int gap = h / 3;
    ssd1351_fb_set(x, y + gap, x + dot_size - 1, y + gap + dot_size - 1, color);
    ssd1351_fb_set(x, y + h - gap - dot_size, x + dot_size - 1, y + h - gap - 1, color);
}

int digit_total_width(int w, int thick) {
    (void)thick;
    return w;
}
