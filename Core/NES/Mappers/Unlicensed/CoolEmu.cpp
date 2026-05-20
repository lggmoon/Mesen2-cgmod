#include "pch.h"
#include "NES/Mappers/Unlicensed/CoolEmu.h"
#include "NES/NesConsole.h"
#include "NES/NesCpu.h"
#include "NES/NesMemoryManager.h"
#include "NES/BaseNesPpu.h"
#include "NES/MapperFactory.h"
#include "Shared/MessageManager.h"
#include "Utilities/FolderUtilities.h"

CoolEmu::CoolEmu()
{
	_wram.resize(32 * 1024, 0);
}

CoolEmu::~CoolEmu()
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		_nativeMapper.reset();
	}
	_mode = Mode::Menu;
}

uint32_t CoolEmu::DecodeRamSize(uint8_t code)
{
	switch (code) {
	case 0: return 0;
	case 1: return 0x2000;
	case 2: return 0x4000;
	case 3: return 0x8000;
	default: return 0;
	}
}

void CoolEmu::MenuModeSync()
{	
	// set prg bank 0x8000, 0xBFFF;
	uint32_t prgBankAddr = (uint32_t)_menuPrgBank * 0x4000;
	SetCpuMemoryMapping(0x8000, 0xBFFF, PrgMemoryType::PrgRom, prgBankAddr, MemoryAccessType::Read);
	// set prg bank 0xC000, 0xFFFF: bank 15¡¢bank 16 so 15 >> 1 = 7
	SetCpuMemoryMapping(0xC000, 0xFFFF, PrgMemoryType::PrgRom, 7 * 0x4000, MemoryAccessType::Read);

	bool sramEnabled = (_menuConfig & 0x01) != 0;
	if (sramEnabled && _wram.size() > 0) {
		SetCpuMemoryMapping(0x6000, 0x7FFF, _wram.data(), (_menuSramPage & 0x03) * 0x2000, (uint32_t)_wram.size(), MemoryAccessType::ReadWrite);
	} else {
		RemoveCpuMemoryMapping(0x6000, 0x7FFF);
	}

	uint32_t chrBankAddr = (uint32_t)_menuChrBank * 0x2000;
	if (chrBankAddr + 0x2000 <= _chrRamSize) {
		SelectChrPage8x(0, _menuChrBank, ChrMemoryType::ChrRam);
	}

	if (!(_menuConfig & 0x02)) {
		SetMirroringType(MirroringType::Vertical);
	} else {
		SetMirroringType(MirroringType::Horizontal);
	}
}

void CoolEmu::ActivateGame()
{
	string batteryRomName = GetGameFileName();
	_emu->GetBatteryManager()->Initialize(batteryRomName, true);

	size_t dataSize = _gameSize * 1024;
	uint8_t* pData = _prgRom + _gameOffset * 1024;
	VirtualFile gameFile(pData, dataSize, batteryRomName +".nes");
	RomData romData;
	LoadRomResult result = LoadRomResult::UnknownType;
	auto mapper = MapperFactory::InitializeFromFile(_console, gameFile, romData, result);
	if(!mapper) {
		MessageManager::Log("[CoolEmu] Failed to create native mapper!");
		RestoreMenuMode();
		return;
	}
	_nativeMapper.swap(mapper);

	auto romInfo = _nativeMapper->GetRomInfo();
	MessageManager::Log("[CoolEmu] Activating native mapper: " + std::to_string(romInfo.MapperID) + "." + std::to_string(romInfo.SubMapperID));
	MessageManager::Log("[CoolEmu] Game offset: 0x" + std::to_string(_gameOffset) + "KB");
	MessageManager::Log("[CoolEmu] Game size: 0x" + std::to_string(_gameSize) + "KB");
	MessageManager::Log("[CoolEmu] Game idx: " + std::to_string(_gameIdx));

	RemoveRegisterRange(0x5000, 0x5FFF, MemoryOperation::Any);
	RemoveRegisterRange(0x6000, 0x7FFF, MemoryOperation::Any);
	RemoveRegisterRange(0x8000, 0xFFFF, MemoryOperation::Any);
		
	_nativeMapper->InitSpecificMapper(romData);

	auto memoryManager = _console->GetMemoryManager();
	memoryManager->UnregisterIODevice(this);

	if(_nativeMapper->GetEpsm()) {
		memoryManager->RegisterIODevice(reinterpret_cast<INesMemoryHandler*>(_nativeMapper->GetEpsm()));
	}
	memoryManager->RegisterIODevice(_nativeMapper.get());	

	_nativeMapper->Reset(true);
	_nativeMapper->OnAfterResetPowerOn();	

	_mode = Mode::GameRunning;
	uint16_t resetAddr = _nativeMapper->DebugReadRam(0xFFFC) |
	                     (_nativeMapper->DebugReadRam(0xFFFD) << 8);
	NesCpuState& cpuState = _console->GetCpu()->GetState();
	cpuState.A = 0;
	cpuState.X = 0;
	cpuState.Y = 0;
	cpuState.SP = 0xFD;
	cpuState.PS = 0x04;
	cpuState.NmiFlag = false;
	cpuState.IrqFlag = 0;
	cpuState.PC = resetAddr;
}

void CoolEmu::RestoreMenuMode()
{
	auto memoryManager = _console->GetMemoryManager();
	if (_mode == Mode::GameRunning && _nativeMapper) {		
		memoryManager->UnregisterIODevice(_nativeMapper.get());
		if(_nativeMapper->GetEpsm()) {
			memoryManager->UnregisterIODevice(reinterpret_cast<INesMemoryHandler*>(_nativeMapper->GetEpsm()));
		}
		if(_nativeMapper->GetRomInfo().MapperID == 5) {
			//fix mapperid 5
			memoryManager->RegisterWriteHandler(_console->GetPpu(), 0x2000, 0x2007);
		}
		_nativeMapper.reset();		
	}	

	memoryManager->RegisterIODevice(this);

	AddRegisterRange(0x5000, 0x5FFF, MemoryOperation::Any);
	AddRegisterRange(0x6000, 0x7FFF, MemoryOperation::Any);
	AddRegisterRange(0x8000, 0xFFFF, MemoryOperation::Any);

	_mode = Mode::Menu;
	_emu->RegisterMemory(MemoryType::NesPrgRom, _prgRom, _prgSize);
	_emu->RegisterMemory(MemoryType::NesChrRam, _chrRam, _chrRamSize);

	_emu->GetBatteryManager()->Initialize(FolderUtilities::GetFilename(_romInfo.RomName, false), true);

	MenuModeSync();
}

void CoolEmu::InitMapper()
{
	if (_chrRomSize == 0 && _chrRamSize < GetChrRamSize()) {
		InitializeChrRam(GetChrRamSize());
	}
	InterReset();
}

void CoolEmu::InterReset()
{
	AddRegisterRange(0x5000, 0x5FFF, MemoryOperation::Any);
	AddRegisterRange(0x6000, 0x7FFF, MemoryOperation::Any);
	AddRegisterRange(0x8000, 0xFFFF, MemoryOperation::Any);

	_mode = Mode::Menu;
	_menuPrgBank = 0;
	_menuChrBank = 0;
	_menuSramPage = 0;
	_menuConfig = 0;
	_menuDpcmBank = 0;

	_gameOffset = 0;
	_gameSize = 0;
	_gameIdx = 0;

	_writeDelay = 0;

	MenuModeSync();
}

void CoolEmu::Reset(bool softReset)
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		_nativeMapper->SaveBattery();
	}

	RestoreMenuMode();

	InterReset();
}

void CoolEmu::WriteRegister(uint16_t addr, uint8_t value)
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		_nativeMapper->WriteRam(addr, value);
		return;
	}

	if (_mode == Mode::Menu && (_menuConfig & 0x01) && addr >= 0x6000 && addr < 0x8000 && _wram.size() > 0) {
		_wram[(_menuSramPage & 0x03) * 0x2000 + (addr & 0x1FFF)] = value;
	}

	if (_writeDelay) return;
	_writeDelay = 1;

	if (addr == 0x5000) {
		if (value == 0x00) {
			_mode = Mode::Menu;
			MenuModeSync();
		} else if (value == 0x01) {
			_mode = Mode::GameConfig;
			_gameOffset = 0;
			_gameSize = 0;
			_gameIdx = 0;
		}
		return;
	}

	if (_mode == Mode::Menu && addr > 0x5000 && addr < 0x5005) {
		switch (addr & 0x07) {
		case 1: _menuPrgBank = value; break;
		case 2: _menuChrBank = value; break;
		case 3: _menuSramPage = value & 0x03; break;
		case 4: _menuConfig = value & 0x03; break;
		default: return;
		}
		MenuModeSync();
		return;
	}

	if (_mode == Mode::GameConfig && addr >= 0x5001 && addr <= 0x5010) {
		switch (addr) {
		case 0x5001: _gameOffset =  value; break; 
		case 0x5002: _gameOffset = (_gameOffset & 0x00FF) | ((uint16_t)value << 8); break;
		case 0x5003: _gameSize = value; break;
		case 0x5004: _gameSize = (_gameSize & 0x00FF) | ((uint16_t)value << 8); break;
		case 0x5005: _gameIdx = value; break;
		case 0x5006: _gameIdx = (_gameIdx & 0x00FF) | ((uint16_t)value << 8); break;
		case 0x500A:
			if (value == LAUNCH_MAGIC) {
				ActivateGame();
			}
			break;
		}
		return;
	}

}

uint8_t CoolEmu::ReadRegister(uint16_t addr)
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		return _nativeMapper->ReadRam(addr);
	}

	if (_mode == Mode::Menu && (_menuConfig & 0x01) && addr >= 0x6000 && addr < 0x8000 && _wram.size() > 0) {
		return _wram[(_menuSramPage & 0x03) * 0x2000 + (addr & 0x1FFF)];
	}

	if (addr >= 0x8000) {
		return InternalReadRam(addr);
	}

	switch (addr & 0x07) {
	case 6: return VERSION;
	case 7: return ((_mode == Mode::GameRunning) ? 0x80 : 0x00) | (VERSION & 0x07);
	default: return _console->GetMemoryManager()->GetOpenBus();
	}
}

void CoolEmu::ProcessCpuClock()
{
	_writeDelay = 0;

	if (_mode == Mode::GameRunning && _nativeMapper) {
		if (_nativeMapper->HasCpuClockHook()) {
			_nativeMapper->ProcessCpuClock();
		}
	}
}

void CoolEmu::NotifyVramAddressChange(uint16_t addr)
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		if (_nativeMapper->HasVramAddressHook()) {
			_nativeMapper->NotifyVramAddressChange(addr);
		}
	}
}

uint8_t CoolEmu::MapperReadVram(uint16_t addr, MemoryOperationType operationType)
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		return _nativeMapper->ReadVram(addr, operationType);
	}
	return InternalReadVram(addr);
}

void CoolEmu::MapperWriteVram(uint16_t addr, uint8_t value)
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		_nativeMapper->WriteVram(addr, value);
		return;
	}
	InternalWriteVram(addr, value);
}

void CoolEmu::SaveBattery()
{		
	if(_mode == Mode::GameRunning && _nativeMapper) {
		_nativeMapper->SaveBattery();
		_emu->GetBatteryManager()->Initialize(FolderUtilities::GetFilename(_romInfo.RomName, false), true);
	}
	_emu->GetBatteryManager()->SaveBattery(".sav", _wram.data(), (uint32_t)_wram.size());
	if(_mode == Mode::GameRunning && _nativeMapper) {
		_emu->GetBatteryManager()->Initialize(GetGameFileName(), true);
	}
}

void CoolEmu::LoadBattery()
{
	if(_mode == Mode::GameRunning && _nativeMapper) {
		_nativeMapper->LoadBattery();
		_emu->GetBatteryManager()->Initialize(FolderUtilities::GetFilename(_romInfo.RomName, false), true);
	}
	_emu->GetBatteryManager()->LoadBattery(".sav", _wram.data(), (uint32_t)_wram.size());
	if(_mode == Mode::GameRunning && _nativeMapper) {
		_emu->GetBatteryManager()->Initialize(GetGameFileName(), true);
	}
}

bool CoolEmu::HasBattery()
{
	return true;
}

string CoolEmu::GetGameFileName()
{
	char buf[255];
	snprintf(buf, sizeof(buf), "%s_%03d", FolderUtilities::GetFilename(_romInfo.RomName, false).c_str(), _gameIdx);
	return string(buf);
}

void CoolEmu::GetMemoryRanges(MemoryRanges& ranges)
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		_nativeMapper->GetMemoryRanges(ranges);
	} else {
		BaseMapper::GetMemoryRanges(ranges);
	}
}

vector<MapperStateEntry> CoolEmu::GetMapperStateEntries()
{
	if (_mode == Mode::GameRunning && _nativeMapper) {
		return _nativeMapper->GetMapperStateEntries();
	}

	vector<MapperStateEntry> entries;
	entries.push_back(MapperStateEntry("--", "Mode", (uint8_t)_mode));
	entries.push_back(MapperStateEntry("--", "Menu PRG Bank", _menuPrgBank));
	entries.push_back(MapperStateEntry("--", "Menu CHR Bank", _menuChrBank));
	entries.push_back(MapperStateEntry("--", "Menu SRAM Page", _menuSramPage));
	entries.push_back(MapperStateEntry("--", "Menu Config", _menuConfig));
	entries.push_back(MapperStateEntry("--", "Menu DPCM Bank", _menuDpcmBank));
	return entries;
}

void CoolEmu::Serialize(Serializer& s)
{
	BaseMapper::Serialize(s);

	SV(_mode);
	SV(_menuPrgBank);
	SV(_menuChrBank);
	SV(_menuSramPage);
	SV(_menuConfig);
	SV(_menuDpcmBank);
	SV(_gameOffset);
	SV(_gameSize);
	SV(_gameIdx);	
	SVArray(_wram.data(), (uint32_t)_wram.size());

	if (_mode == Mode::GameRunning && _nativeMapper) {
		_nativeMapper->Serialize(s);
	}

	/*if (!s.IsSaving() && _mode == Mode::GameRunning && !_nativeMapper) {
		RestoreMenuMode();
	}*/
}