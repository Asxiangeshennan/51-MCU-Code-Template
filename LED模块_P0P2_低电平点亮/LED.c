#include "LED.h"
#include <REGX51.H>

/* ---------- 硬件映射（图1，LED 低电平点亮） ----------
 * P0.0~P0.7 -> D9  D8  D7  D6  D5  D4  D3  D2（自上而下）
 * P2.0~P2.7 -> D10 D11 D12 D13 D14 D15 D16 D17（自上而下）
 * 新增图案只需在两张表里各加一个字节，不用再写函数。
 *-----------------------------------------------------*/

static code unsigned char PAT_P0[LED_PAT_NUM] = {
    0xFF,   /* OFF 全灭 */
    0x00,   /* ON  全亮 */
    0xAA,   /* A：P0.0/2/4/6 拉 low 亮 D9 D7 D5 D3 */
    0x55,   /* B：P0.1/3/5/7 拉 low 亮 D8 D6 D4 D2 */
    0x00,   /* 左列全亮 */
    0xFF    /* 右列：P0 全灭 */
};

static code unsigned char PAT_P2[LED_PAT_NUM] = {
    0xFF,   /* OFF 全灭 */
    0x00,   /* ON  全亮 */
    0x55,   /* A：P2.1/3/5/7 拉 low 亮 D11 D13 D15 D17 */
    0xAA,   /* B：P2.0/2/4/6 拉 low 亮 D10 D12 D14 D16 */
    0xFF,   /* 左列：P2 全灭 */
    0x00    /* 右列全亮 */
};

void LED_SetPattern(unsigned char pat)
{
    if (pat >= LED_PAT_NUM) pat = LED_PAT_OFF;   /* 越界保护 */
    P0 = PAT_P0[pat];
    P2 = PAT_P2[pat];
}
