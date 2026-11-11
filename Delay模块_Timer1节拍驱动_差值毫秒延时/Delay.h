#ifndef __DELAY_H__
#define __DELAY_H__

/* Delay.c 延时模块：基于 Timer1 的 1ms 定时节拍实现（参考实验报告一）
 * 使用前须已调用 Timer1_Init()，否则节拍不推进、延时不返回 */

void DelayMs(unsigned int ms);   /* 阻塞延时约 ms 毫秒 */

#endif
