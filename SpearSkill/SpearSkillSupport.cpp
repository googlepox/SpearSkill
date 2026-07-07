#include "common/IDebugLog.h"
#include "obse/GameAPI.h"
#include "obse/GameBSExtraData.h"
#include "obse/GameData.h"
#include "obse/GameExtraData.h"
#include "obse/GameForms.h"
#include "obse/GameObjects.h"
#include "obse_common/SafeWrite.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <windows.h>

std::FILE* IDebugLog::logFile = nullptr;
char IDebugLog::sourceBuf[16] = {};
char IDebugLog::headerText[16] = {};
char IDebugLog::formatBuf[8192] = {};
int IDebugLog::indentLevel = 0;
int IDebugLog::rightMargin = 0;
int IDebugLog::cursorPos = 0;
int IDebugLog::inBlock = 0;
bool IDebugLog::autoFlush = true;
IDebugLog::LogLevel IDebugLog::logLevel = IDebugLog::kLevel_DebugMessage;
IDebugLog::LogLevel IDebugLog::printLevel = IDebugLog::kLevel_Message;

IDebugLog::IDebugLog()
{
}

IDebugLog::IDebugLog(const char* name)
{
	Open(name);
}

IDebugLog::~IDebugLog()
{
	if (logFile)
		std::fclose(logFile);
	logFile = nullptr;
}

void IDebugLog::Open(const char* path)
{
	if (logFile)
		std::fclose(logFile);
	logFile = nullptr;

	if (path && path[0])
		fopen_s(&logFile, path, "w");
}

void IDebugLog::OpenRelative(int, const char* relPath)
{
	Open(relPath);
}

void IDebugLog::Message(const char* message, const char* source)
{
	if (source)
		SetSource(source);

	if (!message)
		message = "";

	if (logFile)
	{
		if (headerText[0])
			std::fputs(headerText, logFile);
		std::fputs(message, logFile);
		std::fputc('\n', logFile);
		if (autoFlush)
			std::fflush(logFile);
	}

	OutputDebugStringA(message);
	OutputDebugStringA("\n");
}

void IDebugLog::FormattedMessage(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	FormattedMessage(fmt, args);
	va_end(args);
}

void IDebugLog::FormattedMessage(const char* fmt, va_list args)
{
	if (!fmt)
		return;

	vsprintf_s(formatBuf, sizeof(formatBuf), fmt, args);
	Message(formatBuf);
}

void IDebugLog::Log(LogLevel level, const char* fmt, va_list args)
{
	if (!fmt || level > logLevel)
		return;

	vsprintf_s(formatBuf, sizeof(formatBuf), fmt, args);
	Message(formatBuf);
}

void IDebugLog::SetSource(const char* source)
{
	strcpy_s(sourceBuf, sizeof(sourceBuf), source ? source : "");
	strcpy_s(headerText, sizeof(headerText), "[        ]\t");

	char* target = headerText + 1;
	const char* current = sourceBuf;
	for (int i = 0; i < 8 && *current; ++i)
		*target++ = *current++;
}

void IDebugLog::ClearSource()
{
	sourceBuf[0] = '\0';
	headerText[0] = '\0';
}

void IDebugLog::Indent()
{
	++indentLevel;
}

void IDebugLog::Outdent()
{
	if (indentLevel)
		--indentLevel;
}

void IDebugLog::OpenBlock()
{
	inBlock = 1;
}

void IDebugLog::CloseBlock()
{
	inBlock = 0;
}

void IDebugLog::SetAutoFlush(bool inAutoFlush)
{
	autoFlush = inAutoFlush;
}

void IDebugLog::PrintSpaces(int numSpaces)
{
	if (logFile)
	{
		for (int i = 0; i < numSpaces; ++i)
			std::fputc(' ', logFile);
	}
	cursorPos += numSpaces;
}

void IDebugLog::PrintText(const char* buf)
{
	if (!buf)
		return;
	if (logFile)
		std::fputs(buf, logFile);
	cursorPos += static_cast<int>(std::strlen(buf));
}

void IDebugLog::NewLine()
{
	if (logFile)
		std::fputc('\n', logFile);
	cursorPos = 0;
}

void IDebugLog::SeekCursor(int position)
{
	if (position > cursorPos)
		PrintSpaces(position - cursorPos);
}

int IDebugLog::TabSize()
{
	return ((~cursorPos) & 3) + 1;
}

int IDebugLog::RoundToTab(int spaces)
{
	return (spaces + 3) & ~3;
}

void SafeWrite8(UInt32 addr, UInt32 data)
{
	DWORD oldProtect = 0;
	VirtualProtect(reinterpret_cast<void*>(addr), 1, PAGE_EXECUTE_READWRITE, &oldProtect);
	*reinterpret_cast<UInt8*>(addr) = static_cast<UInt8>(data);
	VirtualProtect(reinterpret_cast<void*>(addr), 1, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), 1);
}

void SafeWrite16(UInt32 addr, UInt32 data)
{
	DWORD oldProtect = 0;
	VirtualProtect(reinterpret_cast<void*>(addr), 2, PAGE_EXECUTE_READWRITE, &oldProtect);
	*reinterpret_cast<UInt16*>(addr) = static_cast<UInt16>(data);
	VirtualProtect(reinterpret_cast<void*>(addr), 2, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), 2);
}

void SafeWrite32(UInt32 addr, UInt32 data)
{
	DWORD oldProtect = 0;
	VirtualProtect(reinterpret_cast<void*>(addr), 4, PAGE_EXECUTE_READWRITE, &oldProtect);
	*reinterpret_cast<UInt32*>(addr) = data;
	VirtualProtect(reinterpret_cast<void*>(addr), 4, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), 4);
}

void SafeWriteBuf(UInt32 addr, void* data, UInt32 len)
{
	DWORD oldProtect = 0;
	VirtualProtect(reinterpret_cast<void*>(addr), len, PAGE_EXECUTE_READWRITE, &oldProtect);
	std::memcpy(reinterpret_cast<void*>(addr), data, len);
	VirtualProtect(reinterpret_cast<void*>(addr), len, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), len);
}

void WriteRelJump(UInt32 jumpSrc, UInt32 jumpTgt)
{
	SafeWrite8(jumpSrc, 0xE9);
	SafeWrite32(jumpSrc + 1, jumpTgt - jumpSrc - 5);
}

void WriteRelCall(UInt32 jumpSrc, UInt32 jumpTgt)
{
	SafeWrite8(jumpSrc, 0xE8);
	SafeWrite32(jumpSrc + 1, jumpTgt - jumpSrc - 5);
}

void WriteRelJnz(UInt32 jumpSrc, UInt32 jumpTgt)
{
	SafeWrite16(jumpSrc, 0x850F);
	SafeWrite32(jumpSrc + 2, jumpTgt - jumpSrc - 6);
}

void WriteRelJle(UInt32 jumpSrc, UInt32 jumpTgt)
{
	SafeWrite16(jumpSrc, 0x8E0F);
	SafeWrite32(jumpSrc + 2, jumpTgt - jumpSrc - 6);
}

const _ExtractArgs ExtractArgs = (_ExtractArgs)0x004FAE80;
const _LookupFormByID LookupFormByID = (_LookupFormByID)0x0046B250;
const _QueueUIMessage QueueUIMessage = (_QueueUIMessage)0x0057ACC0;
const _QueueUIMessage_2 QueueUIMessage_2 = (_QueueUIMessage_2)0x0057ADD0;

DataHandler** g_dataHandler = (DataHandler**)0x00B33A98;
PlayerCharacter** g_thePlayer = (PlayerCharacter**)0x00B333C4;
void* g_gameSettingsTable = (void*)0x00B35574;

static BSExtraData* FindExtraByType(const BaseExtraList* list, UInt32 type)
{
	if (!list)
		return nullptr;

	const UInt32 index = type >> 3;
	const UInt8 bitMask = 1 << (type & 7);
	if (!(list->m_presenceBitfield[index] & bitMask))
		return nullptr;

	for (BSExtraData* traverse = list->m_data; traverse; traverse = traverse->next)
	{
		if (traverse->type == type)
			return traverse;
	}

	return nullptr;
}

UInt32 NiTPointerMap_Lookup(void* map, void* key, void** data)
{
	return ThisStdCall(0x0055E000, map, key, data);
}

bool GetGameSetting(char* settingName, SettingInfo** setting)
{
	return NiTPointerMap_Lookup(g_gameSettingsTable, settingName, (void**)setting) != 0;
}

UInt32 Actor::GetBaseActorValue(UInt32 value)
{
	return ThisStdCall(0x005F1910, this, value);
}

EquippedItemsList Actor::GetEquippedItems()
{
	EquippedItemsList itemList;

	ExtraContainerChanges* xChanges =
		static_cast<ExtraContainerChanges*>(FindExtraByType(&baseExtraList, kExtraData_ContainerChanges));
	if (!xChanges || !xChanges->data || !xChanges->data->objList)
		return itemList;

	for (ExtraContainerChanges::Entry* entry = xChanges->data->objList; entry; entry = entry->next)
	{
		if (!entry->data || !entry->data->extendData || !entry->data->type)
			continue;

		for (ExtraContainerChanges::EntryExtendData* extend = entry->data->extendData; extend; extend = extend->next)
		{
			if (extend->data &&
				(FindExtraByType(extend->data, kExtraData_Worn) || FindExtraByType(extend->data, kExtraData_WornLeft)))
			{
				itemList.push_back(entry->data->type);
			}
		}
	}

	return itemList;
}

TESSkill* TESSkill::SkillForActorVal(UInt32 valSkill)
{
	return &(*g_dataHandler)->skills[valSkill - kSkill_Armorer];
}

const ModEntry** DataHandler::GetActiveModList()
{
	static const ModEntry* activeModList[0x100] = { 0 };

	if (!activeModList[0])
	{
		UInt8 index = 0;
		for (ModEntry* entry = &(*g_dataHandler)->modList; entry && index < 0xFF; entry = entry->next)
		{
			if (entry->IsLoaded())
				activeModList[index++] = entry;
		}
	}

	return activeModList;
}

UInt8 DataHandler::GetModIndex(const char* modName)
{
	UInt8 modIndex = 0xFF;
	const ModEntry** activeModList = GetActiveModList();

	for (UInt8 idx = 0; idx < 0x100 && activeModList[idx] && modIndex == 0xFF; ++idx)
	{
		if (!_stricmp(activeModList[idx]->data->name, modName))
			modIndex = idx;
	}

	return modIndex;
}

UInt8 DataHandler::GetActiveModCount()
{
	UInt8 count = 0;
	const ModEntry** activeModList = GetActiveModList();

	while (activeModList[count])
		++count;

	return count;
}

const char* DataHandler::GetNthModName(UInt32 modIndex)
{
	const ModEntry** activeModList = GetActiveModList();
	if (modIndex < GetActiveModCount() && activeModList[modIndex]->data)
		return activeModList[modIndex]->data->name;
	return "";
}

const char* TESForm::GetEditorID()
{
	switch (typeID)
	{
	case kFormType_Quest:
		return static_cast<TESQuest*>(this)->editorName.m_data;

	case kFormType_Cell:
	{
		TESObjectCELL* cell = static_cast<TESObjectCELL*>(this);
		ExtraEditorID* xData = static_cast<ExtraEditorID*>(FindExtraByType(&cell->extraData, kExtraData_EditorID));
		return xData ? xData->editorID.m_data : nullptr;
	}

	case kFormType_WorldSpace:
		return static_cast<TESWorldSpace*>(this)->editorID.m_data;

	default:
		return nullptr;
	}
}

TESClass* PlayerCharacter::GetPlayerClass() const
{
	if (!baseForm || baseForm->typeID != kFormType_NPC)
		return nullptr;

	return static_cast<TESNPC*>(baseForm)->npcClass;
}
