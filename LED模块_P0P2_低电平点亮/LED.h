#ifndef __LED_H__
#define __LED_H__

/* 图案编号（对应图1布局，低电平点亮） */
enum {
    LED_PAT_OFF = 0,   /* 全灭 */
    LED_PAT_ON,        /* 全亮 */
    LED_PAT_A,         /* 组A：D9 D7 D5 D3 / D11 D13 D15 D17 */
    LED_PAT_B,         /* 组B：D8 D6 D4 D2 / D10 D12 D14 D16 */
    LED_PAT_LEFT,      /* 左列：D2~D9（P0） */
    LED_PAT_RIGHT,     /* 右列：D10~D17（P2） */
    LED_PAT_NUM
};

void LED_SetPattern(unsigned char pat);   /* 显示指定图案 */

#endif
