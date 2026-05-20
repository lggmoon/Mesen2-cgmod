# Yoko Mapper (264) NTSC/DENDY 修复记录

## 问题描述

Master Fighter VI 游戏在 PAL 模式下正常，NTSC/DENDY 模式下部分花屏。

## 根因分析

### PAL 与 NTSC/DENDY 的时序差异

Yoko mapper 的 IRQ 计数器在 M2 模式下每个 CPU 周期递增/递减一次。不同区域的 CPU/PPU 时钟比率不同，导致每扫描线的 CPU 周期数不同：

| 区域 | CPU 分频 | PPU 分频 | PPU周期/CPU周期 | CPU周期/扫描线 | 帧扫描线数 |
|------|---------|---------|----------------|---------------|-----------|
| NTSC | 12 | 4 | 3.0 | 113.667 | 262 |
| PAL | 16 | 5 | 3.2 | 106.5625 | 312 |
| DENDY | 15 | 5 | 3.0 | 113.667 | 312 |

### 两个叠加因素导致 IRQ 触发位置偏移

1. **每扫描线 CPU 周期差异**：NTSC/DENDY 每扫描线 113.667 周期 vs PAL 的 106.5625 周期，计数器消耗更快
2. **帧长度差异**：NTSC 帧仅 262 扫描线 vs PAL 的 312 扫描线，VBlank 区间短 50 扫描线

### 日志验证

PAL 日志（IRQ 在 scanline 242 设置，计数器 $5F10 = 24336）：
- IRQ FIRED at scanline **158** ✓

NTSC 日志（相同计数器值 24336）：
- IRQ FIRED at scanline **194** ✗（推迟了 36 条扫描线）

计算验证：
```
PAL:  24336 / 106.5625 = 228.4 扫描线 → (242 + 228.4) % 312 = 158 ✓
NTSC: 24336 / 113.667 = 214.1 扫描线 → (242 + 214.1) % 262 = 194 ✗
```

## 修复方案

修复分为两部分，分别处理两个叠加因素：

### 1. ProcessCpuClock：15/16 预分频（补偿每扫描线周期差异）

NTSC/DENDY 每扫描线的 CPU 周期比 PAL 多，比率 = 113.667 / 106.5625 = 16/15。因此每 16 个 CPU 周期跳过 1 次 IRQ 时钟，使有效速率等于 PAL：

```
NTSC 有效每扫描线 tick = 113.667 × 15/16 = 106.5625（= PAL）
```

```cpp
void ProcessCpuClock() override
{
    BaseProcessCpuClock();

    if(!_irqSourceA12 && _irqEnabled && _irqCounter != 0) {
        ConsoleRegion region = _console->GetRegion();
        if(region == ConsoleRegion::Ntsc || region == ConsoleRegion::Dendy) {
            _m2Prescaler = (_m2Prescaler + 1) & 0x0F;
            if(_m2Prescaler == 0) {
                return;  // 每16周期跳过1次
            }
        }
        ClockIrqCounter();
    }
}
```

### 2. WriteRegister 0x8801：扣除 PAL 多出的 50 扫描线计数（补偿帧长度差异）

PAL 帧比 NTSC 多 50 扫描线（312 vs 262）。当 IRQ 计数器在 VBlank 区间（scanline > 242）设置时，PAL 的计数器值包含了这 50 条额外扫描线的周期数。在 NTSC/DENDY 下需要扣除。

关键判断逻辑：
- 仅当计数器设置时当前扫描线 > 242（VBlank 区间）时才扣除
- 仅当计数器值足够大（跨越了 VBlank 区间）时才扣除
- 若计数器值小于 50 扫描线对应的周期数，说明目标在 VBlank 内，直接清零（NTSC 的 VBlank 不够长）

```cpp
case 0x8801:
{
    const uint16_t PAL_50_LINE_CYCLE = (uint16_t)floor(50 * 106.5625);  // = 5328

    _irqEnabled = (_mode & 0x80) != 0;
    _irqCounter = (_irqCounter & 0xFF) | (value << 8);
    ConsoleRegion region = _console->GetRegion();
    if(region == ConsoleRegion::Ntsc || region == ConsoleRegion::Dendy) {
        int32_t scanline = (int32_t)floor(_irqCounter / 106.5625) + _console->GetPpu()->GetCurrentScanline();
        if(scanline > 242) {
            if(_irqCounter < PAL_50_LINE_CYCLE) {
                _irqCounter = 0;
            } else {
                _irqCounter = (uint16_t)floor(_irqCounter - PAL_50_LINE_CYCLE);
            }
        }
    }
    break;
}
```

### 为什么不能无条件扣除 50 扫描线

PAL 下帧内多次触发 IRQ 的场景：

```
IRQ1: scanline 242 设置, 计数器 $3231 → scanline 51 触发（跨帧）
IRQ2: scanline 51 设置,  计数器 $2020 → scanline 129 触发（帧内）
IRQ3: scanline 130 设置, 计数器 $1050 → scanline 169 触发（帧内）
```

IRQ2 和 IRQ3 的设置位置在活跃扫描线区域（scanline 51、130），目标也在活跃区域，不涉及 VBlank 的 50 扫描线差异，因此不应扣除。只有跨帧的 IRQ（设置和触发之间经过 VBlank）才需要扣除。

## 修复效果

- PAL 模式：无变化，保持原有行为
- NTSC/DENDY 模式：IRQ 触发扫描线与 PAL 精确对齐，画面正常

## 涉及文件

- `Core/NES/Mappers/Unlicensed/Yoko.h` — ProcessCpuClock() 和 WriteRegister() case 0x8801
