#ifndef __MUSIC_H__
#define __MUSIC_H__

/* Music.c 发声与乐曲模块：定时器0中断产生方波驱动扬声器（仿真中的 SOUNDER）
 * 电路连接：扬声器 LS1 接 P3.7（引脚17），另一端接地 */

void Music_Init(void);               /* 定时器0初始化（方式1，开ET0；EA由main开） */
void Music_Note(unsigned char n);    /* n: 0=停止/休止，1~7=C调 do~si */
void Music_PlaySong(void);           /* 播放内置乐曲：阻塞式，播放期间按键无效 */

#endif
