# Coolgirl Emulator 开发计划

## 项目概述

Coolgirl 是一款基于 CPLD（EPM1270T144）的 Famicom 多合一卡带硬件，通过可编程逻辑模拟多种 NES mapper。Mesen 模拟器中的 Coolgirl.cpp 是对该硬件的软件模拟实现。

本开发计划分为三个阶段，逐步完善 Coolgirl mapper 的模拟精度和功能。

---

## 第一阶段：修正与 Coolgirl 硬件不一致的地方 ✅ 已完成

**目标**：让 Mesen 的 Coolgirl.cpp 行为与 FPGA 硬件（CoolGirl_mappers.vh、CoolGirl.v）完全一致。

### 修复清单（12 项）

| # | 问题 | 优先级 | 状态 |
|---|------|--------|------|
| 1 | ConyYoko prgBankC 写错（写了 _prgBankB） | 严重 | ✅ 已修复 |
| 2 | ConyYoko submapper1 CHR 缺失（case 2/3/4/5） | 严重 | ✅ 已修复 |
| 3 | TxSROM per-tile mirroring 检查错误位（bit7→bit8） | 严重 | ✅ 已修复 |
| 4 | FME7 IRQ 缺少使能检查 | 严重 | ✅ 已修复 |
| 5 | MMC3 irq_ready 机制缺失 | 建议 | ✅ 已修复 |
| 6 | Taito IRQ 未检查 flags（mapper33 不应有 IRQ） | 建议 | ✅ 已修复 |
| 7 | Mapper163 scanline 触发值（127→129, 239→0） | 建议 | ✅ 已修复 |
| 8 | prgMask 初始值（bit16 差异） | 微小 | ✅ 已修复 |
| 9 | 合并冲突标记清理 | 杂项 | ✅ 已修复 |

### 参考硬件源码
- `CoolGirl_config.vh` — CPLD 配置参数
- `CoolGirl_mappers.vh` — Mapper 逻辑实现
- `CoolGirl.v` — 顶层地址映射

---

## 第二阶段：修复软件实现错误 ⏳ 未开始

**目标**：修复 Coolgirl.cpp 中即使按照 FPGA 硬件逻辑也是错误的实现。这些不是与 FPGA 不一致的问题，而是 Coolgirl 自身逻辑就有 bug 的地方。

### 待修复项（8 项）

| # | Mapper | 问题 | 说明 |
|---|--------|------|------|
| 1 | Sunsoft2 | CHR bank 映射错误 | CHR bank 位设置不正确 |
| 2 | Mapper112 | 缺少 outerChrBank | 缺少外层 CHR bank 支持 |
| 3 | HolyDiver | bank0 固定逻辑缺失 | bank0 应该固定但没实现 |
| 4 | TAM_S1 | 镜像模式不完整 | 缺少某些镜像模式 |
| 5 | Mapper184 | CHR bank 映射错误 | CHR bank 粒度错误 |
| 6 | Sunsoft3 | CHR 粒度错误 | CHR banking 粒度不对 |
| 7 | NINA03_06 | 实现不完整 | 部分功能缺失 |
| 8 | ColorDreams | PRG bank bits 不足 | PRG bank 位数不够 |

### 参考原生 Mapper
- `Sunsoft89.h` — Sunsoft2/3 原生实现
- `IremTamS1.h` — TAM_S1 原生实现
- `JyCompany.h` — Mapper112 相关

---

## 第三阶段：增强模式（超越硬件限制） ⏳ 未开始

**目标**：在 Coolgirl 基础上添加超出 FPGA 硬件能力的功能，利用 Mesen 原生 mapper 的完善实现来补充 Coolgirl 的不足。

### A 类 — 硬件架构限制（25% 的问题）

| # | 问题 | 说明 |
|---|------|------|
| 1 | 无音频扩展芯片支持 | FME7/Sunsoft5B、VRC6/VRC7 音频无法模拟 |
| 2 | 无 ExRAM/split nametable | MMC5 的扩展 RAM 和分屏功能缺失 |
| 3 | CHR-RAM only 设计 | 无法模拟 CHR-ROM 特定行为 |
| 4 | 有限的 PRG/CHR banking 模式 | 只有 8 种模式 |
| 5 | PRG bank 寄存器位宽限制 | 8 bits，某些游戏需要更多 |
| 6 | 仅 3 个 flag bits | 某些 mapper 需要更多标志位 |
| 7 | 无 per-tile mirroring 硬件支持 | 需要软件模拟 |

### B 类 — 硬件设计简化（30% 的问题）

| # | 问题 | 说明 |
|---|------|------|
| 1 | 缺少 MMC1 写入周期保护 | MMC1 的 5-bit 移位寄存器写保护 |
| 2 | 简化 MMC3 WRAM 保护 | 缺少完整的 WRAM 保护逻辑 |
| 3 | 缺少 MMC6 特殊 RAM | MMC6 的 256-byte RAM 模拟 |
| 4 | 简化 VRC3 IRQ 处理 | VRC3 的 IRQ 逻辑不完整 |
| 5 | 不完整 JY mapper 功能 | JY Company mapper 的部分功能缺失 |
| 6 | 缺少 bus conflict 模拟 | UNROM 等 mapper 的总线冲突 |
| 7 | 缺少 submapper 区分 | 同一 mapper 下的变体无法区分 |

### 实现方案

采用**选择性增强**架构：
- 在 Coolgirl 内部保留统一的寄存器系统
- 针对特定 mapper 添加增强逻辑（通过配置开关控制）
- 不破坏现有硬件兼容性
- 可选启用/禁用增强功能

---

## 进度总览

| 阶段 | 目标 | 项目数 | 已完成 | 进度 |
|------|------|--------|--------|------|
| 第一阶段 | 对齐 FPGA 硬件行为 | 12 项 | 12 项 | 100% ✅ |
| 第二阶段 | 修复软件实现 bug | 8 项 | 0 项 | 0% ⏳ |
| 第三阶段 | 增强模式（超越硬件） | ~14 项 | 0 项 | 0% ⏳ |

---

## 相关文档

- `Coolgirl.md` — Coolgirl 硬件寄存器文档
- `coolgirl-famicom-multicart/CoolGirl_mappers.vh` — FPGA mapper 逻辑
- `coolgirl-famicom-multicart/CoolGirl_config.vh` — FPGA 配置参数
- `coolgirl-famicom-multicart/CoolGirl_rev6.x/CoolGirl.v` — FPGA 顶层逻辑
