 #pragma once
#include "pch.h"
#include "NES/BaseMapper.h"
#include "NES/NesCpu.h"
#include "NES/NesConsole.h"
#include "NES/NesMemoryManager.h"
#include "NES/BaseNesPpu.h"
#include "NES/Mappers/A12Watcher.h"

class Yoko : public BaseMapper
{
	uint8_t _regs[7] = {};
	uint8_t _exRegs[4] = {};
	uint8_t _mode = 0;
	uint8_t _bank = 0;
	uint16_t _irqCounter = 0;
	bool _irqEnabled = false;
	bool _irqSourceA12 = false;
	A12Watcher _a12Watcher;
	uint8_t _m2Prescaler = 0;

protected:
	uint32_t GetDipSwitchCount() override { return 2; }
	uint16_t RegisterStartAddress() override { return 0x5000; }
	uint16_t RegisterEndAddress() override { return 0x5FFF; }
	uint16_t GetPrgPageSize() override { return 0x2000; }
	uint16_t GetChrPageSize() override { return 0x800; }
	bool AllowRegisterRead() override { return true; }
	bool EnableCpuClockHook() override { return true; }
	bool EnableVramAddressHook() override { return true; }

	void InitMapper() override
	{
		memset(_regs, 0, sizeof(_regs));
		memset(_exRegs, 0, sizeof(_exRegs));
		_mode = 0;
		_bank = 0;
		_irqCounter = 0;
		_irqEnabled = false;
		_irqSourceA12 = false;

		RemoveRegisterRange(0x5000, 0x53FF, MemoryOperation::Write);
		AddRegisterRange(0x8000, 0xFFFF, MemoryOperation::Write);

		UpdateState();
	}

	void Serialize(Serializer& s) override
	{
		BaseMapper::Serialize(s);

		SVArray(_regs, 7);
		SVArray(_exRegs, 4);
		SV(_mode);
		SV(_bank);
		SV(_irqCounter);
		SV(_irqEnabled);
		SV(_irqSourceA12);
		SV(_a12Watcher);
		SV(_m2Prescaler);
	}

	void Reset(bool softReset) override
	{
		if(softReset) {
			_mode = 0;
			_bank = 0;
		}
	}

	void ClockIrqCounter()
	{
		if(_mode & 0x40) {
			_irqCounter--;
		} else {
			_irqCounter++;
		}
		if(_irqCounter == 0) {
			_irqEnabled = false;
			MessageManager::Log("[Yoko] IRQ FIRED (M2)! scanline=" + std::to_string(_console->GetPpu()->GetCurrentScanline()) + " mode=" + std::to_string(_mode) + " dir=" + std::string((_mode & 0x40) ? "DEC" : "INC"));
			_console->GetCpu()->SetIrqSource(IRQSource::External);
		}
	}

	void ProcessCpuClock() override
	{
		BaseProcessCpuClock();

		if(!_irqSourceA12 && _irqEnabled && _irqCounter != 0) {
			ConsoleRegion region = _console->GetRegion();
			if(region == ConsoleRegion::Ntsc || region == ConsoleRegion::Dendy) {
				_m2Prescaler = (_m2Prescaler + 1) & 0x0F;
				if(_m2Prescaler == 0) {
					return;
				}
			}
			ClockIrqCounter();
		}
	}

	void NotifyVramAddressChange(uint16_t addr) override
	{
		if(_irqSourceA12 && _irqEnabled && _irqCounter != 0) {
			if(_a12Watcher.UpdateVramAddress(addr, _console->GetPpu()->GetFrameCycle()) == A12StateChange::Rise) {
				ClockIrqCounter();
			}
		}
	}

	void UpdateState()
	{
		SetMirroringType(_mode & 0x01 ? MirroringType::Horizontal : MirroringType::Vertical);

		SelectChrPage(0, _regs[3]);
		SelectChrPage(1, _regs[4]);
		SelectChrPage(2, _regs[5]);
		SelectChrPage(3, _regs[6]);

		if(_mode & 0x10) {
			uint32_t outer = (_bank & 0x08) << 1;
			SelectPrgPage(0, outer | (_regs[0] & 0x0F));
			SelectPrgPage(1, outer | (_regs[1] & 0x0F));
			SelectPrgPage(2, outer | (_regs[2] & 0x0F));
			SelectPrgPage(3, outer | 0x0F);
		} else if(_mode & 0x08) {
			SelectPrgPage4x(0, (_bank & 0xFE) << 1);
		} else {
			SelectPrgPage2x(0, _bank << 1);
			SelectPrgPage2x(1, -2);
		}
	}

	uint8_t ReadRegister(uint16_t addr) override
	{
		if(addr <= 0x53FF) {
			return (_console->GetMemoryManager()->GetOpenBus() & 0xFC) | GetDipSwitches();
		} else {
			return _exRegs[addr & 0x03];
		}
	}

	void WriteRegister(uint16_t addr, uint8_t value) override
	{
		if(addr < 0x8000) {
			_exRegs[addr & 0x03] = value;
		} else {
			uint16_t decoded = addr & 0x8C17;
			if(decoded == 0x8C10 && (addr & 0x8C18) == 0x8C18) {
				_irqSourceA12 = (value != 0);
				MessageManager::Log("[Yoko] IRQ Source: " + std::string(_irqSourceA12 ? "PPU A12" : "M2") + " val=$" + HexUtilities::ToHex(value) + " scanline=" + std::to_string(_console->GetPpu()->GetCurrentScanline()));
				return;
			}
			switch(decoded) {
				case 0x8000:
					MessageManager::Log("[Yoko] PRG Bank=$" + HexUtilities::ToHex(value) + " scanline=" + std::to_string(_console->GetPpu()->GetCurrentScanline()));
					_bank = value; UpdateState(); break;
				case 0x8400:
					MessageManager::Log("[Yoko] Mode=$" + HexUtilities::ToHex(value) + " dir=" + std::string((value & 0x40) ? "DEC" : "INC") + " enable=" + std::string((value & 0x80) ? "1" : "0") + " prg_mode=" + std::to_string((value >> 3) & 0x03) + " mirror=" + std::to_string(value & 0x03) + " scanline=" + std::to_string(_console->GetPpu()->GetCurrentScanline()));
					_mode = value; UpdateState(); break;
				case 0x8800:
					_irqCounter = (_irqCounter & 0xFF00) | value;
					MessageManager::Log("[Yoko] IRQ Counter Low=$" + HexUtilities::ToHex(value) + " counter=$" + HexUtilities::ToHex(_irqCounter) + " scanline=" + std::to_string(_console->GetPpu()->GetCurrentScanline()));
					_console->GetCpu()->ClearIrqSource(IRQSource::External);
					break;
				case 0x8801:
				{
					_irqEnabled = (_mode & 0x80) != 0;
					_irqCounter = (_irqCounter & 0xFF) | (value << 8);
					ConsoleRegion region = _console->GetRegion();
					if(region == ConsoleRegion::Ntsc || region == ConsoleRegion::Dendy) {
						const uint16_t PAL_50_LINE_CYCLE = floor(50 * 106.5625);
						int32_t scanline = (int32_t)floor(_irqCounter / 106.5625) + _console->GetPpu()->GetCurrentScanline();
						if(scanline > 242) {
							if(_irqCounter < PAL_50_LINE_CYCLE) {
								_irqCounter = 0;
							} else {
								_irqCounter = (uint16_t)floor(_irqCounter - PAL_50_LINE_CYCLE);
							}
						}
					}
					MessageManager::Log("[Yoko] IRQ Counter High=$" + HexUtilities::ToHex(value) + " counter=$" + HexUtilities::ToHex(_irqCounter) + " enabled=" + std::string(_irqEnabled ? "1" : "0") + " scanline=" + std::to_string(_console->GetPpu()->GetCurrentScanline()));
				}
					break;
				case 0x8c00: _regs[0] = value; UpdateState(); break;
				case 0x8c01: _regs[1] = value; UpdateState(); break;
				case 0x8c02: _regs[2] = value; UpdateState(); break;
				case 0x8c10: _regs[3] = value; UpdateState(); break;
				case 0x8c11: _regs[4] = value; UpdateState(); break;
				case 0x8c16: _regs[5] = value; UpdateState(); break;
				case 0x8c17: _regs[6] = value; UpdateState(); break;
			}
		}
	}
};
