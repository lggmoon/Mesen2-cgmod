# CoolEmu 技术文档

## 概述

CoolEmu 是 Mesen2 中的一个 NES mapper 实现，用于模拟 CoolGirl 多合一合卡。与原版 CoolGirl mapper 的关键区别在于：**CoolEmu 在检测到游戏启动信号后，切换到 Mesen 原生 mapper 来运行游戏**，而非全程用 CoolGirl 硬件逻辑模拟。

### 设计哲学

CoolGirl 是 FPGA 硬件实现，只能支持有限的 mapper 类型。CoolEmu 运行在软件模拟器中，可以利用 Mesen 已有的全部原生 mapper 实现，理论上可以支持 Mesen 支持的任何 mapper。

## 文件结构

| 文件 | 职责 |
|------|------|
| `CoolEmu.h` | 类定义、成员变量声明 |
| `CoolEmu.cpp` | 核心实现 |
| `coolemu.md` | 本文档 |

## 核心流程

```
NES 加载 .nes 文件
  │
  ├─ CoolEmu 作为 mapper 初始化
  │   └─ 分配 512KB CHR RAM
  │   └─ 进入 菜单模式
  │   └─ 菜单寄存器
  │      ├─ $5001 _menuPrgBank  单位 0x4000, 0~255;
  │      ├─ $5002 _menuChrBank  单位 0x2000, 0~31;
  │      ├─ $5003 _menuSramPage 单位 0x2000, 0~3;
  │      ├─ $5004 _menuConfig
  │
  ├─ 用户在菜单中选择游戏
  │
  ├─ ROM 侧 loader 执行:
       └─ loader
          ├─ sta $5000       ← 设置游戏模式
          ├─ sta $5001       ← offset_l
          ├─ sta $5002       ← offset_h
          ├─ sta $5003       ← size_l
          ├─ sta $5004       ← size_h
          ├─ sta $5005       ← idx_l
          ├─ sta $5006       ← idx_h
          └─ sta $500a #$CC  ← 启动游戏
  │
  ├─ CoolEmu 拦截 $5000 = 0x1， 开始接收load data， 等待 $500a = 0xCC,启动游戏
  │   ├─ ActivateGame()
  │   │   ├─ _gameOffset 单位1024，计算 _prgrom 中得偏移
  │   │   ├─ _gameSize   单位1024，计算 _prgrom 中得大小
  │   │   ├─ _gameIdx    菜单序号
  │   │   |
  │   │   ├─ MapperFactory::InitializeFromFile(_console, gameFile, romData, result) 装载游戏
  │   │
  │   │
  │
  └─ 原生 mapper 接管，游戏运行
```

### 常见问题排查

| 症状 | 可能原因 | 排查方法 |
|------|---------|---------|
| 游戏画面错乱 | PRG/CHR 大小计算错误 | 检查日志中的 game PRG/CHR size |
| 游戏读取到下一游戏数据 | PRG 大小算大了 | 检查 prgMask 和反推公式 |
| CHR 画面不对 | CHR 数据提取位置错误 | 检查 CHR write 标志和 ChrRAM 配置 |
| 存档不工作 | SaveRAM 大小为 0 | 检查 ConfigureNativeMapper 配置 |
| 游戏崩溃/重启 | BusConflicts 设置错误 | 检查 UxROM/CNROM 等是否启用冲突 |
| Mapper 不识别 | TranslateMapperCode 未覆盖 | 检查 CG code 是否在映射表中 |

## 与 CoolGirl 的差异

| 特性 | CoolGirl | CoolEmu |
|------|----------|---------|
| 运行方式 | FPGA 硬件模拟 | 软件模拟器 |
| Mapper 支持 | 受硬件限制（38种） | 可扩展至 Mesen 全部支持 |
| CHR 处理 | 硬件 bank switching | 提取数据后交给原生 mapper |
| 音频扩展 | 不支持 | 可通过原生 mapper 支持 |
| 存档 | Flash 写入 | 独立 .sav 文件 |
| PRG RAM | 固定 8KB | 按 mapper 配置灵活分配 |
| Bus Conflicts | 硬件自动处理 | 需显式配置 |

## 参考资源
- `docs/coolgirl.md` — CoolGirl 寄存器硬件文档
- `docs/coolgirl_rom.md` — ROM 内存布局与数据提取
- `coolgirl-multirom-builder` — 合卡构建工具 (https://github.com/ClusterM/coolgirl-multirom-builder)
- NES 2.0 规范 — SubMapper 和扩展信息
- Mesen2 源码 — 原生 mapper 实现 (`Core/NES/Mappers/`)
