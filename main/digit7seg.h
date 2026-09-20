/*
 * digit7seg.h
 * 7-세그먼트 스타일 숫자 렌더러 (사각형 조합, 폰트 불필요)
 */
#pragma once
#include <stdint.h>
#include "ssd1351.h"

/* (x,y)를 좌상단으로, w x h 크기의 숫자(0~9)를 프레임버퍼에 그립니다.
   thick: 세그먼트 굵기, color/bg: 켜진/꺼진 색 */
void digit_draw(int x, int y, int w, int h, int thick, int digit, uint16_t color, uint16_t bg);

/* 콜론(:) 을 그립니다 */
void colon_draw(int x, int y, int h, int dot_size, uint16_t color, uint16_t bg);

/* digit_draw 한 칸이 차지하는 총 너비(세그먼트 폭 포함) 계산 */
int digit_total_width(int w, int thick);
