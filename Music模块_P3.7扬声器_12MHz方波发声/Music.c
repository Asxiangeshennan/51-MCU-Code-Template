#include "Music.h"
#include <REGX51.H>
#include "Delay.h"

/* ---------- 硬件映射与发声原理 ----------
 * 扬声器 LS1：P3.7（引脚17）接扬声器正端，负端接地
 * 定时器0每半个音周期翻转一次 P3.7，得到频率 f 的方波信号
 * 半周期（定时计数）= 500000/f（us）@12MHz（1个机器周期=1us）
 * 初值 = 65536 - 半周期
 *-----------------------------------------------------*/
#define SPEAKER P3_7

/* ---------- C调中音区 do~si 定时器0初值表 ----------
 * f = 261.63/293.66/329.63/349.23/392.00/440.00/493.88 Hz
 *-----------------------------------------------------*/
static code unsigned int NoteReload[8] = {
    0,        /* 下标0：休止/停止（不查该下标发声） */
    63625,    /* 音阶1 do  261.63Hz 0xF889 */
    63833,    /* 音阶2 re  293.66Hz 0xF979 */
    64019,    /* 音阶3 mi  329.63Hz 0xFA13 */
    64104,    /* 音阶4 fa  349.23Hz 0xFA68 */
    64260,    /* 音阶5 so  392.00Hz 0xFB04 */
    64400,    /* 音阶6 la  440.00Hz 0xFB90 */
    64524     /* 音阶7 si  493.88Hz 0xFC0C */
};

static volatile unsigned char note_idx = 0;   /* 当前发声音阶号（0=静音），中断查表用 */

/* 定时器0中断（发声）：按当前音阶补装初值并翻转扬声器电平 */
void Timer0_Routine(void) interrupt 1
{
    unsigned int r = NoteReload[note_idx];
    TL0 = (unsigned char)r;
    TH0 = (unsigned char)(r >> 8);
    SPEAKER = !SPEAKER;
}

void Music_Init(void)
{
    note_idx = 0;
    SPEAKER = 0;          /* 初始静音 */

    TMOD = (TMOD & 0x0F) | 0x01;   /* 定时器0，方式1（16位）；高半字节保留定时器1配置 */
    TF0 = 0;
    TR0 = 0;              /* 音阶键未按下时不启动，发声时机由 Music_Note 决定 */
    ET0 = 1;              /* 允许定时器0中断（EA总开关由 main 打开） */
}

void Music_Note(unsigned char n)
{
    unsigned int r;

    if (n > 7) return;

    if (n == 0)                     /* 停止发声/休止 */
    {
        TR0 = 0;                    /* 关定时器，扬声器保持静止电平 */
        SPEAKER = 0;
        note_idx = 0;
    }
    else if (n != note_idx)         /* 音阶变化才重装初值，持续按键时不会反复触发 */
    {
        TR0 = 0;                    /* 先停表，保证装载过程不被中断打断 */
        note_idx = n;
        r = NoteReload[n];
        TL0 = (unsigned char)r;
        TH0 = (unsigned char)(r >> 8);
        TR0 = 1;                    /* 启动，此后中断内自动补装初值并翻转电平 */
    }
}

/* ---------- 内置自选乐曲《小星星》：音符与拍数成对存储 ----------
 * 表中每两个字节为一组 {音符, 拍数}，一行一条乐句，对照简谱直读：
 *   音符：1=do 2=re 3=mi 4=fa 5=so 6=la 7=si（本曲未用 si）
 *         0=乐曲结束标志（其后不再有内容）
 *   拍数：1=四分音符，2=二分音符
 * 与音符、时值分列两张表相比，成对存储不会发生两表下标错位
 *-----------------------------------------------------*/
static code unsigned char Song[] = {
    1,1, 1,1, 5,1, 5,1, 6,1, 6,1, 5,2,    /* do do so so la la so~ */
    4,1, 4,1, 3,1, 3,1, 2,1, 2,1, 1,2,    /* fa fa mi mi re re do~ */
    5,1, 5,1, 4,1, 4,1, 3,1, 3,1, 2,2,    /* so so fa fa mi mi re~ */
    5,1, 5,1, 4,1, 4,1, 3,1, 3,1, 2,2,    /* so so fa fa mi mi re~ */
    1,1, 1,1, 5,1, 5,1, 6,1, 6,1, 5,2,    /* do do so so la la so~ */
    4,1, 4,1, 3,1, 3,1, 2,1, 2,1, 1,2,    /* fa fa mi mi re re do~ */
    0,0                                    /* 结束标志 */
};

#define BEAT_MS    400   /* 一拍（四分音符）时长 400ms */
#define GAP_MS     20    /* 相邻音符之间的短停顿，乐句更清晰 */

void Music_PlaySong(void)    /* 阻塞式整曲播放：期间主循环不扫描键盘，其他按键无效 */
{
    unsigned char i = 0;

    while (Song[i] != 0)                               /* 未到结束标志 */
    {
        Music_Note(Song[i]);                           /* 发出该音符 */
        DelayMs((unsigned int)Song[i + 1] * BEAT_MS);  /* 按拍长持续发声 */
        Music_Note(0);                                 /* 音符间隙（下一音符前重置状态） */
        DelayMs(GAP_MS);
        i += 2;                                        /* 跳到下一组 {音符,拍数} */
    }

    Music_Note(0);     /* 乐曲结束，静音 */
}
