#pragma once
#include "pch.h"
#include "NES/BaseMapper.h"
#include "NES/NesConsole.h"
#include "NES/NesCpu.h"
#include "NES/Mappers/A12Watcher.h"
#include "Shared/Emulator.h"
#include "Shared/BatteryManager.h"
#include "Utilities/Patches/IpsPatcher.h"

enum class CgMapper : uint8_t
{
	Passthrough    = 0b000000,
	UxROM          = 0b000001,
	CNROM          = 0b000010,
	HolyDiver      = 0b000011,
	TAM_S1         = 0b000100,
	Sunsoft2       = 0b000101,
	Mapper163      = 0b000110,
	SS88006        = 0b000111,
	AxROM          = 0b001000,
	Cheetahmen2    = 0b001001,
	ColorDreams    = 0b001010,
	GxROM          = 0b001011,
	Mapper87       = 0b001100,
	JY             = 0b001101,
	H3001          = 0b001110,
	MMC5           = 0b001111,
	MMC1           = 0b010000,
	MMC2_MMC4      = 0b010001,
	Bandai152      = 0b010010,
	VRC3           = 0b010011,
	MMC3_MMC6      = 0b010100,
	Mapper112      = 0b010101,
	Taito          = 0b010110,
	Mapper42       = 0b010111,
	VRC2_VRC4      = 0b011000,
	FME7           = 0b011001,
	IremG101       = 0b011010,
	NINA03_06      = 0b011011,
	Mapper133      = 0b011100,
	Mapper36       = 0b011101,
	Mapper70       = 0b011110,
	Mapper184      = 0b011111,
	Mapper38       = 0b100000,
	VRC1           = 0b100010,
	Mapper83			= 0b100011,
	Sunsoft3       = 0b100100,
	Sunsoft2On3    = 0b100101,
	ConyYoko		= 0b100110,
};

class Coolgirl : public BaseMapper
{
private:
	static const uint32_t SAVE_FLASH_SIZE = 1024 * 1024 * 8;
	static const uint32_t FLASH_SECTOR_SIZE = 128 * 1024;

	struct BitSpecEntry {
		const char* key;
		bool isContiguous;
		int shift;
		uint32_t mask;
		uint32_t posMask;
		int bits[32];
		uint8_t count;
	};

	BitSpecEntry _bitCache[64] = {};
	int _bitCacheCount = 0;
	BitSpecEntry* GetBitSpec(const char* bitsStr);

	vector<uint8_t> _wram;
	vector<uint8_t> _saveFlash;

	uint8_t _sramEnabled = 0;
	uint8_t _sramPage = 0;
	uint8_t _canWriteChr = 0;
	uint8_t _mapRomOn6000 = 0;
	uint8_t _flags = 0;
	CgMapper _mapper = CgMapper::Passthrough;
	uint8_t _canWriteFlash = 0;
	uint8_t _mirroring = 0;
	uint8_t _fourScreen = 0;
	uint8_t _lockout = 0;

	uint32_t _prgBase = 0;
	uint32_t _prgMask = 0b11111000 << 14;
	uint8_t _prgMode = 0;
	uint8_t _prgBank6000 = 0;
	uint8_t _prgBankA = 0;
	uint8_t _prgBankB = 1;
	uint8_t _prgBankC = (uint8_t)~1;
	uint8_t _prgBankD = (uint8_t)~0;

	uint32_t _chrMask = 0;
	uint8_t _chrMode = 0;
	uint16_t _chrBankA = 0;
	uint16_t _chrBankB = 1;
	uint16_t _chrBankC = 2;
	uint16_t _chrBankD = 3;
	uint16_t _chrBankE = 4;
	uint16_t _chrBankF = 5;
	uint16_t _chrBankG = 6;
	uint16_t _chrBankH = 7;

	uint8_t _tksMir[8] = {};

	uint32_t _prgBank6000Mapped = 0;
	uint32_t _prgBankAMapped = 0;
	uint32_t _prgBankBMapped = 0;
	uint32_t _prgBankCMapped = 0;
	uint32_t _prgBankDMapped = 0;

	uint8_t _ppuLatch0 = 0;
	uint8_t _ppuLatch1 = 0;
	uint8_t _mmc1LoadRegister = 0;
	uint8_t _mmc3Internal = 0;
	uint8_t _mapper69Internal = 0;
	uint8_t _mapper112Internal = 0;
	uint8_t _mapper163Latch = 0;
	uint8_t _mapper163R0 = 0;
	uint8_t _mapper163R1 = 0;
	uint8_t _mapper163R2 = 0;
	uint8_t _mapper163R3 = 0;
	uint8_t _mapper163R4 = 0;
	uint8_t _mapper163R5 = 0;

	uint8_t _mul1 = 0;
	uint8_t _mul2 = 0;

	uint8_t _mmc3IrqEnabled = 0;
	uint8_t _mmc3IrqLatch = 0;
	uint8_t _mmc3IrqCounter = 0;
	uint8_t _mmc3IrqReload = 0;
	bool _mmc3IrqReady = false;
	//uint8_t _mmc5IrqEnabled = 0;
	//uint8_t _mmc5IrqLine = 0;
	//uint8_t _mmc5IrqOut = 0;
	uint16_t _mapper18IrqValue = 0;
	uint8_t _mapper18IrqControl = 0;
	uint16_t _mapper18IrqLatch = 0;
	uint8_t _mapper65IrqEnabled = 0;
	uint16_t _mapper65IrqValue = 0;
	uint16_t _mapper65IrqLatch = 0;
	uint8_t _mapper69IrqEnabled = 0;
	uint8_t _mapper69CounterEnabled = 0;
	uint16_t _mapper69IrqValue = 0;
	uint8_t _vrc4IrqValue = 0;
	uint8_t _vrc4IrqControl = 0;
	uint8_t _vrc4IrqLatch = 0;
	uint8_t _vrc4IrqPrescaler = 0;
	uint8_t _vrc4IrqPrescalerCounter = 0;
	uint16_t _vrc3IrqValue = 0;
	uint8_t _vrc3IrqControl = 0;
	uint16_t _vrc3IrqLatch = 0;
	uint8_t _mapper42IrqEnabled = 0;
	uint16_t _mapper42IrqValue = 0;
	uint8_t _mapper83IrqEnabledLatch = 0;
	uint8_t _mapper83IrqEnabled = 0;
	uint16_t _mapper83IrqCounter = 0;
	uint8_t _mapper83IrqPrescaler = 0;
	bool _irqSourceA12 = false;
	uint8_t _conyYokoBank = 0;
	uint8_t _conyYokoMode = 0;
	uint8_t _exRegs[4] = {};
	uint8_t _mapper90Xor = 0;
	uint8_t _mapper67IrqEnabled = 0;
	uint8_t _mapper67IrqLatch = 0;
	uint16_t _mapper67IrqCounter = 0;

	uint8_t _flashState = 0;
	uint16_t _flashBufferA[10] = {};
	uint8_t _flashBufferV[10] = {};
	uint8_t _cfiMode = 0;

	uint64_t _lastWriteCycle = 0;
	vector<uint8_t> _orgPrgRom;

	A12Watcher _a12Watcher;

	typedef void (Coolgirl::*IrqHandler)();
	IrqHandler _irqHandler = nullptr;
	void UpdateIrqHandler();
	void ProcessVrc4Irq();
	void ProcessVrc3Irq();
	void ProcessFme7Irq();
	void ProcessSs88006Irq();
	void ProcessH3001Irq();
	void ProcessMapper42Irq();
	void ProcessConyYokoIrq();
	void ProcessSunsoft3Irq();

	//IRQ counter related fields
	uint8_t _mmc5IrqLine = 0;
	bool _mmc5IrqEnabled = false;
	bool _mmc5IrqPending = false;

	uint8_t _scanlineCounter = 0;
	bool _needInFrame = false;
	bool _ppuInFrame = false;
	uint8_t _ppuIdleCounter = 0;
	uint16_t _lastPpuReadAddr = 0;
	uint8_t _ntReadCounter = 0;

	// Bit manipulation functions (like FCEUX)
	uint32_t GetBits(uint32_t value, const char* bitsStr);
	uint32_t SetBits(uint32_t value, const char* bitsStr, uint32_t newValue);

	void SyncPrg();
	void SyncChr();
	void SyncMirroring();
	void Sync();
	void FlashWrite(uint16_t addr, uint8_t value);
	void HandleSubMapperLogic(uint16_t addr, uint8_t value);
	void ProcessScanlineCounter();
	static uint16_t ApplyChrMask(uint32_t chrMask, uint16_t bank);
	void MapPrgBank(uint16_t start, uint16_t end, uint32_t mappedBank);
	void SerializeFlashDiff(Serializer& s);

protected:
	uint32_t GetNametableCount() override { return 4; }
	uint16_t GetPrgPageSize() override { return 0x2000; }
	uint16_t GetChrPageSize() override { return 0x400; }
	uint32_t GetChrRamSize() override { return 512 * 1024; }
	uint16_t RegisterStartAddress() override { return 0x5000; }
	uint16_t RegisterEndAddress() override { return 0x5FFF; }
	bool HasBattery() override { return true; }
	uint32_t GetSaveRamSize() override { return _saveFlash.size() > 0 ? 0x2000 : 0; }
	bool EnableCpuClockHook() override { return true; }
	bool EnableVramAddressHook() override { return true; }
	bool AllowRegisterRead() override { return true; }
	bool EnableCustomVramRead() override { return true; }
	void SelectChrPage8x(uint16_t slot, uint16_t page, ChrMemoryType memoryType = ChrMemoryType::Default) override;
	void SelectChrPage4x(uint16_t slot, uint16_t page, ChrMemoryType memoryType = ChrMemoryType::Default) override;
	void SelectChrPage2x(uint16_t slot, uint16_t page, ChrMemoryType memoryType = ChrMemoryType::Default) override;
	void SelectChrPage(uint16_t slot, uint16_t page, ChrMemoryType memoryType = ChrMemoryType::Default) override;

	void InitMapper() override;
	void Reset(bool softReset) override;
	void WriteRegister(uint16_t addr, uint8_t value) override;
	uint8_t ReadRegister(uint16_t addr) override;
	void Serialize(Serializer& s) override;
	void ProcessCpuClock() override;
	void NotifyVramAddressChange(uint16_t addr) override;
	void SaveBattery() override;
	void LoadBattery() override;

	// for MMC5 (not real) scanline tick only.
	uint8_t MapperReadVram(uint16_t addr, MemoryOperationType memoryOperationType) override;

public:
	Coolgirl();
};
