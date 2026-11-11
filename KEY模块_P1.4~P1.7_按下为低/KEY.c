#include "KEY.h"
#include <REGX51.H>

/* 图1：P1.4~P1.7 依次接 K1(L上移)/K2(L下移)/K3(R上移)/K4(R下移)，低电平=按下 */

#define KEY1 P1_4
#define KEY2 P1_5
#define KEY3 P1_6
#define KEY4 P1_7

unsigned char Key(void)
{
    unsigned char KeyNum = 0;
    if (KEY1 == 0) KeyNum = 1;
    if (KEY2 == 0) KeyNum = 2;
    if (KEY3 == 0) KeyNum = 3;
    if (KEY4 == 0) KeyNum = 4;
    return KeyNum;      /* 多键同按时取最后扫描到的键 */
}
