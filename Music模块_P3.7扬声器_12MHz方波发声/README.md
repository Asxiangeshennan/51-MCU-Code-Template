# Music模块_P3.7扬声器_12MHz方波发声

方波发声模块（中厚模块）：Timer0 中断按"半周期翻转"在 P3.7 上生成音调方波，`Music_Note()` 发 C 调中音 do~si（0 = 静音），`Music_PlaySong()` 播放内置《小星星》。音准收敛在一张初值表、乐曲收敛在一张乐谱表 —— **换曲子、调音域都只改数据，不改函数**。12MHz 定标（1 机器周期 = 1µs），换晶振必须重算初值表。

> 配套文章：[51单片机扬声器方波发声_P3.7定时器中断驱动模块的设计与实现（CSDN）](https://blog.csdn.net/2503_93333717/article/details/170719104)

## 模块信息

| 项目 | 说明 |
|------|------|
| 对外接口 | `void Music_Init(void)` —— Timer0 初始化；`void Music_Note(unsigned char n)` —— 发 n 号音阶（0=停止/休止，1~7=C 调 do~si）；`void Music_PlaySong(void)` —— 阻塞式播放内置《小星星》 |
| 占用引脚 | **P3.7**（DIP40 第 17 脚），接扬声器 / 无源蜂鸣器；该脚复用外部总线 **WR** 信号，常规最小系统无碍 |
| 占用片上资源 | **Timer0**（模式 1，16 位定时，中断号 1 / 向量 0x0B）+ ET0/EA + 约 4B RAM + 约 102B ROM（音阶初值表 16B + 乐谱表 86B，`code` 区） |
| 依赖 | `Delay.h` —— **仅 `Music_PlaySong()` 需要**；`Music_Init()` / `Music_Note()` 零依赖 |
| 前置条件 | ① **先 `Music_Init()` 再发声**；② **EA 必须由 main 置 1**（`Music.h` 注释写明）；③ 调 `Music_PlaySong()` 前 **Delay 的节拍源必须已就绪**（`Timer1_Init()` 先行），否则 `DelayMs` 永不返回 |
| 音准定标 | **12MHz**：1 机器周期 = 1µs，初值 = 65536 − 500000/f；换晶振必须重算初值表，否则整体跑调（见移植指南步骤 3） |

配套模块：[Delay模块_Timer1节拍驱动_差值毫秒延时](../Delay模块_Timer1节拍驱动_差值毫秒延时/README.md) —— 整曲播放的节拍来源（单音发声不依赖它）；[KEY模块_P1.4~P1.7_按下为低](../KEY模块_P1.4~P1.7_按下为低/README.md) —— 四键弹琴的最小组合示范见仓库根 README「模块组合 · 场景 2」。

---

## 一、接线与发声原理

### 1.1 接线与器件选择

扬声器 LS1（Proteus 仿真里的 SOUNDER 元件）**一端接 P3.7（引脚17），另一端接 GND** —— 这是源码注释标注的原始接法，仿真里直接可用。

实机搭电路时有三件事必须知道：

1. **必须用无源蜂鸣器或扬声器**：有源蜂鸣器内置振荡电路，通电即响固定音，**发不了音阶** —— 这是"按键弹琴"类实验最常见的坑；
2. **直驱音量很小**：8051 准双向口拉电流只有几百 µA、灌电流几 mA，直接推扬声器响度有限。标准做法是 P3.7 → 1k~4.7k 电阻 → NPN 三极管（S8050）基极，蜂鸣器/扬声器接在 VCC 与集电极之间，发射极接 GND，方波经三极管放大后驱动；
3. **P3.7 复用外部总线的 WR 信号**：常规最小系统无碍；工程里要用外部 RAM / 总线扩展时，这个脚要给 WR 让位。

### 1.2 方波发声：半周期翻转法

音调由方波频率决定。生成办法：**定时器每"半个音周期"中断一次，中断里把 P3.7 电平翻转一次** —— 引脚上就得到频率为 f 的方波，直到下一个音阶改变装载。

12MHz 下 1 个机器周期 = 1µs，半周期计数 = 500000/f（µs），**初值 = 65536 − 500000/f**：

| 音阶（n） | 频率 (Hz) | 半周期计数 | 初值（十进制） | 初值（十六进制） |
|-----------|-----------|-----------|----------------|------------------|
| 0 | —— | —— | 0（休止占位，不用于发声） | —— |
| 1 do | 261.63 | 1911 | 63625 | `0xF889` |
| 2 re | 293.66 | 1703 | 63833 | `0xF959`* |
| 3 mi | 329.63 | 1517 | 64019 | `0xFA13` |
| 4 fa | 349.23 | 1432 | 64104 | `0xFA68` |
| 5 so | 392.00 | 1276 | 64260 | `0xFB04` |
| 6 la | 440.00 | 1136 | 64400 | `0xFB90` |
| 7 si | 493.88 | 1012 | 64524 | `0xFC0C` |

\* 源码注释里 re 写的是 `0xF979`，属笔误；装载以十进制 **63833 = `0xF959`** 为准（65536 − 1703），不影响功能。

**为什么用中断而不是主循环翻转？** 主循环里 `SPEAKER = !SPEAKER; DelayUs(...);` 的空转写法，节奏会被任何业务代码打断，音准随主循环忙闲漂移。中断方案里频率完全由定时器初值保证：**主循环爱忙不忙，音准纹丝不动** —— 这是"用硬件资源换确定性"的典型取舍，也和 Delay 模块"时间交给节拍源"的思路一脉相承。

### 1.3 静音的实现：关表，而不是装零

`Music_Note(0)` 做的是 `TR0 = 0`（定时器停跑）+ `SPEAKER = 0`（引脚归零），**不是**把一个"零频率初值"装进去让 ISR 空转。`NoteReload[0]` 里的 0 只是占位 —— 它的作用是把"休止"对齐到下标 0，让 1~7 恰好对应 do~si，方便乐谱表直读。扩表加音阶时，这个"下标 0 = 休止"的约定要保住。

---

## 二、源码解析

### Music.h（全文）

```c
#ifndef __MUSIC_H__
#define __MUSIC_H__

/* Music.c 发声与乐曲模块：定时器0中断产生方波驱动扬声器（仿真中的 SOUNDER）
 * 电路连接：扬声器 LS1 接 P3.7（引脚17），另一端接地 */

void Music_Init(void);               /* 定时器0初始化（方式1，开ET0；EA由main开） */
void Music_Note(unsigned char n);    /* n: 0=停止/休止，1~7=C调 do~si */
void Music_PlaySong(void);           /* 播放内置乐曲：阻塞式，播放期间按键无效 */

#endif
```

三个前置/边界信息全部写在注释里：**EA 由 main 开**、**音阶编码 0~7**、**PlaySong 阻塞**。头文件是模块说明书，这是本仓库一贯的规矩。

### Music.c（全文）

```c
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
```

去掉两张表，核心就是"一个 ISR + 两个控制函数 + 一个播放循环"。六个设计点值得展开。

**① 音准与乐谱分离，各收敛在一张 `code` 表。** `NoteReload[]` 只管"音阶号 → 定时器初值"（乐理 × 硬件计数的换算），`Song[]` 只管"演奏什么、各多长"。换曲子不动音准，调音域不动乐谱 —— 和 LED 模块"图案长在数据表里"是同一种数据驱动思想。

**② `volatile note_idx` + 幂等的 `Music_Note()`。** `note_idx` 被主循环写、ISR 读，必须 `volatile`，否则编译器可能把主循环里的写入优化掉，ISR 查到旧值。更妙的是 `else if (n != note_idx)` 这个判断：**重复发同一个音阶时函数什么都不做** —— 波形不重启、相位连续，听感平滑。副作用是它天然"幂等"：应用层反复调 `Music_Note(3)` 不会反复触发，按键抖动、轮询写法都伤不到它（根 README「模块组合 · 场景 2」的弹琴 demo 因此连消抖都省了）。

**③ 音阶切换的原子性：先停表，再装载。** `TR0 = 0 → 装初值 → TR0 = 1` 这个顺序，保证装载过程不被 ISR 打断（否则旧初值会在装载中途触发一次错拍）。它和 [Delay README 寄存器速查](../Delay模块_Timer1节拍驱动_差值毫秒延时/README.md)里 `Timer1_GetTick()` 的"关中断读 16 位变量"是同一个思想 —— 那里用关中断，这里用停表，因为这里要保护的对象是"初值装载过程"，停表比关总中断影响面更小。

**④ 乐谱 `{音符, 拍数}` 成对存储，从结构上消灭错位。** 音符、时值若分列两张表，往中间插音符时必须两表同步（正是 [LED README 移植步骤 5](../LED模块_P0P2_低电平点亮/README.md)警告过的"两张表下标对齐"问题）。成对存储把"一个音符的两个属性"焊在一个单元里，插删只动一处。`0,0` 是结束标志，`GAP_MS` 的 20ms 短间隙让相邻的同名音符断开（不间隙的话"do do"会连成一个长音）。

**⑤ `Music_PlaySong()` 的阻塞语义诚实外露。** 函数体一进来就是 `while` 死循环 + `DelayMs`，播放期间主循环完全停摆 —— 这一点直接写在 `Music.h` 注释里（"阻塞式，播放期间按键无效"），不藏着。要后台播放见扩展路线 4.1。

**⑥ `Music_Init()` 只动 TMOD 的低半字节。** `TMOD = (TMOD & 0x0F) | 0x01` 只改 T0 的 4 位、保留 T1 配置 —— 与 Delay 参考实现里 `TMOD &= 0x0F; TMOD |= 0x10;`（只动高 4 位）正好互补。**两个模块各自只碰自己的半字节，同场工作互不破坏**，这不是巧合，是写法约定里"硬件绑定点必须收敛"的直接体现。

---

## 三、移植指南（怎么修改一下就能套用）

以下假设你新建了一个 Keil C51 工程，想把本模块搬进去。

### 步骤 1：拷文件、理清初始化顺序

把 `Music.c/.h` 拖进工程，加 Include Paths。`Music_Note()` 单独用只需 `Music.c/.h`；要用 `Music_PlaySong()` 整曲播放，还需 `Delay.c/.h` 与你的 `Timer1.c/.h`（接口要求见 [Delay README](../Delay模块_Timer1节拍驱动_差值毫秒延时/README.md)）。

main 里的初始化顺序：

```c
Timer1_Init();      // ① 节拍源（PlaySong 需要；EA 通常在这里已开）
Music_Init();       // ② 发声初始化（TMOD/ET0）
EA = 1;             // ③ 总中断 —— Music.h 写明由 main 打开；若 Timer1_Init 已开过，重复置 1 无害
```

### 步骤 2：换引脚 —— 只改 1 行 `#define`

整个文件与引脚相关的只有 `#define SPEAKER P3_7` 一行（外加注释）。比如换到 P2.0 之外的空闲脚：

```c
#define SPEAKER P1_0    /* 若换到 P1.0，注意它复用 Timer2 的 T2 脚 */
```

头文件没有现成 `sbit` 别名时自己补一行：`sbit SPEAKER = P1^0;`。选脚提示：避开 P3.0/P3.1（下载）、P3.6/P3.7（WR/RD，用外部总线时）、P1.0/P1.1（Timer2 复用）。

### 步骤 3：换晶振 —— 重算初值表

初值公式随机器周期变化：

```text
12MHz：      初值 = 65536 − 500000/f            （1 机器周期 = 1µs）
11.0592MHz： 初值 = 65536 − (500000/f) × 11.0592/12 （1 机器周期 ≈ 1.085µs）
```

11.0592MHz 下的整套初值（计数按四舍五入，音准误差 < 0.05%，听不出来）：

| 音阶 | 频率 (Hz) | 计数 | 初值（十进制） | 初值（十六进制） |
|------|-----------|------|----------------|------------------|
| 1 do | 261.63 | 1762 | 63774 | `0xF91E` |
| 2 re | 293.66 | 1569 | 63967 | `0xF9DF` |
| 3 mi | 329.63 | 1398 | 64138 | `0xFA8A` |
| 4 fa | 349.23 | 1320 | 64216 | `0xFAD8` |
| 5 so | 392.00 | 1176 | 64360 | `0xFB68` |
| 6 la | 440.00 | 1047 | 64489 | `0xFBE9` |
| 7 si | 493.88 | 933 | 64603 | `0xFC5B` |

**不换表的后果**：11.0592MHz 的板子直接套 12MHz 表，所有音整体偏低约 8% —— 超过半个音，一听就跑调，不是"稍微不准"。

### 步骤 4：改音域 / 加音阶

低八度 = 频率 ÷2，高八度 = 频率 ×2，初值同公式重算。两个参照点（12MHz）：

- 低音 do（130.81Hz）：计数 3822 → 初值 61714 = `0xF112`；
- 高音 do（523.25Hz）：计数 956 → 初值 64580 = `0xFC44`。

扩表做法：`NoteReload[]` 加表项（建议编号规则：1~7 中音、11~17 低音、21~27 高音），`Music_Note()` 的 `if (n > 7) return;` 改成新上限，乐谱表里的音符号即可直写新编码。边界提醒：16 位初值决定频率下限约 7.6Hz（500000/65535），音频范围内下限无忧；上限几 kHz 内音准无忧，更高音域会受 ISR 开销（进中断 + 查表 + 重装 + 翻转，约 20~30 机器周期）挤压，音准开始劣化。

### 步骤 5：换曲子 —— 只改乐谱表，一个函数都不动

对照简谱，每两个字节一组 `{音符, 拍数}` 直写进 `Song[]`，`0,0` 收尾。拍数：1 = 四分音符，2 = 二分音符；速度改 `BEAT_MS`，顿挫改 `GAP_MS`。以《两只老虎》开头为例：

```c
static code unsigned char Song_Tiger[] = {
    1,1, 2,1, 3,1, 1,1,   1,1, 2,1, 3,1, 1,1,   /* 两只老虎 两只老虎 */
    3,1, 4,1, 5,2,        3,1, 4,1, 5,2,       /* 跑得快 跑得快 */
    0,0
};
```

带附点、休止符的曲子也能表达：休止就是 `{0, 拍数}`（`Music_Note(0)` 正好是静音语义）；附点音符拆成"长音 + 短音同音相连"或把拍数放宽到更多档（改 `BEAT_MS` 粒度）。

### 步骤 6：换编译器

Keil C51 的 `code` 在 SDCC 写作 `__code`；`void Timer0_Routine(void) interrupt 1` 在 SDCC 写作 `void Timer0_Routine(void) __interrupt(1)`（中断号相同）；`sbit` 别名缺失自补。其余无关键字依赖。

---

## 四、扩展路线（改哪里能得到什么）

### 4.1 非阻塞点歌（最值得做的升级：播放期间主循环是活的）

`Music_PlaySong()` 阻塞的根源是"用延时换拍子"。非阻塞版改成**状态机 + 毫秒节拍**：发声阶段与间隙阶段交替推进，主循环每圈调一次 `Music_PlayTick()`，到点自动切下一音（需节拍源 `Timer1_GetTick()`，接口要求见 [Delay README](../Delay模块_Timer1节拍驱动_差值毫秒延时/README.md)）：

```c
/* Music.h 追加 */
void Music_StartSong(void);      /* 开始后台播放内置乐谱 */
void Music_StopSong(void);       /* 停止后台播放并静音 */
void Music_PlayTick(void);       /* 主循环每圈调一次：到点自动推进乐谱 */

/* Music.c 追加（需 #include "Timer1.h"） */
static unsigned char songIdx = 0xFF;   /* 乐谱游标：0xFF = 空闲 */
static unsigned char songPh  = 0;      /* 0=发声阶段，1=间隙阶段 */
static unsigned int  songT0  = 0;      /* 当前阶段起点时刻 */
static unsigned int  songDur = 0;      /* 当前阶段时长（ms） */

void Music_StartSong(void)
{
    songIdx = 0;                       /* 从乐谱头开始 */
    songPh  = 1;                       /* 先走一个 GAP 间隙，再起第一音 */
    songDur = GAP_MS;
    songT0  = Timer1_GetTick();
}

void Music_StopSong(void)
{
    Music_Note(0);
    songIdx = 0xFF;
}

void Music_PlayTick(void)
{
    unsigned int now = Timer1_GetTick();

    if (songIdx == 0xFF) return;                        /* 空闲 */
    if ((unsigned int)(now - songT0) < songDur) return; /* 当前阶段未到点 */

    if (songPh == 0)                    /* 发声阶段结束 → 消音进间隙 */
    {
        Music_Note(0);
        songPh  = 1;
        songDur = GAP_MS;
        songT0  = now;
    }
    else                                /* 间隙结束 → 推进到下一音符 */
    {
        if (Song[songIdx] == 0)         /* 曲终 */
        {
            Music_Note(0);
            songIdx = 0xFF;
            return;
        }
        Music_Note(Song[songIdx]);
        songPh  = 0;
        songDur = (unsigned int)Song[songIdx + 1] * BEAT_MS;
        songT0  = now;
        songIdx += 2;
    }
}
```

应用层从此可以"边放歌边干别的"：

```c
Music_StartSong();
while (1)
{
    Music_PlayTick();      /* 推进乐谱（未到点时瞬间返回） */
    /* 按键、刷 LED、扫数码管…… 全都不受播放影响；按下某键还能 Music_StopSong() 打断 */
}
```

代价要清楚：多约 6B RAM（四个 `static`），且后台版依赖节拍源。差值法保证 tick 回绕也正确（原理见 [Delay README 第二节](../Delay模块_Timer1节拍驱动_差值毫秒延时/README.md)）。

### 4.2 曲库：把乐谱从"写死"变"传参"

`Music_PlaySong()` 把 `Song[]` 焊死在函数里。加一个带参版本，旧接口原样保留（接口演进的老规矩：加新函数，别动旧接口）：

```c
/* Music.h 追加 */
void Music_PlaySongOf(unsigned char code *song);   /* 播放指定乐谱（阻塞） */

/* Music.c：把原函数的循环体搬进来，Song[] 换成形参 */
void Music_PlaySongOf(unsigned char code *song)
{
    unsigned char i = 0;
    while (song[i] != 0)
    {
        Music_Note(song[i]);
        DelayMs((unsigned int)song[i + 1] * BEAT_MS);
        Music_Note(0);
        DelayMs(GAP_MS);
        i += 2;
    }
    Music_Note(0);
}

void Music_PlaySong(void) { Music_PlaySongOf(Song); }   /* 旧接口不变 */
```

配合 [KEY 模块](../KEY模块_P1.4~P1.7_按下为低/README.md)就能做选曲机（消抖/等松手参考 KEY README 4.1）：

```c
unsigned char key = Key();
if (key == 1) { Music_StopSong(); Music_PlaySongOf(Song); }        /* 键1：小星星 */
if (key == 2) { Music_StopSong(); Music_PlaySongOf(Song_Tiger); }  /* 键2：两只老虎 */
```

### 4.3 音量与音色：占空比可调

当前方波是标准 50% 占空比（响度最大、最饱满）。想调响度或音色，把"一张半周期表"改成**两张表交替装载**：`TON[]`（高电平半周期）与 `TOFF[]`（低电平半周期），ISR 里按当前电平查对应表。`TON / (TON+TOFF)` 即占空比 —— 小占空比音量变小、音色更"尖"，50% 最饱满。改动点：两张表、ISR 里一次条件判断，接口完全不变。

### 4.4 与 KEY / LED 联动：按哪个键，响哪个音、亮哪组灯

```c
unsigned char key = Key();
if (key != 0)
{
    LED_SetPattern(key);    /* 键号直接当图案号用：1=全亮 2=组A 3=组B 4=左列 */
    Music_Note(key);        /* 同时发声 do~fa */
}
else
{
    LED_SetPattern(LED_PAT_OFF);
    Music_Note(0);
}
```

三个模块互相不认识，`main` 只做翻译 —— 完整可跑版本见仓库根 README「模块组合」的两个场景。

### 4.5 定时器让位：谁动了 Timer0 怎么办

本仓库的默认分工是 **Timer0 = Music 发声、Timer1 = Delay 节拍**，天然无冲突。两种常见的让位场景：

- Delay 节拍已按 [Delay README 步骤 2](../Delay模块_Timer1节拍驱动_差值毫秒延时/README.md) 迁到了 Timer0？那 Music 没定时器了 —— 89C52 的 **Timer2** 支持 16 位自动重装（`RCAP2H/RCAP2L`）+ 中断号 5，把 ISR 里的"手动补装初值"换成硬件自动重装，抖动还会更小；`Music_Init()` / `Music_Note()` 里的启停换到 TR2 一组寄存器即可；
- 反过来 Music 不用时，Timer0 自动空出来给数码管扫描、硬件 PWM —— 这就是每个模块 README 都写清"占用片上资源"的意义。

---

## 五、资源占用

| 资源 | 占用情况 |
|------|----------|
| I/O | **P3.7** × 1（DIP40 第 17 脚，复用 WR）；接扬声器 / 无源蜂鸣器 |
| 定时器 | **Timer0**（模式 1，16 位，中断号 1 / 向量 0x0B）；静音时 TR0=0 不空耗 |
| 中断 | ET0 + EA（EA 由 main 置 1） |
| RAM | 约 4B（`note_idx` 1B + 形参/局部）；用 4.1 非阻塞点歌再 +约 6B |
| ROM / Flash | `NoteReload` 16B + `Song` 86B ≈ 102B（`code` 区，随乐谱长度线性增长）+ 百余条指令 |
| 特殊功能寄存器 | TMOD **低半字节**（不碰 T1 配置）、TH0/TL0/TR0/TF0/ET0、P3_7 位 |
| 电流 | 引脚直驱 µA~mA 级（仿真够用、实机音量小）；三极管放大后由发声器件自定 |

与 [Delay](../Delay模块_Timer1节拍驱动_差值毫秒延时/README.md)、[KEY](../KEY模块_P1.4~P1.7_按下为低/README.md)、[LED](../LED模块_P0P2_低电平点亮/README.md) 同时使用：21 个 I/O + **Timer0 + Timer1 双定时器** + ET0/ET1/EA + 约 12B RAM，无资源竞争；但 **Timer2 成了定时器里最后一个空位**（数码管 / 硬件 PWM / 串口波特率都在盯它），后续模块落地前先做让位规划。

---

## 六、使用边界（防坑清单）

1. **必须是无源蜂鸣器 / 扬声器**：有源蜂鸣器内置振荡、通电即响固定音，发不了音阶 —— "弹琴不响、只响一个音"时先查这个。
2. **EA 必须为 1 且不被中途关闭**：`Music.h` 写明由 main 打开。Delay 用户的 `Timer1_Init()` 通常已开 EA，但工程里任何"关中断"代码忘了恢复，Music 会哑火且**无任何报错**。
3. **12MHz 定标**：换晶振必须按步骤 3 重算初值表；11.0592M 板子直套 12M 表会整体偏低约 8%（超过半个音，一听就跑调）。
4. **`Music_PlaySong()` 是阻塞的**：播放期间按键无效、主循环停摆（`Music.h` 已注明）；要边放歌边干活走 4.1。
5. **`note_idx` 是 ISR 与主循环的共享状态**：已用 `volatile` + "先停表再装载"保护；不要再在别的中断服务里调 `Music_Note()`，会打断装载过程。
6. **静音 = `TR0=0` + `SPEAKER=0`**：`NoteReload[0]` 是 1~7 音阶对齐用的占位，不是可发声项；自己扩表时保住"下标 0 = 休止"的约定。
7. **实机直驱音量小**：P3.7 直接推扬声器在仿真里可以，实物建议三极管放大（见 1.1）。
8. **P3.7 复用 WR**：工程用外部总线 / 外部 RAM 时该脚要让位（见 1.1）。

> **模块定位**：仓库里第一个**自带硬件配置层**的模块（初始化 + ISR 收在自己体内），也是第一个**服务层依赖服务层**的模块 —— 且依赖被刻意压到"只有 `PlaySong` 一处"。至此 Delay（时间）、KEY（输入）、LED（画面）、Music（声音）四件套凑齐，剩下的组合都是应用层自己的事。
