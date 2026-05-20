#pragma once
#include "pch.h"
#include "NES/BaseMapper.h"
#include "NES/NesCpu.h"
#include "NES/NesConsole.h"
#include "NES/NesMemoryManager.h"
#include "NES/BaseNesPpu.h"
#include "NES/Mappers/A12Watcher.h"

class Mapper83 : public BaseMapper
{
	uint8_t _regs[11] = {};
	uint8_t _exRegs[4] = {};
	bool _is2kBank = false;
	bool _isNot2kBank = false;
	uint8_t _mode = 0;
	uint8_t _bank = 0;
	uint16_t _irqCounter = 0;
	bool _irqEnabled = false;
	bool _irqSourceA12 = false;
	A12Watcher _a12Watcher;
	uint8_t _m2Prescaler = 0;

protected:
	uint32_t GetDipSwitchCount() override { return 2; }
	uint16_t GetPrgPageSize() override { return 0x2000; }
	uint16_t GetChrPageSize() override { return 0x400; }
	bool AllowRegisterRead() override { return true; }
	bool EnableCpuClockHook() override { return true; }
	bool EnableVramAddressHook() override { return true; }

	void InitMapper() override
	{
		memset(_regs, 0, sizeof(_regs));
		memset(_exRegs, 0, sizeof(_exRegs));
		_is2kBank = false;
		_isNot2kBank = false;
		_mode = 0;
		_bank = 0;
		_irqCounter = 0;
		_irqEnabled = false;
		_irqSourceA12 = false;
		_m2Prescaler = 0;

		AddRegisterRange(0x5000, 0x5000, MemoryOperation::Read);
		AddRegisterRange(0x5100, 0x5103, MemoryOperation::Any);
		RemoveRegisterRange(0x8000, 0xFFFF, MemoryOperation::Read);

		UpdateState();
	}

	void Serialize(Serializer& s) override
	{
		BaseMapper::Serialize(s);

		SVArray(_regs, 11);
		SVArray(_exRegs, 4);
		SV(_is2kBank);
		SV(_isNot2kBank);
		SV(_mode);
		SV(_bank);
		SV(_irqCounter);
		SV(_irqEnabled);
		SV(_irqSourceA12);
		SV(_a12Watcher);
		SV(_m2Prescaler);
	}

	void ProcessCpuClock() override
	{
		BaseProcessCpuClock();

		if(!_irqSourceA12 && _irqEnabled && _irqCounter != 0) {
			// NTSC/DENDY compensation: skip 1 tick every 16 CPU cycles (15/16 prescaler)
			// to match PAL's effective IRQ tick rate per scanline.
			ConsoleRegion region = _console->GetRegion();
			if(region == ConsoleRegion::Ntsc || region == ConsoleRegion::Dendy) {
				_m2Prescaler = (_m2Prescaler + 1) & 0x0F;
				if(_m2Prescaler == 0) {
					return;
				}
			}
			_irqCounter--;
			if(_irqCounter == 0) {
				_irqEnabled = false;
				_irqCounter = 0xFFFF;
				_console->GetCpu()->SetIrqSource(IRQSource::External);
			}
		}
	}

	void NotifyVramAddressChange(uint16_t addr) override
	{
		if(_irqSourceA12 && _irqEnabled && _irqCounter != 0) {
			if(_a12Watcher.UpdateVramAddress(addr, _console->GetPpu()->GetFrameCycle()) == A12StateChange::Rise) {
				_irqCounter--;
				if(_irqCounter == 0) {
					_irqEnabled = false;
					_irqCounter = 0xFFFF;
					_console->GetCpu()->SetIrqSource(IRQSource::External);
				}
			}
		}
	}

	void UpdateState()
	{
		switch(_mode & 0x03) {
			case 0: SetMirroringType(MirroringType::Vertical); break;
			case 1: SetMirroringType(MirroringType::Horizontal); break;
			case 2: SetMirroringType(MirroringType::ScreenAOnly); break;
			case 3: SetMirroringType(MirroringType::ScreenBOnly); break;
		}

		if(_is2kBank && !_isNot2kBank) {
			SelectChrPage2x(0, _regs[0] << 1);
			SelectChrPage2x(1, _regs[1] << 1);
			SelectChrPage2x(2, _regs[6] << 1);
			SelectChrPage2x(3, _regs[7] << 1);
		} else {
			for(int i = 0; i < 8; i++) {
				SelectChrPage(i, _regs[i] | ((_bank & 0x30) << 4));
			}
		}

		if(_mode & 0x40) {
			SelectPrgPage2x(0, (_bank & 0x3F) << 1);
			SelectPrgPage2x(1, ((_bank & 0x30) | 0x0F) << 1);
		} else {
			SelectPrgPage(0, _regs[8]);
			SelectPrgPage(1, _regs[9]);
			SelectPrgPage(2, _regs[10]);
			SelectPrgPage(3, -1);
		}
	}

	uint8_t ReadRegister(uint16_t addr) override
	{
		if(addr == 0x5000) {
			return (_console->GetMemoryManager()->GetOpenBus() & 0xFC) | GetDipSwitches();
		} else {
			return _exRegs[addr & 0x03];
		}
	}

	void WriteRegister(uint16_t addr, uint8_t value) override
	{
		if(addr < 0x8000) {
			_exRegs[addr & 0x03] = value;
		} else if((addr & 0x8C17) == 0x8C10 && (addr & 0x8C18) == 0x8C18) {
			_irqSourceA12 = (value != 0);
		} else if(addr >= 0x8300 && addr <= 0x8302) {
			_mode &= 0xBF;
			_regs[addr - 0x8300 + 8] = value;
			UpdateState();
		} else if(addr >= 0x8310 && addr <= 0x8317) {
			_regs[addr - 0x8310] = value;
			if(addr >= 0x8312 && addr <= 0x8315) {
				_isNot2kBank = true;
			}
			UpdateState();
		} else {
			switch(addr) {
				case 0x8000:
					_is2kBank = true;
					_bank = value;
					_mode |= 0x40;
					UpdateState();
					break;

				case 0xB000: case 0xB0FF: case 0xB1FF:
					// Dragon Ball Z Party [p1] BMC
					_bank = value;
					_mode |= 0x40;
					UpdateState();
					break;

				case 0x8100:
					_mode = value | (_mode & 0x40);
					UpdateState();
					break;

				case 0x8200:
					_irqCounter = (_irqCounter & 0xFF00) | value;
					_console->GetCpu()->ClearIrqSource(IRQSource::External);
					break;

				case 0x8201:
					_irqEnabled = (_mode & 0x80) == 0x80;
					_irqCounter = (_irqCounter & 0xFF) | (value << 8);
					// NTSC/DENDY compensation: subtract the equivalent of PAL's extra
					// 50 VBlank scanlines from the counter when the IRQ target scanline
					// would fall beyond the NTSC VBlank boundary (scanline > 242).
					{
						ConsoleRegion region = _console->GetRegion();
						if(region == ConsoleRegion::Ntsc || region == ConsoleRegion::Dendy) {
							const uint16_t PAL_50_LINE_CYCLE = (uint16_t)floor(50 * 106.5625);
							int32_t scanline = (int32_t)floor(_irqCounter / 106.5625) + _console->GetPpu()->GetCurrentScanline();
							if(scanline > 242) {
								if(_irqCounter < PAL_50_LINE_CYCLE) {
									_irqCounter = 0;
								} else {
									_irqCounter = (uint16_t)floor(_irqCounter - PAL_50_LINE_CYCLE);
								}
							}
						}
					}
					break;
			}
		}
	}
};
