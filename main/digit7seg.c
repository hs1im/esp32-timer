/*
 * digit7seg.c
 * 7-세그먼트 스타일 숫자 렌더러 구현
 *
 * 세그먼트 배치 (a~g):
 *   ---a---
 *  f       b
 *   ---g---
 *  e       c
 *   ---d---
 */
#include "digit7seg.h"

// 각 숫자별로 켜지는 세그먼트 (a,b,c,d,e,f,g 순서, 1=켜짐)
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

    // 배경을 먼저 지웁니다 (숫자 박스 전체)
    ssd1351_fb_set(x, y, x + w - 1, y + h - 1, bg);

    // a: 상단 가로
    if (seg[0]) ssd1351_fb_set(x + thick, y, x + w - thick - 1, y + thick - 1, color);
    // b: 우측 상단 세로
    if (seg[1]) ssd1351_fb_set(x + w - thick, y + thick, x + w - 1, y + half_h - 1, color);
    // c: 우측 하단 세로
    if (seg[2]) ssd1351_fb_set(x + w - thick, y + half_h + 1, x + w - 1, y + h - thick - 1, color);
    // d: 하단 가로
    if (seg[3]) ssd1351_fb_set(x + thick, y + h - thick, x + w - thick - 1, y + h - 1, color);
    // e: 좌측 하단 세로
    if (seg[4]) ssd1351_fb_set(x, y + half_h + 1, x + thick - 1, y + h - thick - 1, color);
    // f: 좌측 상단 세로
    if (seg[5]) ssd1351_fb_set(x, y + thick, x + thick - 1, y + half_h - 1, color);
    // g: 중앙 가로
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
