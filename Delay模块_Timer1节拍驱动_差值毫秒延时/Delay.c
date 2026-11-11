#include "Delay.h"
#include "Timer1.h"

/* 阻塞式毫秒延时：时长由定时器节拍给出，最小误差 1ms（软件空转循环无此精度）
 * 差值法在 unsigned int 回绕时自动正确，连续使用互不影响 */
void DelayMs(unsigned int ms)
{
    unsigned int start = Timer1_GetTick();
    while ((unsigned int)(Timer1_GetTick() - start) < ms)
        ;
}
