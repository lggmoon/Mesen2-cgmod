#pragma once
#include "pch.h"
#include "NES/BaseMapper.h"
#include "NES/NesConsole.h"
#include "NES/NesCpu.h"
#include "NES/NesMemoryManager.h"
#include "NES/MapperFactory.h"
#include "Shared/Emulator.h"
#include "Shared/BatteryManager.h"

class CoolEmu : public BaseMapper
{
private:
	enum class Mode : uint8_t {
		Menu = 0,
		GameConfig = 1,
		GameRunning = 2
	};

	static constexpr uint8_t LAUNCH_MAGIC = 0xCC;
	static constexpr uint8_t VERSION = 0x20;

	unique_ptr<BaseMapper> _nativeMapper;

	Mode _mode = Mode::Menu;

	uint8_t _menuPrgBank = 0;
	uint8_t _menuChrBank = 0;
	uint8_t _menuSramPage = 0;
	uint8_t _menuConfig = 0;
	uint8_t _menuDpcmBank = 0;

	uint16_t _gameOffset = 0;
	uint16_t _gameSize = 0;
	uint16_t _gameIdx = 0;

	vector<uint8_t> _wram;
	uint8_t _writeDelay = 0;

	string _SaveName;

	void MenuModeSync();
	void ActivateGame();
	void RestoreMenuMode();
	uint32_t DecodeRamSize(uint8_t code);
	string GetGameFileName();

protected:
	uint32_t GetNametableCount() override { return 4; }
	uint16_t GetPrgPageSize() override { return 0x2000; }
	uint16_t GetChrPageSize() override { return 0x0400; }
	uint32_t GetChrRamSize() override { return 512 * 1024; }
	uint32_t GetSaveRamSize() override { return 0; }
	uint16_t RegisterStartAddress() override { return 0x5000; }
	uint16_t RegisterEndAddress() override { return 0x5FFF; }
	bool AllowRegisterRead() override { return true; }
	bool EnableCpuClockHook() override { return true; }
	bool EnableVramAddressHook() override { return true; }
	bool EnableCustomVramRead() override { return true; }

	void InitMapper() override;	
	void InterReset();
	void Reset(bool softReset) override;
	void WriteRegister(uint16_t addr, uint8_t value) override;
	uint8_t ReadRegister(uint16_t addr) override;
	void Serialize(Serializer& s) override;
	void ProcessCpuClock() override;
	void NotifyVramAddressChange(uint16_t addr) override;
	uint8_t MapperReadVram(uint16_t addr, MemoryOperationType operationType) override;
	void MapperWriteVram(uint16_t addr, uint8_t value) override;
	void SaveBattery() override;
	void LoadBattery() override;
	bool HasBattery() override;	
	void GetMemoryRanges(MemoryRanges& ranges) override;
	vector<MapperStateEntry> GetMapperStateEntries() override;

public:
	CoolEmu();
	~CoolEmu();

	static constexpr uint16_t MapperID = 999;
};
