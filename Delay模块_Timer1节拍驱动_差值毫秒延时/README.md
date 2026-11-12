# Delay模块_Timer1节拍驱动_差值毫秒延时

阻塞式毫秒延时模块：核心是 3 行"差值法"，在 `unsigned int` 回绕时**自动正确**，误差 ≤ ±1ms。换晶振只改两个字节，换定时器只改两处函数名，还能一路升级成非阻塞软定时器、µs 级延时。

> 配套文章：[51单片机差值法毫秒延时_Timer1节拍驱动模块的设计与实现（CSDN）](https://blog.csdn.net/2503_93333717/article/details/170548684)

## 模块信息

| 项目 | 说明 |
|------|------|
| 对外接口 | `void DelayMs(unsigned int ms)` —— 阻塞延时约 ms 毫秒 |
| 占用引脚 | **无**（GPIO 占用为零） |
| 占用片上资源 | Timer1 + ET1/EA + 约 5B RAM（中断号 3 / 向量 0x1B，由节拍源产生，本模块本体不碰任何寄存器） |
| 依赖 | `Timer1.h` —— **节拍源不在本仓库中**，需自备定时器模块，只要求两个接口，见下文「节拍源要求与参考实现」 |
| 前置条件 | **必须先 `Timer1_Init()` 再 `DelayMs()`**，否则节拍不推进、程序卡死（已写进 `Delay.h` 注释） |
| 上限 | 参数为 `unsigned int`，单次延时 ≤ 65535 ms（≈65.5 s），更长请外层套循环 |

配套模块：[KEY模块_P1.4~P1.7_按下为低](../KEY模块_P1.4~P1.7_按下为低/README.md) —— 按键消抖正是用本模块的 `DelayMs(20)` 实现。

---

## 一、源码

### Delay.h（全文）

```c
#ifndef __DELAY_H__
#define __DELAY_H__

/* Delay.c 延时模块：基于 Timer1 的 1ms 定时节拍实现（参考实验报告一）
 * 使用前须已调用 Timer1_Init()，否则节拍不推进、延时不返回 */

void DelayMs(unsigned int ms);   /* 阻塞延时约 ms 毫秒 */

#endif
```

头文件就是模块的说明书：**前置条件直接写在注释里**，是"拷走就能用"模块的基本修养。

### Delay.c（全文）

```c
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
```

`Delay.c` 里没有 `sfr`、`sbit`、没有任何寄存器操作 —— Keil C51 / SDCC 通吃，天然跨编译器；与具体定时器绑定的地方只有 `Timer1_GetTick()` 这一个函数名（出现两处）。

### 节拍源要求与参考实现（不在本仓库）

本模块只要求节拍源对外提供两个接口：

```c
void Timer1_Init(void);             /* 启动 1ms 节拍 */
unsigned int Timer1_GetTick(void);  /* 读"开机至今"的毫秒数 */
```

12MHz 参考实现（可整段照抄；每行为什么这么写见「六、寄存器速查」）：

```c
/* Timer1.h */
#ifndef __TIMER1_H__
#define __TIMER1_H__

void Timer1_Init(void);             /* 启动 1ms 节拍 */
unsigned int Timer1_GetTick(void);  /* 读"开机至今"的毫秒数 */

#endif
```

```c
/* Timer1.c —— 12MHz 版 */
#include <REGX52.H>
#include "Timer1.h"

static unsigned int tick = 0;

#define T1_RELOAD_H 0xFC    /* 65536-1000 = 0xFC18 → 12MHz 下 1ms */
#define T1_RELOAD_L 0x18

void Timer1_Init(void)
{
    TMOD &= 0x0F;           /* 只动 T1 的 4 位，不影响 Timer0 */
    TMOD |= 0x10;           /* T1 模式 1：16 位定时 */
    TH1 = T1_RELOAD_H;      /* 进 ISR 前先装好初值 */
    TL1 = T1_RELOAD_L;
    ET1 = 1;                /* 允许 T1 中断 */
    EA  = 1;                /* 开总中断 */
    TR1 = 1;                /* 启动 */
}

void Timer1_ISR(void) interrupt 3   /* T1 中断号 3，向量 0x1B */
{
    TH1 = T1_RELOAD_H;      /* 模式 1 不会自动重装，必须手动补 */
    TL1 = T1_RELOAD_L;
    tick++;                 /* 一个节拍 = 1ms；ISR 越短越好 */
}

unsigned int Timer1_GetTick(void)
{
    unsigned int t;
    EA = 0;                 /* 16 位变量读写不是原子的，关中断保完整 */
    t  = tick;
    EA = 1;
    return t;
}
```

---

## 二、差值法：3 行核心代码为什么"回绕也不怕"

核心逻辑里藏着本模块最关键的设计：**为什么比较"流逝差值"，而不是比较"当前时刻 ≥ 目标时刻"？**

### 1. 绝对时刻比较的经典事故

`tick` 是 `unsigned int`，每 65.536 s 回绕归零。直觉写法是"先算目标时刻，再死等"：

```c
unsigned int start  = Timer1_GetTick();
unsigned int target = start + ms;        /* 例：start=65530, ms=10 → 65540 溢出截断成 4 */
while (Timer1_GetTick() < target)        /* 入口时 tick=65530，65530 < 4 不成立 */
    ;
```

只要目标时刻跨过回绕点，`start + ms` 就被截断成一个"很小"的数，循环**第一次判断就为假 —— 延时直接消失**。这不是小概率 bug：程序每跑 65.5 秒，tick 必然回绕一圈。

### 2. 差值法为什么天然免疫

C 标准规定：**无符号整数运算永远按模 2^N 进行**（不存在"溢出"这个概念）。所以只要两次读 tick 之间的真实流逝时间小于 65536 ms，`now - start` 的模运算结果就**恒等于**真实流逝的毫秒数 —— 与 tick 转了几圈无关。

拿上面的事故现场重演一遍（`start = 65530`，`ms = 10`）：

| 事件 | tick 实际值 | `now - start`（模 65536） | 真实流逝 |
|------|------------|--------------------------|---------|
| 读取 start | 65530 | 0 | 0 ms |
| 5 ms 后 | 65535 | 5 | 5 ms |
| 6 ms 后 | **0（回绕）** | 6 | 6 ms |
| 10 ms 后 | 4 | **10 ≥ 10，退出** | 10 ms ✅ |

回绕瞬间差值照样平滑爬升，不需要任何特殊分支处理。

两个推论：

- 只要单次延时 ≤ 65535 ms（一个回绕周期内），`now - start` 就在 `[0, 65535]` 内单调递增、不会中途归零 —— 循环既不会提前退出，也不会迟到。这也是 `DelayMs` 参数用 `unsigned int`（上限 65535 ms ≈ 65.5 s）刚刚好的原因；
- 这套"差值 + 回绕"逻辑可原样复制到按键长按判定（见 [KEY README 的短按/长按判定](../KEY模块_P1.4~P1.7_按下为低/README.md)）、串口帧超时、心跳掉线检测等一切"流逝时间"场景。

### 3. 精度账

节拍粒度是 1 ms，`start` 的读取相位随机，所以实际延时落在 `[ms − 1ms, ms + 1ms]` 区间内，**误差不超过一个节拍**。对比软件空转延时动辄 ±10% 的"开盲盒"（魔法循环常数依赖编译器版本与优化等级，换晶振全作废），这是数量级的提升。

---

## 三、移植指南（怎么修改一下就能套用）

以下假设你新建了一个 Keil C51 工程，想把本模块搬进去。

### 步骤 1：拷文件、加头文件搜索路径

把 `Delay.c/.h` 与你的 `Timer1.c/.h` 一起拖入工程，并在 `Options for Target → C51 → Include Paths` 里加上它们所在的目录。

### 步骤 2：对准节拍源 —— 换定时器只需改两处

`Delay.c` 里只有两行与具体定时器绑定：

```c
unsigned int start = Timer1_GetTick();
while ((unsigned int)(Timer1_GetTick() - start) < ms)
```

想改用 **Timer0** 提供节拍？把这两行的 `Timer1_GetTick()` 换成 `Timer0_GetTick()`、第 2 行的 `#include "Timer1.h"` 换成 `"Timer0.h"` 即可，算法本身一个字都不用动。这就是"取节拍函数"作为唯一耦合点的价值。

### 步骤 3：对准晶振 —— 改重装初值

1ms 节拍的初值随晶振变化（模式 1，16 位定时器，机器周期 = 12 / 晶振频率）：

| 晶振 | 计数一次耗时 | 1ms 需要的计数 | 重装初值（THx/TLx） |
|------|-------------|----------------|---------------------|
| 12 MHz | 1 µs | 1000 | `0xFC` / `0x18`（65536−1000 = 64536 = 0xFC18） |
| 11.0592 MHz | ≈1.085 µs | ≈922 | `0xFC` / `0x66`（65536−922 = 64614 = 0xFC66） |

在 `Timer1_Init()` 与 ISR 的重装代码里同步修改这两个字节即可。

### 步骤 4：对准精度需求 —— 改节拍周期

ISR 里若把重装值改为 65536−50000，节拍就变成 50ms/次；此时只要把 `Delay.c` 中的比较改成：

```c
while ((unsigned int)(Timer1_GetTick() - start) < ms / 50) ;   // 单位换算
```

更推荐的做法：定义 `#define TICK_MS 1`，让换算集中在一处（注意用"tick 数向上取整"的写法，避免小延时被整除截断为 0）：

```c
#define TICK_MS  1                       /* 一个节拍 = 1ms，换晶振/换重装值时只改这里 */
while ((unsigned int)(Timer1_GetTick() - start) < (ms + TICK_MS - 1) / TICK_MS)
    ;
```

### 步骤 5：对准编译器 —— 关键字差异

| 写法（Keil C51） | SDCC 等价写法 |
|------------------|---------------|
| `sbit / sfr` | `__sbit __at 0xA0` / `__sfr __at 0x80` |
| `code` 数组常量 | `__code` → `__flash`（部分型号） |
| `void main(void)` + `while(1)` | 相同，但 ISR 用 `__interrupt(n)` |

`Delay.c` 不含任何关键字相关的硬件代码，**天然跨编译器**；需要动的只有节拍源（`Timer1.c` 的 ISR 语法）。

### 步骤 6：裸机直接套用，不必改造

`DelayMs` 是纯阻塞语义，适合流水灯、数码管演示、蜂鸣器节奏这类"时间就是业务"的场景，拷过去就能用，无需操作系统或调度器。

---

## 四、扩展路线（改哪里能得到什么）

每一节给出"改哪里 → 得到什么"。

### 1. 非阻塞软定时器（最值得做的升级）

把"死等"改成"到期判断"，新增一个函数即可：

```c
/* Delay.h 追加 */
unsigned char DelayCheck(unsigned int start, unsigned int ms);
/* Delay.c 追加：到期返回 1；利用回绕差值，天然支持溢出 */
unsigned char DelayCheck(unsigned int start, unsigned int ms)
{
    return (unsigned int)(Timer1_GetTick() - start) >= ms;
}
```

应用层写法（注意 `t` 必须在主循环**外层**初始化一次，否则每圈都重置、永远不到期）：

```c
unsigned int t = Timer1_GetTick();     // 循环外初始化
while (1)
{
    if (DelayCheck(t, 500)) {
        P2 ^= 0x01;                    // 每 500ms 翻转 LED
        t = Timer1_GetTick();          // 重新计时
    }
    // 这 500ms 里 CPU 可以去扫按键、刷新数码管 —— 单核也能"并发"
}
```

### 2. 微秒级延时

两种改法任选：

- **自动重装法（Timer1 模式 2，推荐）**：把 Timer1 切到 8 位自动重装，12MHz 下初值 `TH1 = TL1 = 256−12 = 0xF4`，每溢出一次恰好 12µs；ISR 里累加的就不再是"毫秒 tick"而是"12µs 计数"，`DelayUs(us)` 照抄差值逻辑、把比较阈值换成 `us / 12` 即可（无需软件重装，抖动更小）；
- **双计数器法**：粗计时用 1ms 节拍、细分段用 TLx 当前值插值，可做几 µs ~ 十几 µs 的精确定时（DS18B20 时序够用）。

> ⚠️ 注意：Timer1 一旦被挪去做 µs 延时，就不能同时当 1ms 节拍源了。两者都要用时，请让 Timer1 专职做节拍、另找 Timer0（或 Timer2）做 µs 延时。

### 3. 超时退出 / 带喂狗的延时

在 `while` 循环里塞入副作用：

```c
while ((unsigned int)(Timer1_GetTick() - start) < ms)
{
    WDT_Restart();      // STC 系列清看门狗
    // 或在这里轮询串口接收缓冲，防止阻塞期间丢数据
}
```

### 4. 倒计时显示

`Timer1_GetTick() - start` 本身就是"已流逝毫秒数"，拿出去就是现成的倒计时 / 正计时：

```c
unsigned int elapsed = Timer1_GetTick() - start;   // 送数码管显示剩余 ms
```

### 5. 把同一套差值法推广到其它场景

差值 + 无符号回绕是个通用模式，可以原样复制成：

- **按键长按判定**：记录按下时刻，`elapsed > 1000` 判为长按（KEY 模块已给出完整实现，见 [KEY README 的短按/长按判定](../KEY模块_P1.4~P1.7_按下为低/README.md)）；
- **通讯帧超时**：UART 收到首字节后，`elapsed > 50` 且无新字节则判帧结束；
- **心跳 / 掉线检测**：`now - lastRx > 3000` 视为链路断开。

### 6. 精度再进一步的两个小改动

1. ISR 中先重装初值再做其它事，并把节拍累加放在重装之后，减少抖动；
2. 若单次定时超过 65535 个计数，改为"溢出次数 × 溢出周期 + TLx 残值"的组合计时（见上文「微秒级延时」的双计数器法）。

---

## 五、使用边界（防坑清单）

1. **调用顺序**：必须 `Timer1_Init()` 之后再调 `DelayMs()`，否则节拍不推进、程序卡死（已在 `Delay.h` 注释中标明）。
2. **最大延时**：参数为 `unsigned int`，上限 65535 ms（≈65.5 s）；更长请外层套循环或用「非阻塞软定时器」的检查式接口。
3. **回绕安全的前提**：两次读取之间的间隔不超过一个回绕周期 —— tick 每 65.536 s 回绕一次，只要单次延时 ≤ 65535 ms，就始终成立。
4. **中断优先级**：节拍 ISR 务必短（只做重装 + 累加），否则 1ms 精度会被长中断破坏；需要重活请放到主循环。
5. **阻塞语义**：`DelayMs` 期间不响应任务切换；多任务场景请使用「非阻塞软定时器」方案。
6. **临界区**：读 16 位 `tick` 不是原子操作，读到一半被 ISR 打断会拼出错值（最坏偏差 256 ms 量级，而非 ±1ms）。防护应放在**取数函数内部**（`EA=0` 读完整再 `EA=1`，或"连读两次一致"法，见参考实现）；应用层若需连续读多次 tick（如计算时间戳差）同理。`Delay.c` 的回绕差值算法本身无需额外保护 —— 只要读到的 tick 完整一致，差值法就自动正确。

---

## 六、寄存器速查（Timer1 模式 1）

```text
TMOD &= 0x0F; TMOD |= 0x10;   // T1: 模式1，16位定时（只动高 4 位，不影响 Timer0）
TH1 = 0xFC; TL1 = 0x18;       // 12MHz → 1ms（65536−1000 = 0xFC18）
                              // 11.0592MHz → TH1 = 0xFC, TL1 = 0x66
ET1 = 1; EA = 1;              // 开 T1 中断、总中断
TR1 = 1;                      // 启动
// ISR（中断号 3，向量 0x1B）: TF1 硬件自动清零，先重装 TH1/TL1，再 tick++
```

节拍源的三个实现要点：

1. **节拍变量 `tick` 是 16 位，8051 读它需要两条指令**。若读到一半被 ISR 打断，会拼出"新低字节 + 旧高字节"的错值，最坏偏差可达 256 ms 量级。因此 `Timer1_GetTick()` 内部应加一个两条指令长度的短临界区（开销可忽略）：

   ```c
   unsigned int Timer1_GetTick(void)
   {
       unsigned int t;
       EA = 0;      /* 防读撕裂：读完整再开中断 */
       t = tick;
       EA = 1;
       return t;
   }
   ```

2. **模式 1 不会自动重装**，必须在 ISR 里手动补初值，并按"先重装、后累加"的顺序写，减小节拍抖动；
3. **ISR 必须短**：只做"重装 + 自增"，重活一律放回主循环，否则 1ms 精度会被长中断破坏。
