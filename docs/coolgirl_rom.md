# CoolGirl ROM 内存布局与 CoolEmu 数据提取

## ROM 加载机制

Mesen2 加载 CoolGirl .nes 文件时，整个 flash 芯片内容**全部**装载到 `_prgRom`（PRG ROM）中。
CHR ROM 大小为 0（CoolGirl 硬件只有 CHR RAM），CHR RAM 分配 512KB，初始全零。

| 存储区域 | 内容 | 大小 |
|---------|------|------|
| `_prgRom` | 整个 flash 内容（loader + 所有游戏PRG + 所有游戏CHR） | 可达 128MB |
| `_chrRom` | 空（`_chrRomSize = 0`） | 0 |
| `_chrRam` | 512KB CHR RAM，初始全零 | 512KB |

## Flash 内存布局

```
_prgRom (PRG ROM, 整个flash内容):
┌──────────────────────────────────────┐
│ Loader/Menu (128KB)                  │  offset 0x00000
├──────────────────────────────────────┤
│ Game A PRG data                      │  PrgOffset_A (按游戏大小对齐)
├──────────────────────────────────────┤
│ Game B PRG data                      │  PrgOffset_B
├──────────────────────────────────────┤
│ ...更多游戏 PRG...                    │
├──────────────────────────────────────┤
│ Game A CHR data (flash中)            │  ChrOffset_A (按 0x2000 对齐)
├──────────────────────────────────────┤
│ Game B CHR data (flash中)            │  ChrOffset_B
├──────────────────────────────────────┤
│ ...更多游戏 CHR...                    │
└──────────────────────────────────────┘

注意：PRG 和 CHR 数据在 flash 中是分开存放的！
- PRG 按游戏大小对齐存放（combiner 先放大游戏再放小游戏）
- CHR 按 0x2000 对齐存放
- 同一游戏的 PRG 和 CHR 在 flash 中**不在**相邻位置
```

## CoolGirl 寄存器定义

范围: $5000-$5FFF, Mask: $5007, 上电/复位后全 $00

### $5xx0 — PRG base offset 高位
```
 7  bit  0
 ---- ----
 PPPP PPPP
 |||| ||||
 ++++-++++-- PRG base offset (A29-A22)
```

### $5xx1 — PRG base offset 低位
```
 7  bit  0
 ---- ----
 PPPP PPPP
 |||| ||||
 ++++-++++-- PRG base offset (A21-A14)
```

### $5xx2 — PRG mask + CHR mask 高位
```
 7  bit  0
 ---- ----
 AMMM MMMM
 |||| ||||
 |+++-++++-- PRG mask (A20-A14, inverted+anded with PRG address)
 +---------- CHR mask (A18, inverted+anded with CHR address)
```

### $5xx3 — PRG mode + CHR bank A 高位
```
 7  bit  0
 ---- ----
 BBBC CCCC
 |||| ||||
 |||+-++++-- CHR bank A (bits 7-3)
 +++-------- PRG banking mode
```

### $5xx4 — CHR mode + CHR mask 低位
```
 7  bit  0
 ---- ----
 DDDE EEEE
 |||| ||||
 |||+-++++-- CHR mask (A17-A13, inverted+anded with CHR address)
 +++-------- CHR banking mode
```

### $5xx5 — CHR bank A 低位 + PRG bank A + SRAM page
```
 7  bit  0
 ---- ----
 CDDE EEWW
 |||| ||||
 |||| ||++-- 8KiB WRAM page at $6000-$7FFF
 |+++-++---- PRG bank A (bits 5-1)
 +---------- CHR bank A (bit 8)
```

### $5xx6 — Flags + Mapper code 低位
```
 7  bit  0
 ---- ----
 FFFM MMMM
 |||| ||||
 |||+ ++++-- Mapper code (bits 4-0)
 +++-------- Flags 2-0
```

### $5xx7 — Lockout + Mapper高位 + Mirroring + SRAM
```
 7  bit  0
 ---- ----
 LMTR RSNO
 |||| |||+-- Enable WRAM at $6000-$7FFF
 |||| ||+--- Allow writes to CHR RAM
 |||| |+---- Allow writes to flash chip
 |||+-+----- Mirroring (00=V, 01=H, 10=1Sa, 11=1Sb)
 ||+-------- Enable four-screen mode
 |+--------- Mapper code (bit 5)
 +---------- Lockout bit
```

## 从 Mask 反推游戏大小

### 原理

Combiner (coolgirl-multirom-builder) 计算寄存器时：
```csharp
int prgMask = ~(game.PRG.Length / 0x4000 - 1);   // PRG mask
int chrMask = ~(chrBankingSize / 0x2000 - 1);     // CHR mask
```

反推游戏大小：
```
prgSize / 0x4000 = (~prgMask + 1)  (在有效位范围内)
chrSize / 0x2000 = (~chrMask + 1)  (在有效位范围内)
```

### 代码实现

```cpp
// PRG mask 有效位: A20-A14 (7位), 存储在 _prgMask 的 bit[20:14]
// _prgMask = (_regs[2] & 0x7F) << 14
_gamePrgSize = ((~(_prgMask >> 14) & 0x7F) + 1) * 0x4000;

// CHR mask 有效位: A18(1位) + A17-A13(5位), 共6位
// _chrMask = ((_regs[2] & 0x80) << 11) | ((_regs[4] & 0x1F) << 13)
_gameChrSize = ((~(_chrMask >> 13) & 0x3F) + 1) * 0x2000;
```

### 计算示例

| 游戏 PRG 大小 | prgMask (A20-A14) | _regs[2]低7位 | 反推结果 |
|-------------|-------------------|-------------|---------|
| 32KB (0x8000) | 0b1111110 << 14 | 0x7E | (~0x7E+1)&0x7F = 2, *0x4000 = 0x8000 ✓ |
| 64KB (0x10000) | 0b1111100 << 14 | 0x7C | (~0x7C+1)&0x7F = 4, *0x4000 = 0x10000 ✓ |
| 128KB (0x20000) | 0b1111000 << 14 | 0x78 | (~0x78+1)&0x7F = 8, *0x4000 = 0x20000 ✓ |
| 256KB (0x40000) | 0b1110000 << 14 | 0x70 | (~0x70+1)&0x7F = 16, *0x4000 = 0x40000 ✓ |
| 512KB (0x80000) | 0b1100000 << 14 | 0x60 | (~0x60+1)&0x7F = 32, *0x4000 = 0x80000 ✓ |

## PRG 偏移量计算

```cpp
// PRG base = 游戏 PRG 数据在 flash 中的起始地址
_prgBase = ((uint32_t)_regs[0] << 22) | ((uint32_t)_regs[1] << 14);

// PRG bank A mapped = 实际映射的 bank 号
uint32_t prgBankAMapped = (_prgBase >> 13) | (_prgBankA & ((~(_prgMask >> 13) & 0xFE) | 1));

// PRG offset = 游戏 PRG 数据在 _prgRom 中的字节偏移
_prgOffset = prgBankAMapped * 0x2000;
```

## CHR 数据提取

### 关键点

CHR 数据**不在** `_chrRom` 中（`_chrRomSize = 0`），而是：
1. 在 flash 中位于独立的 ChrOffset（与 PrgOffset 不同）
2. 由 loader 通过 PPU 写入 CHR RAM
3. 在 CoolEmu 拦截时，CHR 数据已在 `_chrRam` 中从 offset 0 连续存放

### CHR RAM vs CHR ROM 判断

```cpp
_chrWriteEnabled = (_regs[7] & 0x02) != 0;
```

- `_chrWriteEnabled = false`：游戏使用 CHR ROM，CHR 数据已由 loader 拷贝到 CHR RAM
- `_chrWriteEnabled = true`：游戏使用 CHR RAM（运行时动态写入），不需要提供初始 CHR ROM 数据

### 提取代码

```cpp
if (!_chrWriteEnabled && _gameChrSize > 0 && _gameChrSize <= _orgChrSize) {
    // CHR ROM 游戏：从 CHR RAM 起始位置拷贝（loader 已将数据加载到这里）
    romData.ChrRom.assign(_orgChrRom, _orgChrRom + _gameChrSize);
}
// CHR RAM 游戏：不提供 ChrRom，让原生 mapper 自行分配可写的 CHR RAM
```

## Loader 执行流程

从 `fc_preloader.asm` 和 `fc_loader.asm`：

```
1. load_all_chr_banks  ← 从 flash(_prgRom) 拷贝 CHR 数据到 CHR RAM(_chrRam)
   - 按 8KB bank 依次拷贝
   - 使用 PPUADDR/PPUDATA 寄存器写入
   - bank 0, bank 1, bank 2... 连续存放

2. jmp loader          ← 设置 CoolGirl 寄存器
   - sta $5fff (0x00)  ← 清除配置/解锁
   - sta $5000-$5007   ← 逐个写入寄存器

3. loader_clean_and_start ← 清空内存，跳转复位向量
   - 清零 $0200-$07DF
   - jmp [$FFFC]
```

## Combiner 寄存器计算参考

来源: coolgirl-multirom-builder/Program.cs

```csharp
regs["reg_0"] = ((game.PrgOffset / 0x4000) >> 8) & 0xFF;  // prg_base[26:22]
regs["reg_1"] = (game.PrgOffset / 0x4000) & 0xFF;          // prg_base[21:14]
regs["reg_2"] = ((chrMask & 0x20) << 2) | (prgMask & 0x7F); // chr_mask[18], prg_mask[20:14]
regs["reg_3"] = (mapperInfo.PrgMode << 5) | 0;              // prg_mode[2:0], chr_bank_a[7:3]
regs["reg_4"] = (mapperInfo.ChrMode << 5) | (chrMask & 0x1F); // chr_mode[2:0], chr_mask[17:13]
regs["reg_5"] = (((mapperInfo.PrgBankA & 0x1F) << 2) | (game.Battery ? 0x02 : 0x01)) & 0xFF;
regs["reg_6"] = (flags << 5) | (mapperInfo.MapperRegister & 0x1F);
regs["reg_7"] = @params | ((mapperInfo.MapperRegister & 0x20) << 1);
```

其中:
```csharp
int prgMask = ~(game.PRG.Length / 0x4000 - 1);
int chrMask = ~(chrBankingSize / 0x2000 - 1);
```
