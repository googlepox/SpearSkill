#include "SpearWeaponTypeSidecar.h"
#include "..\shared\SidecarSkillProvider.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <windows.h>
#include <commctrl.h>

#ifndef CB_SETMINVISIBLE
#define CB_SETMINVISIBLE 0x1701
#endif

typedef unsigned char UInt8;
typedef unsigned short UInt16;
typedef unsigned int UInt32;
typedef signed int SInt32;
typedef signed short SInt16;
typedef unsigned int PluginHandle;

#include "SpearNpcSkillSidecar.h"

struct PluginInfo
{
	enum
	{
		kInfoVersion = 1
	};

	UInt32 infoVersion;
	const char* name;
	UInt32 version;
};

struct OBSEInterface
{
	UInt32 obseVersion;
	UInt32 oblivionVersion;
	UInt32 editorVersion;
	UInt32 isEditor;
	bool(*RegisterCommand)(void*);
	void(*SetOpcodeBase)(UInt32);
	void*(*QueryInterface)(UInt32);
	PluginHandle(*GetPluginHandle)(void);
	bool(*RegisterTypedCommand)(void*, UInt32);
	const char*(*GetOblivionDirectory)();
	bool(*GetPluginLoaded)(const char*);
	UInt32(*GetPluginVersion)(const char*);
};

namespace SpearSkillCSE
{
	static constexpr UInt32 kPluginVersion = 1;
	static constexpr UInt32 kWeaponDialogInitPatch = 0x51D710;
	static constexpr UInt32 kWeaponDialogInitPatchLength = 7;
	static constexpr UInt32 kObjectWindowListViewGetDispInfoPatch = 0x414E60;
	static constexpr UInt32 kObjectWindowListViewGetDispInfoPatchLength = 14;
	static constexpr UInt32 kNpcStatsPopulateVtableEntry = 0x944328;
	static constexpr UInt32 kNpcStatsPopulateOriginal = 0x4CC3C0;
	static constexpr UInt32 kNpcStatsInitSkillListOpenCall = 0x4DAFAC;
	static constexpr UInt32 kNpcStatsInitSkillListCall = 0x4D76FC;
	static constexpr UInt32 kNpcStatsInitSkillListOriginal = 0x4D6160;
	static constexpr UInt32 kNpcStatsSkillEditAutoCalcGatePatch = 0x4DC70D;
	static constexpr UInt32 kNpcStatsSkillEditAutoCalcContinue = 0x4DC715;
	static constexpr UInt32 kNpcStatsSkillEditAutoCalcRejectReturn = 0x4DB81D;
	static constexpr UInt32 kNpcStatsDialogSuccessReturn = 0x4DD6FD;
	static constexpr UInt32 kNpcStatsSkillEditCommitPatch = 0x4DC75B;
	static constexpr UInt32 kNpcStatsSkillEditCommitReturn = 0x4DC765;
	static constexpr UInt32 kAiFormInitDialogControls = 0x48CC50;
	static constexpr UInt32 kAiFormInitDialogControlsPatchLength = 8;
	static constexpr UInt32 kAiFormSaveDialogControls = 0x48CE80;
	static constexpr UInt32 kAiFormSaveDialogControlsPatchLength = 5;
	static constexpr UInt32 kSavePluginPreWritePatch = 0x47EC2F;
	static constexpr UInt32 kSavePluginPreWriteReturn = 0x47EC34;
	static constexpr UInt32 kSavePluginPreWriteOriginalCall = 0x416E50;
	static constexpr UInt32 kWinDialogWndGetExtraDataByType = 0x442990;
	static constexpr UInt32 kRuntimeDynamicCast = 0x88DC0C;
	static constexpr UInt32 kRttiTesAiFormTypeDescriptor = 0x9EDD38;
	static constexpr UInt32 kRttiTesNpcTypeDescriptor = 0x9EA5C8;
	static constexpr UInt32 kCsFormHeapAllocate = 0x401E80;
	static constexpr UInt32 kCsFormHeapFree = 0x401EA0;
	static constexpr UInt32 kTesFormRefIdOffset = 0x0C;
	static constexpr UInt32 kTesFormTypeOffset = 0x04;
	static constexpr UInt32 kTesFormEditorIdOffset = 0x10;
	static constexpr UInt32 kTesFileFileNameOffset = 0x1C;
	static constexpr UInt32 kTesFileFileIndexOffset = 0x400;
	static constexpr UInt32 kDataHandlerActiveFileOffset = 0x0E04;
	static constexpr UInt32 kDataHandlerFilesByIdOffset = 0x0E14;
	static constexpr UInt32 kBookFullNameOffset = 0x58;
	static constexpr UInt32 kBookDescriptionOffset = 0xCC;
	static constexpr UInt32 kBookFlagsOffset = 0xDC;
	static constexpr UInt32 kDialogExtraWorkingDataOwnerOffset = 0x0C;
	static constexpr UInt8 kDialogExtraWorkingDataType = 6;
	static constexpr UInt32 kBoundObjectListCountOffset = 0x00;
	static constexpr UInt32 kBoundObjectListFirstOffset = 0x04;
	static constexpr UInt32 kTesObjectNextOffset = 0x30;
	static constexpr UInt32 kBsStringDataOffset = 0x00;
	static constexpr UInt32 kBsStringValueOffset = 0x04;
	static constexpr UInt8 kFormTypeBook = 0x15;
	static constexpr UInt8 kFormTypeWeapon = 0x21;
	static constexpr UInt8 kFormTypeNpc = 0x23;
	static constexpr UInt8 kBookCantBeTakenFlag = 0x02;
	static constexpr UInt32 kDefaultGeneratedWeaponTypeCarrierObjectId = 0x901;
	static constexpr UInt32 kDefaultGeneratedNpcSkillCarrierObjectId = 0x902;
	static constexpr UInt32 kDefaultGeneratedNpcTrainingCarrierObjectId = 0x903;
	static constexpr char kWeaponTypeCarrierEditorId[] = "SpearSkillWeaponTypeData";
	static constexpr char kWeaponTypeCarrierFullName[] = "Spear Skill Weapon Type Data";
	static constexpr char kNpcSkillCarrierEditorId[] = "SpearSkillNPCSkillData";
	static constexpr char kNpcSkillCarrierFullName[] = "Spear Skill NPC Skill Data";
	static constexpr char kNpcTrainingCarrierEditorId[] = "SpearSkillNPCTrainingData";
	static constexpr char kNpcTrainingCarrierFullName[] = "Spear Skill NPC Training Data";
	static constexpr const char* kWeaponTypeComboPatchedProp = "SpearSkillWeaponTypeComboPatched";
	static constexpr const char* kWeaponTypeComboOriginalProcProp = "SpearSkillWeaponTypeComboOriginalProc";
	static constexpr const char* kWeaponTypeComboLastNativeSelectionProp = "SpearSkillWeaponTypeComboLastNativeSelection";
	static constexpr const char* kWeaponTypeComboScrollableProp = "SpearSkillWeaponTypeComboScrollable";
	static constexpr const char* kWeaponTypeComboApplyingStoredSelectionProp = "SpearSkillWeaponTypeComboApplyingStoredSelection";
	static constexpr const char* kWeaponTypeComboStoredSelectionAppliedProp = "SpearSkillWeaponTypeComboStoredSelectionApplied";
	static constexpr const char* kWeaponTypeComboRejectedProp = "SpearSkillWeaponTypeComboRejected";
	static constexpr const char* kWeaponTypeComboBoundFormPtrProp = "SpearSkillWeaponTypeComboBoundFormPtr";
	static constexpr const char* kWeaponTypeComboBoundFormIdProp = "SpearSkillWeaponTypeComboBoundFormId";
	static constexpr const char* kWeaponTypeParentPatchedProp = "SpearSkillWeaponTypeParentPatched";
	static constexpr const char* kWeaponTypeParentOriginalProcProp = "SpearSkillWeaponTypeParentOriginalProc";
	static constexpr const char* kWeaponTypeParentComboProp = "SpearSkillWeaponTypeParentCombo";
	static constexpr const char* kTrainingComboPatchedProp = "SpearSkillTrainingComboPatched";
	static constexpr const char* kTrainingComboOriginalProcProp = "SpearSkillTrainingComboOriginalProc";
	static constexpr const char* kTrainingComboScrollableProp = "SpearSkillTrainingComboScrollable";
	static constexpr const char* kTrainingComboApplyingSelectionProp = "SpearSkillTrainingComboApplyingSelection";
	static constexpr const char* kTrainingComboBoundNpcFormIdProp = "SpearSkillTrainingComboBoundNpcFormId";
	static constexpr const char* kTrainingParentPatchedProp = "SpearSkillTrainingParentPatched";
	static constexpr const char* kTrainingParentOriginalProcProp = "SpearSkillTrainingParentOriginalProc";
	static constexpr const char* kTrainingParentComboProp = "SpearSkillTrainingParentCombo";
	static constexpr const char* kNativeWeaponTypeBladeOneHand = "Blade - One Hand";
	static constexpr const char* kNativeWeaponTypeBladeTwoHand = "Blade - Two Hand";
	static constexpr const char* kNativeWeaponTypeBluntOneHand = "Blunt - One Hand";
	static constexpr const char* kNativeWeaponTypeBluntTwoHand = "Blunt - Two Hand";
	static constexpr const char* kNativeWeaponTypeStaff = "Staff";
	static constexpr const char* kNativeWeaponTypeBow = "Bow";
	static constexpr const char* kSpearEditorLabel = "Spear";
	static constexpr UINT_PTR kWeaponTypeComboPollTimerId = 0x5A91;
	static constexpr UINT kWeaponTypeComboPollIntervalMs = 1000;
	static constexpr int kWeaponTypeComboMinimumVisibleRows = 9;
	static constexpr int kTrainingComboMinimumVisibleRows = 12;
	static constexpr UINT kCseDeferredComboAddItem = WM_USER + 3000;
	static constexpr LRESULT kCseDeferredAddStringMarkerResult = CB_ERRSPACE - 2;
	static constexpr int kWeaponDialogTypeComboId = 1165;
	static constexpr int kNpcStatsSkillsList = 1087;
	static constexpr int kAiTrainingCheckBox = 1542;
	static constexpr int kAiTrainingSkillCombo = 1040;
	static constexpr int kAiTrainingLevelEdit = 1061;
	static constexpr UInt32 kNativeBladeActorValue = 14;
	static constexpr UInt8 kNpcStatsSpearDefaultSkillValue = 5;
	static constexpr UInt8 kNpcStatsSpearSortSkillIndex = 0xE4;
	static constexpr UInt8 kNpcStatsSpearRowMarker = 0x5A;

	static constexpr UInt8 kExpectedNpcStatsInitSkillListCall[] =
	{
		0xE8, 0x5F, 0xEA, 0xFF, 0xFF
	};

	static constexpr UInt8 kExpectedNpcStatsInitSkillListOpenCall[] =
	{
		0xE8, 0xAF, 0xB1, 0xFF, 0xFF
	};

	static constexpr UInt8 kExpectedNpcStatsSkillEditCommit[] =
	{
		0x8B, 0x44, 0x24, 0x2C, 0xC7, 0x00, 0x01, 0x00, 0x00, 0x00
	};

	static constexpr UInt8 kExpectedNpcStatsSkillEditAutoCalcGate[] =
	{
		0x84, 0xC0,
		0x0F, 0x85, 0x08, 0xF1, 0xFF, 0xFF
	};

	static constexpr UInt8 kExpectedObjectWindowListViewGetDispInfo[] =
	{
		0x55, 0x8D, 0xAC, 0x24, 0x4C, 0xFF, 0xFF, 0xFF,
		0x81, 0xEC, 0x34, 0x01, 0x00, 0x00
	};

	static constexpr UInt8 kExpectedAiFormInitDialogControls[kAiFormInitDialogControlsPatchLength] =
	{
		0x53, 0x55, 0x8B, 0x2D, 0x68, 0x44, 0x92, 0x00
	};

	static constexpr UInt8 kExpectedAiFormSaveDialogControls[kAiFormSaveDialogControlsPatchLength] =
	{
		0x53, 0x8B, 0x5C, 0x24, 0x08
	};

	static constexpr UInt8 kExpectedSavePluginPreWrite[] =
	{
		0xE8, 0x1C, 0x82, 0xF9, 0xFF
	};

	static constexpr UInt8 kExpectedWeaponDialogInit[] =
	{
		0x53,
		0x8B, 0x1D, 0xC8, 0x44, 0x92, 0x00
	};

	typedef void* (__stdcall* LookupEditorFormByEditorIDFn)(const char* editorId);
	typedef void* (__cdecl* LookupEditorFormByFormIDFn)(UInt32 formId);
	typedef void* (__cdecl* CreateEditorFormFn)(UInt8 formType);
	typedef bool(__thiscall* SetEditorIDFn)(void* form, const char* editorId);
	typedef void(__thiscall* SetFormIDFn)(void* form, UInt32 formId, bool reserve);
	typedef bool(__thiscall* SetBSStringFn)(void* bsString, const char* value, SInt16 size);
	typedef void(__thiscall* AddObjectFn)(void* objectList, void* object);
	typedef void* (__thiscall* GetOverrideFileFn)(void* form, int index);
	typedef void* (__cdecl* CsFormHeapAllocateFn)(UInt32 size);
	typedef void(__cdecl* CsFormHeapFreeFn)(void* ptr);
	typedef void* (__cdecl* WinDialogWndGetExtraDataByTypeFn)(HWND dialog, int type);
	typedef void* (__cdecl* RuntimeDynamicCastFn)(void* object, int vfDelta, void* sourceType, void* targetType, int isReference);
	using WeaponDialogInitFn = int(__thiscall*)(void* weapon, HWND dialog);
	using ObjectWindowListViewGetDispInfoFn = void(__thiscall*)(void* treeEntry, NMLVDISPINFOA* data);
	using NpcStatsPopulateFn = int(__thiscall*)(void* npc, HWND dialog);
	using NpcStatsPopulateChainedFn = int(__fastcall*)(void* npc, void*, HWND dialog);
	using NpcStatsInitSkillListFn = LRESULT(__thiscall*)(void* npc, HWND list);
	using NpcStatsInitSkillListChainedFn = LRESULT(__fastcall*)(void* npc, void*, HWND list);
	using AiFormInitDialogControlsFn = int(__thiscall*)(void* aiForm, HWND dialog);
	using AiFormSaveDialogControlsFn = int(__thiscall*)(void* aiForm, HWND dialog);

	struct NpcStatsSkillRowData
	{
		UInt8* value;
		UInt8 skillIndex;
		UInt8 sidecarIndex;
		UInt8 marker;
		UInt8 padding;
	};

	static UINT_PTR g_weaponTypeComboPollTimer = 0;
	static bool g_insideWeaponTypeComboPatch = false;
	static SpearSkillShared::WeaponTypeSidecarStore g_weaponTypeStore;
	static SpearSkillShared::EmbeddedPluginNpcSpearBackend g_npcSkillEmbeddedBackend;
	static SpearSkillShared::NpcSpearStore g_npcSkillStore;
	static SpearSkillShared::NpcSpearTrainingStore g_npcTrainingStore;
	static UInt32 g_savePluginPreWriteChainTarget = 0;
	static bool g_weaponTypeStoreLoaded = false;
	static bool g_npcSkillStoreConfigured = false;
	static bool g_npcSkillStoreLoaded = false;
	static bool g_npcTrainingStoreLoaded = false;
	static void* g_npcTrainingStoreLoadedActiveFile = nullptr;
	static bool g_loggedWeaponTypeNoForm = false;
	static bool g_loggedWeaponTypeNoActivePlugin = false;
	static bool g_loggedLegacySelfWeaponTypeFallback = false;
	static bool g_loggedLegacySelfWeaponTypeAmbiguous = false;
	static bool g_loggedWeaponTypeCarrierStoreFull = false;
	static bool g_loggedNpcStatsScrollbar = false;
	static bool g_loggedNpcSkillNoActivePlugin = false;
	static bool g_loggedLegacySelfNpcSkillFallback = false;
	static bool g_loggedLegacySelfNpcSkillAmbiguous = false;
	static bool g_loggedNpcSkillCarrierStoreFull = false;
	static bool g_loggedNpcTrainingNoForm = false;
	static bool g_loggedNpcTrainingNoActivePlugin = false;
	static bool g_loggedNpcTrainingCarrierStoreFull = false;
	static bool g_loggedNpcTrainingBadDialogOwner = false;
	static UInt8 g_npcStatsSpearValue = kNpcStatsSpearDefaultSkillValue;
	static NpcStatsSkillRowData g_npcStatsSpearListData =
	{
		&g_npcStatsSpearValue,
		kNpcStatsSpearSortSkillIndex,
		0,
		kNpcStatsSpearRowMarker,
		0
	};
	static UInt32 g_activeNpcStatsFormId = 0;
	static HWND g_activeNpcStatsDialog = nullptr;
	static HWND g_activeNpcStatsSkillsList = nullptr;
	static WeaponDialogInitFn g_weaponDialogInitOriginal = nullptr;
	static UInt8 g_weaponDialogInitTrampoline[kWeaponDialogInitPatchLength + 5] = {};
	static ObjectWindowListViewGetDispInfoFn g_objectWindowListViewGetDispInfoOriginal = nullptr;
	static AiFormInitDialogControlsFn g_aiFormInitDialogControlsOriginal = nullptr;
	static AiFormSaveDialogControlsFn g_aiFormSaveDialogControlsOriginal = nullptr;
	static UInt8 g_objectWindowListViewGetDispInfoTrampoline[kObjectWindowListViewGetDispInfoPatchLength + 5] = {};
	static UInt8 g_aiFormInitDialogControlsTrampoline[kAiFormInitDialogControlsPatchLength + 5] = {};
	static UInt8 g_aiFormSaveDialogControlsTrampoline[kAiFormSaveDialogControlsPatchLength + 5] = {};
	static UInt32 g_npcStatsPopulateOriginalTarget = kNpcStatsPopulateOriginal;
	static bool g_npcStatsPopulateOriginalIsFastcall = false;
	static UInt32 g_npcStatsInitSkillListOriginalTarget = kNpcStatsInitSkillListOriginal;
	static bool g_npcStatsInitSkillListOriginalIsFastcall = false;
	static UInt32 g_npcStatsSkillEditAutoCalcGateChainTarget = 0;
	static UInt32 g_npcStatsSkillEditCommitChainTarget = 0;

	static bool ReadComboString(HWND combo, int index, std::string& value);
	static WNDPROC GetOriginalWeaponTypeComboProc(HWND combo);
	static void PersistWeaponTypeSelection(HWND combo);
	static bool ApplyStoredWeaponTypeSelection(HWND combo);
	static bool PrepareNativeWeaponTypeSelectionForSave(HWND combo, int* restoreSelection);
	static void RestoreWeaponTypeSelectionAfterSave(HWND combo, int restoreSelection);
	static void PatchWeaponTypeParentDialog(HWND combo);
	static void PatchWeaponTypeCombo(HWND combo);
	static void RememberNativeWeaponTypeSelection(HWND combo);
	static void SelectTrainingComboSpear(HWND combo);
	static void* LookupUniqueWeaponFormByEditorId(const char* editorId);
	static void* LookupUniqueNpcFormByEditorId(const char* editorId);
	static bool IsNpcStatsSpearListRow(HWND list, int row);
	static const char* ReadEmbeddedNpcSpearPayload(void* context, bool* found);
	static bool WriteEmbeddedNpcSpearPayload(void* context, const char* payload);
	static bool ResolveNpcSpearKeyToEditorFormID(void* context, const SpearSkillShared::NpcSpearKey& key, UInt32* formId);
	static bool MakeNpcSpearKeyFromEditorFormID(void* context, UInt32 formId, SpearSkillShared::NpcSpearKey* key);
	static void MarkEmbeddedNpcSpearCarrierDirty(void* context);
	static const char* ReadEmbeddedNpcSpearTrainingPayload(bool* found);
	static bool WriteEmbeddedNpcSpearTrainingPayload(const char* payload);
	static void LoadNpcSpearTrainingStoreOnce();
	static bool SaveNpcSpearTrainingStore();
	static bool FlushPendingSidecarStoresBeforePluginSave();

	static void LogLine(const char* level, const char* format, va_list args)
	{
		char body[1024] = {};
		_vsnprintf_s(body, sizeof(body), _TRUNCATE, format ? format : "", args);

		char line[1200] = {};
		_snprintf_s(line, sizeof(line), _TRUNCATE, "SpearSkill CS [%s]: %s\r\n", level ? level : "info", body);
		OutputDebugStringA(line);

		FILE* file = nullptr;
		if (!fopen_s(&file, "SpearSkill_CS.log", "a") && file)
		{
			std::fputs(line, file);
			std::fclose(file);
		}
	}

	static void LogMessage(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		LogLine("info", format, args);
		va_end(args);
	}

	static void LogWarning(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		LogLine("warning", format, args);
		va_end(args);
	}

	static void LogError(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		LogLine("error", format, args);
		va_end(args);
	}

	static void WeaponTypeStoreLog(void*, const char* message)
	{
		LogMessage("%s", message ? message : "");
	}

	static void NpcSkillStoreLog(void*, const char* message)
	{
		LogMessage("%s", message ? message : "");
	}

	static void EnsureNpcSpearStoreConfigured()
	{
		if (g_npcSkillStoreConfigured)
			return;

		g_npcSkillEmbeddedBackend.Configure(ReadEmbeddedNpcSpearPayload, WriteEmbeddedNpcSpearPayload,
			ResolveNpcSpearKeyToEditorFormID, MakeNpcSpearKeyFromEditorFormID, MarkEmbeddedNpcSpearCarrierDirty,
			nullptr, kNpcStatsSpearDefaultSkillValue);
		g_npcSkillStore.Configure(&g_npcSkillEmbeddedBackend, NpcSkillStoreLog, nullptr);
		g_npcSkillStoreConfigured = true;
	}

	static BOOL CALLBACK InvalidateVisibleListViews(HWND hwnd, LPARAM)
	{
		if (!IsWindowVisible(hwnd))
			return TRUE;

		char className[32] = {};
		if (GetClassNameA(hwnd, className, sizeof(className)) && !_stricmp(className, "SysListView32"))
		{
			InvalidateRect(hwnd, nullptr, FALSE);
			UpdateWindow(hwnd);
			return TRUE;
		}

		EnumChildWindows(hwnd, InvalidateVisibleListViews, 0);
		return TRUE;
	}

	static void RefreshVisibleListViews()
	{
		EnumThreadWindows(GetCurrentThreadId(), InvalidateVisibleListViews, 0);
	}

	static void RefreshWeaponTypeComboDisplay(HWND combo)
	{
		if (!combo)
			return;

		RedrawWindow(combo, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
		if (HWND parent = GetParent(combo))
			RedrawWindow(parent, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}

	static void* GetEditorDataHandler()
	{
		void** dataHandler = reinterpret_cast<void**>(0x00A0E064);
		return dataHandler ? *dataHandler : nullptr;
	}

	static void* GetActiveEditorFile()
	{
		void* dataHandler = GetEditorDataHandler();
		return dataHandler ? *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(dataHandler) + kDataHandlerActiveFileOffset) : nullptr;
	}

	static const char* GetEditorFileName(const void* file)
	{
		return file ? reinterpret_cast<const char*>(reinterpret_cast<const UInt8*>(file) + kTesFileFileNameOffset) : nullptr;
	}

	static UInt8 GetEditorFileIndex(const void* file)
	{
		return file ? *reinterpret_cast<const UInt8*>(reinterpret_cast<const UInt8*>(file) + kTesFileFileIndexOffset) : 0xFF;
	}

	static void* LookupEditorFileByName(const char* fileName)
	{
		if (!fileName || !fileName[0])
			return nullptr;

		void* dataHandler = GetEditorDataHandler();
		if (!dataHandler)
			return nullptr;

		void** filesById = reinterpret_cast<void**>(reinterpret_cast<UInt8*>(dataHandler) + kDataHandlerFilesByIdOffset);
		for (UInt32 i = 0; i < 0xFF; ++i)
		{
			void* file = filesById[i];
			const char* candidate = GetEditorFileName(file);
			if (candidate && !_stricmp(candidate, fileName))
				return file;
		}

		return nullptr;
	}

	static const char* ReadBSStringValue(const void* bsString)
	{
		if (!bsString)
			return nullptr;

		return *reinterpret_cast<char* const*>(reinterpret_cast<const UInt8*>(bsString) + kBsStringDataOffset);
	}

	static bool SetBSStringValue(void* bsString, const char* value)
	{
		static const SetBSStringFn setString = reinterpret_cast<SetBSStringFn>(0x004051E0);
		return bsString && setString && setString(bsString, value ? value : "", 0);
	}

	static UInt8 ReadEditorFormType(const void* form)
	{
		return form ? *reinterpret_cast<const UInt8*>(reinterpret_cast<const UInt8*>(form) + kTesFormTypeOffset) : 0;
	}

	static UInt8 TryReadEditorFormType(const void* form)
	{
		if (!form)
			return 0;

		const void* address = reinterpret_cast<const UInt8*>(form) + kTesFormTypeOffset;
		if (IsBadReadPtr(address, sizeof(UInt8)))
			return 0;

		return ReadEditorFormType(form);
	}

	static UInt32 ReadEditorFormId(const void* form)
	{
		return form ? *reinterpret_cast<const UInt32*>(reinterpret_cast<const UInt8*>(form) + kTesFormRefIdOffset) : 0;
	}

	static const char* ReadEditorFormEditorId(const void* form)
	{
		return form ? ReadBSStringValue(reinterpret_cast<const UInt8*>(form) + kTesFormEditorIdOffset) : nullptr;
	}

	static void* LookupEditorFormById(UInt32 formId)
	{
		static const LookupEditorFormByFormIDFn lookupByFormId = reinterpret_cast<LookupEditorFormByFormIDFn>(0x00495EF0);
		return lookupByFormId ? lookupByFormId(formId) : nullptr;
	}

	static void MarkEditorFormFromActiveFile(void* form, bool active)
	{
		if (!form)
			return;

		void*** vtbl = reinterpret_cast<void***>(form);
		if (!vtbl || !*vtbl)
			return;

		typedef void(__thiscall* SetFromActiveFileFn)(void*, bool);
		SetFromActiveFileFn setFromActiveFile = reinterpret_cast<SetFromActiveFileFn>((*vtbl)[0x94 / sizeof(void*)]);
		if (setFromActiveFile)
			setFromActiveFile(form, active);
	}

	static void* GetEditorFormOverrideFile(void* form, int index)
	{
		static const GetOverrideFileFn getOverrideFile = reinterpret_cast<GetOverrideFileFn>(0x00495FE0);
		return form && getOverrideFile ? getOverrideFile(form, index) : nullptr;
	}

	static void* LookupWeaponTypeCarrierBook()
	{
		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(kWeaponTypeCarrierEditorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeBook ? form : nullptr;
	}

	static void* FindOrCreateWeaponTypeCarrierBook()
	{
		if (void* existing = LookupWeaponTypeCarrierBook())
			return existing;

		void* dataHandler = GetEditorDataHandler();
		if (!dataHandler || !GetActiveEditorFile())
		{
			LogWarning("cannot create embedded Spear weapon Type carrier without an active plugin");
			return nullptr;
		}

		static const CreateEditorFormFn createForm = reinterpret_cast<CreateEditorFormFn>(0x004793F0);
		static const SetEditorIDFn setEditorId = reinterpret_cast<SetEditorIDFn>(0x00497670);
		static const AddObjectFn addObject = reinterpret_cast<AddObjectFn>(0x005135F0);
		void* book = createForm ? createForm(kFormTypeBook) : nullptr;
		if (!book)
			return nullptr;

		if (!ReadEditorFormId(book))
		{
			const UInt32 nextFormId = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(dataHandler) + 0x0E00);
			static const SetFormIDFn setFormId = reinterpret_cast<SetFormIDFn>(0x00497E50);
			if (setFormId)
				setFormId(book, nextFormId ? nextFormId : kDefaultGeneratedWeaponTypeCarrierObjectId, true);
		}

		if (setEditorId)
			setEditorId(book, kWeaponTypeCarrierEditorId);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookFullNameOffset + kBsStringValueOffset, kWeaponTypeCarrierFullName);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookDescriptionOffset + kBsStringValueOffset, SpearSkillShared::WeaponTypeSidecarPayloadCodec::Header());
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(book) + kBookFlagsOffset) |= kBookCantBeTakenFlag;

		void* objectList = *reinterpret_cast<void**>(dataHandler);
		if (addObject && objectList)
			addObject(objectList, book);

		MarkEditorFormFromActiveFile(book, true);
		LogMessage("created embedded BOOK carrier %s", kWeaponTypeCarrierEditorId);
		return book;
	}

	static const char* ReadEmbeddedWeaponTypePayload(bool* found)
	{
		void* carrier = LookupWeaponTypeCarrierBook();
		if (found)
			*found = carrier != nullptr;
		if (!carrier)
			return nullptr;

		return ReadBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset);
	}

	static bool WriteEmbeddedWeaponTypePayload(const char* payload)
	{
		void* carrier = FindOrCreateWeaponTypeCarrierBook();
		if (!carrier)
			return false;

		SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookFullNameOffset + kBsStringValueOffset, kWeaponTypeCarrierFullName);
		const bool written = SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset, payload ? payload : "");
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(carrier) + kBookFlagsOffset) |= kBookCantBeTakenFlag;
		MarkEditorFormFromActiveFile(carrier, true);
		return written;
	}

	static void* LookupNpcSkillCarrierBook()
	{
		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(kNpcSkillCarrierEditorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeBook ? form : nullptr;
	}

	static void* FindOrCreateNpcSkillCarrierBook()
	{
		if (void* existing = LookupNpcSkillCarrierBook())
			return existing;

		void* dataHandler = GetEditorDataHandler();
		if (!dataHandler || !GetActiveEditorFile())
		{
			LogWarning("cannot create embedded NPC Spear skill carrier without an active plugin");
			return nullptr;
		}

		static const CreateEditorFormFn createForm = reinterpret_cast<CreateEditorFormFn>(0x004793F0);
		static const SetEditorIDFn setEditorId = reinterpret_cast<SetEditorIDFn>(0x00497670);
		static const AddObjectFn addObject = reinterpret_cast<AddObjectFn>(0x005135F0);
		void* book = createForm ? createForm(kFormTypeBook) : nullptr;
		if (!book)
			return nullptr;

		if (!ReadEditorFormId(book))
		{
			const UInt32 nextFormId = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(dataHandler) + 0x0E00);
			static const SetFormIDFn setFormId = reinterpret_cast<SetFormIDFn>(0x00497E50);
			if (setFormId)
				setFormId(book, nextFormId ? nextFormId : kDefaultGeneratedNpcSkillCarrierObjectId, true);
		}

		if (setEditorId)
			setEditorId(book, kNpcSkillCarrierEditorId);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookFullNameOffset + kBsStringValueOffset, kNpcSkillCarrierFullName);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookDescriptionOffset + kBsStringValueOffset, SpearSkillShared::NpcSpearPayloadCodec::Header());
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(book) + kBookFlagsOffset) |= kBookCantBeTakenFlag;

		void* objectList = *reinterpret_cast<void**>(dataHandler);
		if (addObject && objectList)
			addObject(objectList, book);

		MarkEditorFormFromActiveFile(book, true);
		LogMessage("created embedded BOOK carrier %s", kNpcSkillCarrierEditorId);
		return book;
	}

	static const char* ReadEmbeddedNpcSpearPayload(void*, bool* found)
	{
		void* carrier = LookupNpcSkillCarrierBook();
		if (found)
			*found = carrier != nullptr;
		if (!carrier)
			return nullptr;

		return ReadBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset);
	}

	static bool WriteEmbeddedNpcSpearPayload(void*, const char* payload)
	{
		void* carrier = FindOrCreateNpcSkillCarrierBook();
		if (!carrier)
			return false;

		SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookFullNameOffset + kBsStringValueOffset, kNpcSkillCarrierFullName);
		const bool written = SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset, payload ? payload : "");
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(carrier) + kBookFlagsOffset) |= kBookCantBeTakenFlag;
		MarkEditorFormFromActiveFile(carrier, true);
		return written;
	}

	static void* LookupNpcTrainingCarrierBook()
	{
		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(kNpcTrainingCarrierEditorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeBook ? form : nullptr;
	}

	static void* FindOrCreateNpcTrainingCarrierBook()
	{
		if (void* existing = LookupNpcTrainingCarrierBook())
			return existing;

		void* dataHandler = GetEditorDataHandler();
		if (!dataHandler || !GetActiveEditorFile())
		{
			LogWarning("cannot create embedded NPC Spear training carrier without an active plugin");
			return nullptr;
		}

		static const CreateEditorFormFn createForm = reinterpret_cast<CreateEditorFormFn>(0x004793F0);
		static const SetEditorIDFn setEditorId = reinterpret_cast<SetEditorIDFn>(0x00497670);
		static const AddObjectFn addObject = reinterpret_cast<AddObjectFn>(0x005135F0);
		void* book = createForm ? createForm(kFormTypeBook) : nullptr;
		if (!book)
			return nullptr;

		if (!ReadEditorFormId(book))
		{
			const UInt32 nextFormId = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(dataHandler) + 0x0E00);
			static const SetFormIDFn setFormId = reinterpret_cast<SetFormIDFn>(0x00497E50);
			if (setFormId)
				setFormId(book, nextFormId ? nextFormId : kDefaultGeneratedNpcTrainingCarrierObjectId, true);
		}

		if (setEditorId)
			setEditorId(book, kNpcTrainingCarrierEditorId);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookFullNameOffset + kBsStringValueOffset, kNpcTrainingCarrierFullName);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookDescriptionOffset + kBsStringValueOffset, SpearSkillShared::NpcSpearTrainingPayloadCodec::Header());
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(book) + kBookFlagsOffset) |= kBookCantBeTakenFlag;

		void* objectList = *reinterpret_cast<void**>(dataHandler);
		if (addObject && objectList)
			addObject(objectList, book);

		MarkEditorFormFromActiveFile(book, true);
		LogMessage("created embedded BOOK carrier %s", kNpcTrainingCarrierEditorId);
		return book;
	}

	static const char* ReadEmbeddedNpcSpearTrainingPayload(bool* found)
	{
		void* carrier = LookupNpcTrainingCarrierBook();
		if (found)
			*found = carrier != nullptr;
		if (!carrier)
			return nullptr;

		return ReadBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset);
	}

	static bool WriteEmbeddedNpcSpearTrainingPayload(const char* payload)
	{
		void* carrier = FindOrCreateNpcTrainingCarrierBook();
		if (!carrier)
			return false;

		SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookFullNameOffset + kBsStringValueOffset, kNpcTrainingCarrierFullName);
		const bool written = SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset, payload ? payload : "");
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(carrier) + kBookFlagsOffset) |= kBookCantBeTakenFlag;
		MarkEditorFormFromActiveFile(carrier, true);
		return written;
	}

	static bool ResolveWeaponTypeKeyToEditorFormID(const SpearSkillShared::WeaponTypeSidecarKey& key, UInt32* formId)
	{
		if (formId)
			*formId = 0;

		const bool self = !_stricmp(key.sourceMod, "$SELF");
		void* file = self ? GetActiveEditorFile() : LookupEditorFileByName(key.sourceMod);
		const UInt8 fileIndex = GetEditorFileIndex(file);
		if (!file || fileIndex == 0xFF)
			return false;

		const UInt32 candidateFormId = (static_cast<UInt32>(fileIndex) << 24) | (key.objectId & 0x00FFFFFF);
		void* form = LookupEditorFormById(candidateFormId);
		if (ReadEditorFormType(form) == kFormTypeWeapon)
		{
			if (formId)
				*formId = candidateFormId;
			return true;
		}

		if (self && key.editorId[0])
		{
			form = LookupUniqueWeaponFormByEditorId(key.editorId);
			if (ReadEditorFormType(form) == kFormTypeWeapon)
			{
				if (formId)
					*formId = ReadEditorFormId(form);
				if (!g_loggedLegacySelfWeaponTypeFallback)
				{
					LogMessage("resolved legacy $SELF Spear weapon Type sidecar row by editor ID %s", key.editorId);
					g_loggedLegacySelfWeaponTypeFallback = true;
				}
				return true;
			}
		}

		if (formId)
			*formId = 0;
		return false;
	}

	static bool MakeWeaponTypeKeyFromEditorForm(void* form, SpearSkillShared::WeaponTypeSidecarKey* key)
	{
		if (!key || ReadEditorFormType(form) != kFormTypeWeapon)
			return false;

		void* activeFile = GetActiveEditorFile();
		void* sourceFile = GetEditorFormOverrideFile(form, 0);
		std::memset(key, 0, sizeof(*key));
		if (sourceFile && sourceFile != activeFile)
			_snprintf_s(key->sourceMod, sizeof(key->sourceMod), _TRUNCATE, "%s", GetEditorFileName(sourceFile) ? GetEditorFileName(sourceFile) : "$SELF");
		else
			_snprintf_s(key->sourceMod, sizeof(key->sourceMod), _TRUNCATE, "$SELF");

		key->objectId = ReadEditorFormId(form) & 0x00FFFFFF;
		_snprintf_s(key->editorId, sizeof(key->editorId), _TRUNCATE, "%s", ReadEditorFormEditorId(form) ? ReadEditorFormEditorId(form) : "");
		return key->objectId != 0;
	}

	static bool MakeWeaponTypeKeyFromEditorFormID(UInt32 formId, SpearSkillShared::WeaponTypeSidecarKey* key)
	{
		void* form = LookupEditorFormById(formId);
		return MakeWeaponTypeKeyFromEditorForm(form, key);
	}

	static bool ResolveNpcSpearKeyToEditorFormID(void*, const SpearSkillShared::NpcSpearKey& key, UInt32* formId)
	{
		if (formId)
			*formId = 0;

		const bool self = !_stricmp(key.sourceMod, "$SELF");
		void* file = self ? GetActiveEditorFile() : LookupEditorFileByName(key.sourceMod);
		const UInt8 fileIndex = GetEditorFileIndex(file);
		if (!file || fileIndex == 0xFF)
			return false;

		const UInt32 candidateFormId = (static_cast<UInt32>(fileIndex) << 24) | (key.objectId & 0x00FFFFFF);
		void* form = LookupEditorFormById(candidateFormId);
		if (ReadEditorFormType(form) == kFormTypeNpc)
		{
			if (formId)
				*formId = candidateFormId;
			return true;
		}

		if (self && key.editorId[0])
		{
			form = LookupUniqueNpcFormByEditorId(key.editorId);
			if (ReadEditorFormType(form) == kFormTypeNpc)
			{
				if (formId)
					*formId = ReadEditorFormId(form);
				if (!g_loggedLegacySelfNpcSkillFallback)
				{
					LogMessage("resolved legacy $SELF NPC Spear sidecar row by editor ID %s", key.editorId);
					g_loggedLegacySelfNpcSkillFallback = true;
				}
				return true;
			}
		}

		if (formId)
			*formId = 0;
		return false;
	}

	static bool MakeNpcSpearKeyFromEditorForm(void* form, SpearSkillShared::NpcSpearKey* key)
	{
		if (!key || ReadEditorFormType(form) != kFormTypeNpc)
			return false;

		void* activeFile = GetActiveEditorFile();
		void* sourceFile = GetEditorFormOverrideFile(form, 0);
		void* activeOverrideFile = GetEditorFormOverrideFile(form, -1);
		std::memset(key, 0, sizeof(*key));
		if (!sourceFile || sourceFile == activeFile || activeOverrideFile == activeFile)
			_snprintf_s(key->sourceMod, sizeof(key->sourceMod), _TRUNCATE, "$SELF");
		else
			_snprintf_s(key->sourceMod, sizeof(key->sourceMod), _TRUNCATE, "%s", GetEditorFileName(sourceFile) ? GetEditorFileName(sourceFile) : "$SELF");

		key->objectId = ReadEditorFormId(form) & 0x00FFFFFF;
		_snprintf_s(key->editorId, sizeof(key->editorId), _TRUNCATE, "%s", ReadEditorFormEditorId(form) ? ReadEditorFormEditorId(form) : "");
		return key->objectId != 0;
	}

	static bool MakeNpcSpearKeyFromEditorFormID(void*, UInt32 formId, SpearSkillShared::NpcSpearKey* key)
	{
		void* form = LookupEditorFormById(formId);
		return MakeNpcSpearKeyFromEditorForm(form, key);
	}

	static void MarkEmbeddedNpcSpearCarrierDirty(void*)
	{
		MarkEditorFormFromActiveFile(LookupNpcSkillCarrierBook(), true);
	}

	static void LoadWeaponTypeStoreOnce()
	{
		if (g_weaponTypeStoreLoaded)
			return;

		g_weaponTypeStore.Clear();
		bool found = false;
		const char* payload = ReadEmbeddedWeaponTypePayload(&found);
		if (!found || !payload)
		{
			g_weaponTypeStoreLoaded = true;
			return;
		}

		std::vector<SpearSkillShared::WeaponTypeSidecarPayloadRow> rows;
		SpearSkillShared::WeaponTypeSidecarPayloadStats stats = {};
		stats.carrierRecords = 1;
		if (!SpearSkillShared::WeaponTypeSidecarPayloadCodec::Parse(payload, rows, &stats, WeaponTypeStoreLog, nullptr))
		{
			LogWarning("embedded Spear weapon Type carrier found but payload parse failed");
			g_weaponTypeStoreLoaded = true;
			return;
		}

		for (size_t i = 0; i < rows.size(); ++i)
		{
			UInt32 formId = 0;
			if (!ResolveWeaponTypeKeyToEditorFormID(rows[i].key, &formId) || !formId)
			{
				++stats.unresolvedEntries;
				continue;
			}

			if (g_weaponTypeStore.SetLoaded(formId, rows[i].kind))
			{
				++stats.resolvedEntries;
			}
			else
			{
				++stats.unresolvedEntries;
				if (!g_loggedWeaponTypeCarrierStoreFull)
				{
					LogWarning("embedded Spear weapon Type carrier row could not be stored form=%08X kind=%u", formId, rows[i].kind);
					g_loggedWeaponTypeCarrierStoreFull = true;
				}
			}
		}

		g_weaponTypeStore.ClearDirty();
		g_weaponTypeStoreLoaded = true;
		LogMessage("embedded Spear weapon Type carrier load parsed=%u resolved=%u unresolved=%u skipped=%u",
			stats.parsedEntries,
			stats.resolvedEntries,
			stats.unresolvedEntries,
			stats.skippedRows);
	}

	static void LoadNpcSpearStoreOnce()
	{
		EnsureNpcSpearStoreConfigured();
		if (!g_npcSkillStoreLoaded)
		{
			if (g_npcSkillStore.LoadAuthoritativeEmbedded())
			{
				LogMessage("embedded NPC Spear carrier loaded");
			}
			else
			{
				LogMessage("embedded NPC Spear carrier %s; using sidecar defaults until edited",
					g_npcSkillEmbeddedBackend.LastLoadFoundCarrier() ? "found but not usable" : "not found");
			}
			g_npcSkillStoreLoaded = true;
		}
	}

	static void LoadNpcSpearTrainingStoreOnce()
	{
		void* activeFile = GetActiveEditorFile();
		if (g_npcTrainingStoreLoaded && g_npcTrainingStoreLoadedActiveFile == activeFile)
			return;
		if (g_npcTrainingStore.IsDirty())
			return;

		g_npcTrainingStore.Clear();
		bool found = false;
		const char* payload = ReadEmbeddedNpcSpearTrainingPayload(&found);
		if (!found || !payload)
		{
			g_npcTrainingStoreLoaded = activeFile != nullptr;
			g_npcTrainingStoreLoadedActiveFile = activeFile;
			LogMessage("embedded NPC Spear training carrier %s; using native trainer defaults until edited",
				activeFile ? "not found" : "not available yet");
			return;
		}

		std::vector<SpearSkillShared::NpcSpearTrainingPayloadRow> rows;
		SpearSkillShared::NpcSpearPayloadStats stats = {};
		stats.carrierRecords = 1;
		if (!SpearSkillShared::NpcSpearTrainingPayloadCodec::Parse(payload, rows, &stats, NpcSkillStoreLog, nullptr))
		{
			LogWarning("embedded NPC Spear training carrier found but payload parse failed");
			g_npcTrainingStoreLoaded = true;
			g_npcTrainingStoreLoadedActiveFile = activeFile;
			return;
		}

		for (size_t i = 0; i < rows.size(); ++i)
		{
			UInt32 formId = 0;
			if (!ResolveNpcSpearKeyToEditorFormID(nullptr, rows[i].key, &formId) || !formId)
			{
				++stats.unresolvedEntries;
				continue;
			}

			if (g_npcTrainingStore.SetLoaded(formId))
			{
				++stats.resolvedEntries;
			}
			else
			{
				++stats.unresolvedEntries;
				if (!g_loggedNpcTrainingCarrierStoreFull)
				{
					LogWarning("embedded NPC Spear training carrier row could not be stored form=%08X", formId);
					g_loggedNpcTrainingCarrierStoreFull = true;
				}
			}
		}

		g_npcTrainingStore.ClearDirty();
		g_npcTrainingStoreLoaded = true;
		g_npcTrainingStoreLoadedActiveFile = activeFile;
		LogMessage("embedded NPC Spear training carrier load parsed=%u resolved=%u unresolved=%u skipped=%u",
			stats.parsedEntries,
			stats.resolvedEntries,
			stats.unresolvedEntries,
			stats.skippedRows);
	}

	static bool SaveWeaponTypeStore()
	{
		std::vector<SpearSkillShared::WeaponTypeSidecarPayloadRow> rows;
		UInt32 skipped = 0;
		for (UInt32 i = 0; i < g_weaponTypeStore.Count(); ++i)
		{
			const SpearSkillShared::WeaponTypeSidecarEntry& entry = g_weaponTypeStore.EntryAt(i);
			if (!entry.formId)
				continue;

			SpearSkillShared::WeaponTypeSidecarPayloadRow row = {};
			if (!MakeWeaponTypeKeyFromEditorFormID(entry.formId, &row.key))
			{
				++skipped;
				continue;
			}

			row.kind = entry.kind;
			rows.push_back(row);
		}

		const std::string payload = SpearSkillShared::WeaponTypeSidecarPayloadCodec::Write(rows);
		if (!WriteEmbeddedWeaponTypePayload(payload.c_str()))
		{
			LogWarning("failed to save Spear weapon Type sidecar carrier entries=%u skipped=%u",
				static_cast<unsigned int>(rows.size()),
				skipped);
			return false;
		}

		g_weaponTypeStore.ClearDirty();
		return true;
	}

	static bool SaveNpcSpearTrainingStore()
	{
		std::vector<SpearSkillShared::NpcSpearTrainingPayloadRow> rows;
		UInt32 skipped = 0;
		for (UInt32 i = 0; i < g_npcTrainingStore.Count(); ++i)
		{
			const SpearSkillShared::NpcSpearTrainingEntry& entry = g_npcTrainingStore.EntryAt(i);
			if (!entry.formId)
				continue;

			SpearSkillShared::NpcSpearTrainingPayloadRow row = {};
			if (!MakeNpcSpearKeyFromEditorFormID(nullptr, entry.formId, &row.key))
			{
				++skipped;
				continue;
			}

			rows.push_back(row);
		}

		const std::string payload = SpearSkillShared::NpcSpearTrainingPayloadCodec::Write(rows);
		if (!WriteEmbeddedNpcSpearTrainingPayload(payload.c_str()))
		{
			LogWarning("failed to save NPC Spear training sidecar carrier entries=%u skipped=%u",
				static_cast<unsigned int>(rows.size()),
				skipped);
			return false;
		}

		g_npcTrainingStore.ClearDirty();
		return true;
	}

	static bool FlushPendingSidecarStoresBeforePluginSave()
	{
		const bool hasDirtyStores =
			g_weaponTypeStore.IsDirty() ||
			g_npcSkillStore.IsDirty() ||
			g_npcTrainingStore.IsDirty();
		if (!hasDirtyStores)
			return true;

		if (!GetActiveEditorFile())
		{
			LogWarning("Spear sidecar data is dirty but no active plugin is available during pre-save; embedded carrier flush deferred");
			return false;
		}

		bool ok = true;
		if (g_weaponTypeStore.IsDirty())
		{
			if (SaveWeaponTypeStore())
				LogMessage("flushed pending Spear weapon Type sidecar carrier before plugin save");
			else
				ok = false;
		}

		if (g_npcSkillStore.IsDirty())
		{
			EnsureNpcSpearStoreConfigured();
			if (g_npcSkillStore.SaveEmbeddedCarrier())
				LogMessage("flushed pending NPC Spear skill carrier before plugin save");
			else
				ok = false;
		}

		if (g_npcTrainingStore.IsDirty())
		{
			if (SaveNpcSpearTrainingStore())
				LogMessage("flushed pending NPC Spear training carrier before plugin save");
			else
				ok = false;
		}

		return ok;
	}

	static bool IsComboBox(HWND hwnd)
	{
		char className[32] = {};
		return hwnd && GetClassNameA(hwnd, className, sizeof(className)) && !_stricmp(className, "ComboBox");
	}

	static std::string NormalizeWeaponTypeLabel(const std::string& value)
	{
		std::string normalized;
		normalized.reserve(value.size());
		for (char ch : value)
		{
			const unsigned char uch = static_cast<unsigned char>(ch);
			if (std::isalnum(uch))
				normalized.push_back(static_cast<char>(std::tolower(uch)));
		}
		return normalized;
	}

	static bool NormalizedContains(const std::string& value, const char* token)
	{
		return value.find(token) != std::string::npos;
	}

	static bool LabelMatchesNativeWeaponType(const std::string& label, const char* nativeLabel)
	{
		const std::string normalized = NormalizeWeaponTypeLabel(label);
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBladeOneHand))
			return NormalizedContains(normalized, "blade") && (NormalizedContains(normalized, "one") || NormalizedContains(normalized, "1"));
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBladeTwoHand))
			return NormalizedContains(normalized, "blade") && (NormalizedContains(normalized, "two") || NormalizedContains(normalized, "2"));
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBluntOneHand))
			return NormalizedContains(normalized, "blunt") && (NormalizedContains(normalized, "one") || NormalizedContains(normalized, "1"));
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBluntTwoHand))
			return NormalizedContains(normalized, "blunt") && (NormalizedContains(normalized, "two") || NormalizedContains(normalized, "2"));
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeStaff))
			return NormalizedContains(normalized, "staff");
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBow))
			return NormalizedContains(normalized, "bow");
		return false;
	}

	static int FindComboStringExact(HWND combo, const char* text)
	{
		if (!combo || !text)
			return CB_ERR;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		for (int i = 0; i < count; ++i)
		{
			std::string value;
			if (!ReadComboString(combo, i, value))
				continue;
			if (!_stricmp(value.c_str(), text))
				return i;
		}
		return CB_ERR;
	}

	static int FindNativeWeaponTypeString(HWND combo, const char* nativeLabel)
	{
		const int exact = FindComboStringExact(combo, nativeLabel);
		if (exact != CB_ERR)
			return exact;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		for (int i = 0; i < count; ++i)
		{
			std::string value;
			if (ReadComboString(combo, i, value) && LabelMatchesNativeWeaponType(value, nativeLabel))
				return i;
		}
		return CB_ERR;
	}

	static bool ReadComboString(HWND combo, int index, std::string& value)
	{
		value.clear();
		if (!combo || index < 0)
			return false;

		const LONG_PTR style = GetWindowLongPtrA(combo, GWL_STYLE);
		if ((style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)) && !(style & CBS_HASSTRINGS))
			return false;

		const LRESULT length = SendMessageA(combo, CB_GETLBTEXTLEN, index, 0);
		if (length == CB_ERR || length < 0 || length > 4096)
			return false;

		std::string buffer(static_cast<size_t>(length) + 1, '\0');
		const LRESULT copied = SendMessageA(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(buffer.data()));
		if (copied == CB_ERR || copied < 0)
			return false;

		buffer.resize(static_cast<size_t>(copied));
		value.swap(buffer);
		return true;
	}

	static void* DynamicCastTesAiFormToNpc(void* aiForm)
	{
		static const RuntimeDynamicCastFn dynamicCast =
			reinterpret_cast<RuntimeDynamicCastFn>(kRuntimeDynamicCast);
		if (!aiForm || !dynamicCast)
			return nullptr;

		void* npc = dynamicCast(aiForm,
			0,
			reinterpret_cast<void*>(kRttiTesAiFormTypeDescriptor),
			reinterpret_cast<void*>(kRttiTesNpcTypeDescriptor),
			0);
		return TryReadEditorFormType(npc) == kFormTypeNpc ? npc : nullptr;
	}

	static void* ResolveNpcFormFromAiDialog(HWND dialog)
	{
		static const WinDialogWndGetExtraDataByTypeFn getExtraDataByType =
			reinterpret_cast<WinDialogWndGetExtraDataByTypeFn>(kWinDialogWndGetExtraDataByType);
		void* extraData = dialog && getExtraDataByType ?
			getExtraDataByType(dialog, kDialogExtraWorkingDataType) : nullptr;
		if (!extraData)
			return nullptr;

		const void* ownerAddress = reinterpret_cast<const UInt8*>(extraData) + kDialogExtraWorkingDataOwnerOffset;
		if (IsBadReadPtr(ownerAddress, sizeof(void*)))
			return nullptr;

		void* owner = *reinterpret_cast<void* const*>(ownerAddress);
		if (void* npc = DynamicCastTesAiFormToNpc(owner))
			return npc;

		if (!g_loggedNpcTrainingBadDialogOwner)
		{
			LogWarning("NPC Spear training dialog owner did not RTTI-cast from TESAIForm to TESNPC; refusing raw owner pointer=%p",
				owner);
			g_loggedNpcTrainingBadDialogOwner = true;
		}
		return nullptr;
	}

	static void* ResolveNpcFormFromAiDialogOrForm(void* aiForm, HWND dialog)
	{
		(void)aiForm;
		return ResolveNpcFormFromAiDialog(dialog);
	}

	static bool IsSidecarTrainingLabel(const char* text)
	{
		return text && !_stricmp(text, kSpearEditorLabel);
	}

	static void AddSidecarTrainingComboItem(HWND combo)
	{
		if (!combo || FindComboStringExact(combo, kSpearEditorLabel) != CB_ERR)
			return;

		const int index = static_cast<int>(SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kSpearEditorLabel)));
		if (index != CB_ERR && index != CB_ERRSPACE)
			SendMessageA(combo, CB_SETITEMDATA, index, kNativeBladeActorValue);
	}

	static void MakeTrainingComboScrollable(HWND combo)
	{
		if (!combo)
			return;
		if (GetPropA(combo, kTrainingComboScrollableProp))
			return;
		if (SendMessageA(combo, CB_GETDROPPEDSTATE, 0, 0))
			return;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		const int itemHeight = static_cast<int>(SendMessageA(combo, CB_GETITEMHEIGHT, 0, 0));
		const int selectionHeight = static_cast<int>(SendMessageA(combo, CB_GETITEMHEIGHT, static_cast<WPARAM>(-1), 0));
		if (count <= 0 || itemHeight <= 0 || selectionHeight <= 0)
			return;

		SendMessageA(combo, CB_SETMINVISIBLE, static_cast<WPARAM>(kTrainingComboMinimumVisibleRows), 0);

		RECT rect = {};
		if (!GetWindowRect(combo, &rect))
			return;

		const int width = rect.right - rect.left;
		const int visibleRows = count > kTrainingComboMinimumVisibleRows ? kTrainingComboMinimumVisibleRows : count;
		const int height = selectionHeight + (itemHeight * visibleRows) + 10;
		const LONG_PTR style = GetWindowLongPtrA(combo, GWL_STYLE);
		SetWindowLongPtrA(combo, GWL_STYLE, style | WS_VSCROLL | CBS_DISABLENOSCROLL);
		SetWindowPos(combo, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		SetPropA(combo, kTrainingComboScrollableProp, reinterpret_cast<HANDLE>(1));
	}

	static WNDPROC GetOriginalTrainingComboProc(HWND combo)
	{
		return reinterpret_cast<WNDPROC>(GetPropA(combo, kTrainingComboOriginalProcProp));
	}

	static LRESULT CALLBACK TrainingComboProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC original = GetOriginalTrainingComboProc(hwnd);
		if (!original)
			return DefWindowProcA(hwnd, msg, wParam, lParam);

		if (msg == WM_NCDESTROY)
		{
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
			RemovePropA(hwnd, kTrainingComboPatchedProp);
			RemovePropA(hwnd, kTrainingComboOriginalProcProp);
			RemovePropA(hwnd, kTrainingComboScrollableProp);
			RemovePropA(hwnd, kTrainingComboApplyingSelectionProp);
			RemovePropA(hwnd, kTrainingComboBoundNpcFormIdProp);
			return CallWindowProcA(original, hwnd, msg, wParam, lParam);
		}

		if (msg == WM_MBUTTONDOWN)
		{
			SetFocus(hwnd);
			SendMessageA(hwnd, CB_SHOWDROPDOWN, TRUE, 0);
			return 0;
		}

		if (msg == WM_MOUSEWHEEL && !SendMessageA(hwnd, CB_GETDROPPEDSTATE, 0, 0))
		{
			SetFocus(hwnd);
			SendMessageA(hwnd, CB_SHOWDROPDOWN, TRUE, 0);
		}

		return CallWindowProcA(original, hwnd, msg, wParam, lParam);
	}

	static void PatchTrainingCombo(HWND combo)
	{
		if (!combo)
			return;

		AddSidecarTrainingComboItem(combo);
		MakeTrainingComboScrollable(combo);

		if (GetPropA(combo, kTrainingComboPatchedProp))
			return;

		WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(combo, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&TrainingComboProc)));
		if (!original)
			return;

		SetPropA(combo, kTrainingComboOriginalProcProp, reinterpret_cast<HANDLE>(original));
		SetPropA(combo, kTrainingComboPatchedProp, reinterpret_cast<HANDLE>(1));
	}

	static bool TrainingComboSelectionIsSpear(HWND combo)
	{
		const int selection = combo ? static_cast<int>(SendMessageA(combo, CB_GETCURSEL, 0, 0)) : CB_ERR;
		if (selection == CB_ERR)
			return false;

		std::string value;
		return ReadComboString(combo, selection, value) && IsSidecarTrainingLabel(value.c_str());
	}

	static void BindTrainingComboToNpc(HWND combo, void* npc)
	{
		if (!combo || TryReadEditorFormType(npc) != kFormTypeNpc)
			return;

		const UInt32 formId = ReadEditorFormId(npc);
		if (formId)
			SetPropA(combo, kTrainingComboBoundNpcFormIdProp, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(formId)));
	}

	static UInt32 GetBoundTrainingComboNpcFormId(HWND combo)
	{
		const UInt32 formId = static_cast<UInt32>(reinterpret_cast<UINT_PTR>(GetPropA(combo, kTrainingComboBoundNpcFormIdProp)));
		if (!formId)
			return 0;

		void* form = LookupEditorFormById(formId);
		return TryReadEditorFormType(form) == kFormTypeNpc ? formId : 0;
	}

	static bool PersistNpcTrainingSelectionForFormId(UInt32 formId, bool trainsSpear, const char* source)
	{
		if (!formId)
			return false;

		LoadNpcSpearTrainingStoreOnce();
		const bool changed = trainsSpear ?
			g_npcTrainingStore.Set(formId) :
			g_npcTrainingStore.Remove(formId);

		if (!changed && !g_npcTrainingStore.IsDirty())
			return true;

		if (!GetActiveEditorFile())
		{
			if (!g_loggedNpcTrainingNoActivePlugin)
			{
				LogWarning("NPC Spear training sidecar changed for form=%08X but no active plugin can receive %s; selection is affixed for this editor session only",
					formId,
					kNpcTrainingCarrierEditorId);
				g_loggedNpcTrainingNoActivePlugin = true;
			}
			return false;
		}

		if (!SaveNpcSpearTrainingStore())
		{
			LogWarning("failed to save NPC Spear training sidecar for form=%08X source=%s",
				formId,
				source ? source : "unknown");
			return false;
		}

		LogMessage("persisted NPC Spear training sidecar form=%08X trainsSpear=%u source=%s",
			formId,
			trainsSpear ? 1 : 0,
			source ? source : "unknown");
		return true;
	}

	static bool PersistTrainingComboSelection(HWND combo, const char* source)
	{
		if (!combo || GetPropA(combo, kTrainingComboApplyingSelectionProp))
			return false;

		HWND dialog = GetParent(combo);
		UInt32 formId = GetBoundTrainingComboNpcFormId(combo);
		if (!formId)
		{
			if (void* npc = ResolveNpcFormFromAiDialog(dialog))
			{
				BindTrainingComboToNpc(combo, npc);
				formId = GetBoundTrainingComboNpcFormId(combo);
			}
		}
		if (!formId)
		{
			if (!g_loggedNpcTrainingNoForm)
			{
				LogWarning("cannot persist NPC Spear training sidecar; edited NPC form was not resolved for combo=%p", combo);
				g_loggedNpcTrainingNoForm = true;
			}
			return false;
		}

		const bool trainingEnabled = !dialog || IsDlgButtonChecked(dialog, kAiTrainingCheckBox) == BST_CHECKED;
		const bool trainsSpear = trainingEnabled && TrainingComboSelectionIsSpear(combo);
		return PersistNpcTrainingSelectionForFormId(formId, trainsSpear, source);
	}

	static bool TrainingComboBoundNpcTrainsSpear(HWND combo)
	{
		const UInt32 formId = GetBoundTrainingComboNpcFormId(combo);
		if (!formId)
			return false;

		LoadNpcSpearTrainingStoreOnce();
		return g_npcTrainingStore.Has(formId);
	}

	static void RestoreTrainingComboSpearIfAffixed(HWND combo, const char* source)
	{
		if (!combo || GetPropA(combo, kTrainingComboApplyingSelectionProp))
			return;
		if (IsWindowEnabled(GetDlgItem(GetParent(combo), kAiTrainingCheckBox)) &&
			IsDlgButtonChecked(GetParent(combo), kAiTrainingCheckBox) != BST_CHECKED)
			return;
		if (!TrainingComboBoundNpcTrainsSpear(combo) || TrainingComboSelectionIsSpear(combo))
			return;

		SelectTrainingComboSpear(combo);
		LogMessage("restored NPC AI Spear training combo selection after native normalization source=%s",
			source ? source : "unknown");
	}

	static WNDPROC GetOriginalTrainingParentProc(HWND parent)
	{
		return reinterpret_cast<WNDPROC>(GetPropA(parent, kTrainingParentOriginalProcProp));
	}

	static LRESULT CALLBACK TrainingParentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC original = GetOriginalTrainingParentProc(hwnd);
		if (!original)
			return DefWindowProcA(hwnd, msg, wParam, lParam);

		if (msg == WM_NCDESTROY)
		{
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
			RemovePropA(hwnd, kTrainingParentPatchedProp);
			RemovePropA(hwnd, kTrainingParentOriginalProcProp);
			RemovePropA(hwnd, kTrainingParentComboProp);
			return CallWindowProcA(original, hwnd, msg, wParam, lParam);
		}

		HWND restoreCombo = nullptr;
		if (msg == WM_COMMAND)
		{
			const UINT controlId = LOWORD(wParam);
			const UINT notification = HIWORD(wParam);
			if (controlId == kAiTrainingSkillCombo && lParam)
			{
				HWND combo = reinterpret_cast<HWND>(lParam);
				if (GetPropA(combo, kTrainingComboPatchedProp) &&
					(notification == CBN_SELCHANGE || notification == CBN_SELENDOK || notification == CBN_CLOSEUP))
				{
					PersistTrainingComboSelection(combo, "training combo selection");
					if (TrainingComboBoundNpcTrainsSpear(combo))
						restoreCombo = combo;
				}
			}
			else if (controlId == kAiTrainingCheckBox)
			{
				HWND combo = reinterpret_cast<HWND>(GetPropA(hwnd, kTrainingParentComboProp));
				if (!combo)
					combo = GetDlgItem(hwnd, kAiTrainingSkillCombo);
				if (combo && GetPropA(combo, kTrainingComboPatchedProp))
				{
					PersistTrainingComboSelection(combo, "training checkbox toggle");
					if (TrainingComboBoundNpcTrainsSpear(combo))
						restoreCombo = combo;
				}
			}
		}

		const LRESULT result = CallWindowProcA(original, hwnd, msg, wParam, lParam);
		if (restoreCombo)
			RestoreTrainingComboSpearIfAffixed(restoreCombo, "parent dialog notification");

		return result;
	}

	static void PatchTrainingParentDialog(HWND dialog, HWND combo)
	{
		if (!dialog || !combo)
			return;

		SetPropA(dialog, kTrainingParentComboProp, combo);
		if (GetPropA(dialog, kTrainingParentPatchedProp))
			return;

		WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(dialog, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&TrainingParentProc)));
		if (!original)
		{
			RemovePropA(dialog, kTrainingParentComboProp);
			return;
		}

		SetPropA(dialog, kTrainingParentOriginalProcProp, reinterpret_cast<HANDLE>(original));
		SetPropA(dialog, kTrainingParentPatchedProp, reinterpret_cast<HANDLE>(1));
	}

	static void SelectTrainingComboSpear(HWND combo)
	{
		if (!combo)
			return;

		PatchTrainingCombo(combo);
		const int index = FindComboStringExact(combo, kSpearEditorLabel);
		if (index != CB_ERR)
		{
			SetPropA(combo, kTrainingComboApplyingSelectionProp, reinterpret_cast<HANDLE>(1));
			SendMessageA(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
			RemovePropA(combo, kTrainingComboApplyingSelectionProp);
		}
	}

	static void ApplyNpcAiTrainingSidecarSelection(void* aiForm, HWND dialog)
	{
		HWND combo = GetDlgItem(dialog, kAiTrainingSkillCombo);
		if (!combo)
			return;

		PatchTrainingCombo(combo);
		PatchTrainingParentDialog(dialog, combo);

		LoadNpcSpearTrainingStoreOnce();
		void* npc = ResolveNpcFormFromAiDialogOrForm(aiForm, dialog);
		if (!npc)
			return;

		BindTrainingComboToNpc(combo, npc);
		if (!g_npcTrainingStore.Has(ReadEditorFormId(npc)))
			return;

		HWND trainingCheckBox = GetDlgItem(dialog, kAiTrainingCheckBox);
		const BOOL canEditTraining = !trainingCheckBox || IsWindowEnabled(trainingCheckBox);
		CheckDlgButton(dialog, kAiTrainingCheckBox, BST_CHECKED);
		if (canEditTraining)
		{
			EnableWindow(combo, TRUE);
			if (HWND level = GetDlgItem(dialog, kAiTrainingLevelEdit))
				EnableWindow(level, TRUE);
		}
		SelectTrainingComboSpear(combo);
		LogMessage("applied NPC AI Spear training sidecar selection form=%08X", ReadEditorFormId(npc));
	}

	static void SaveNpcAiTrainingSidecarSelection(void* aiForm, HWND dialog)
	{
		void* npc = ResolveNpcFormFromAiDialogOrForm(aiForm, dialog);
		HWND combo = GetDlgItem(dialog, kAiTrainingSkillCombo);
		if (!combo)
			return;

		if (npc)
			BindTrainingComboToNpc(combo, npc);

		const UInt32 formId = npc ? ReadEditorFormId(npc) : GetBoundTrainingComboNpcFormId(combo);
		if (!formId)
			return;

		const bool trainingEnabled = IsDlgButtonChecked(dialog, kAiTrainingCheckBox) == BST_CHECKED;
		const bool trainsSpear = trainingEnabled &&
			(TrainingComboSelectionIsSpear(combo) || TrainingComboBoundNpcTrainsSpear(combo));
		PersistNpcTrainingSelectionForFormId(formId, trainsSpear, "dialog save");
	}

	static int __fastcall AiFormInitDialogControlsHook(void* aiForm, void*, HWND dialog)
	{
		const int result = g_aiFormInitDialogControlsOriginal ?
			g_aiFormInitDialogControlsOriginal(aiForm, dialog) : 0;
		ApplyNpcAiTrainingSidecarSelection(aiForm, dialog);
		return result;
	}

	static int __fastcall AiFormSaveDialogControlsHook(void* aiForm, void*, HWND dialog)
	{
		SaveNpcAiTrainingSidecarSelection(aiForm, dialog);
		const int result = g_aiFormSaveDialogControlsOriginal ?
			g_aiFormSaveDialogControlsOriginal(aiForm, dialog) : 0;
		RestoreTrainingComboSpearIfAffixed(GetDlgItem(dialog, kAiTrainingSkillCombo), "dialog save");
		return result;
	}

	static void __declspec(naked) SavePluginPreWriteHook()
	{
		__asm
		{
			pushfd
			pushad
			call FlushPendingSidecarStoresBeforePluginSave
			popad
			popfd

			mov eax, dword ptr [g_savePluginPreWriteChainTarget]
			test eax, eax
			jz noChain
			jmp eax

		noChain:
			call kSavePluginPreWriteOriginalCall
			jmp kSavePluginPreWriteReturn
		}
	}

	static bool IsSidecarWeaponTypeLabel(const char* value)
	{
		return value && !_stricmp(value, kSpearEditorLabel);
	}

	static SpearSkillShared::WeaponSkillKind KindForSidecarWeaponTypeLabel(const char* value)
	{
		if (value && !_stricmp(value, kSpearEditorLabel))
			return SpearSkillShared::kWeaponSkill_Spear;
		return SpearSkillShared::kWeaponSkill_None;
	}

	static const char* LabelForSidecarWeaponTypeKind(SpearSkillShared::WeaponSkillKind kind)
	{
		return kind == SpearSkillShared::kWeaponSkill_Spear ? kSpearEditorLabel : "";
	}

	static bool IsNativeWeaponTypeColumnText(const char* text)
	{
		if (!text || !text[0])
			return false;

		const std::string normalized = NormalizeWeaponTypeLabel(text);
		return normalized == "bladeonehand" ||
			normalized == "blade1hand" ||
			normalized == "bladeonehanded" ||
			normalized == "blade1handed" ||
			normalized == "bladetwohand" ||
			normalized == "blade2hand" ||
			normalized == "bladetwohanded" ||
			normalized == "blade2handed" ||
			normalized == "bluntonehand" ||
			normalized == "blunt1hand" ||
			normalized == "bluntonehanded" ||
			normalized == "blunt1handed" ||
			normalized == "blunttwohand" ||
			normalized == "blunt2hand" ||
			normalized == "blunttwohanded" ||
			normalized == "blunt2handed" ||
			normalized == "staff" ||
			normalized == "bow";
	}

	static void* ResolveObjectWindowWeaponForm(void* treeEntry, NMLVDISPINFOA* data)
	{
		void* form = data ? reinterpret_cast<void*>(data->item.lParam) : nullptr;
		if (TryReadEditorFormType(form) == kFormTypeWeapon)
			return form;

		const UInt32 lParamAsFormId = data ? static_cast<UInt32>(data->item.lParam) : 0;
		if (lParamAsFormId)
		{
			form = LookupEditorFormById(lParamAsFormId);
			if (ReadEditorFormType(form) == kFormTypeWeapon)
				return form;
		}

		if (TryReadEditorFormType(treeEntry) == kFormTypeWeapon)
			return treeEntry;

		return nullptr;
	}

	static void ApplyObjectWindowWeaponTypeColumnText(void* treeEntry, NMLVDISPINFOA* data)
	{
		if (!data || !(data->item.mask & LVIF_TEXT) || !data->item.pszText || data->item.cchTextMax <= 0)
			return;
		if (!IsNativeWeaponTypeColumnText(data->item.pszText))
			return;

		void* form = ResolveObjectWindowWeaponForm(treeEntry, data);
		if (ReadEditorFormType(form) != kFormTypeWeapon)
			return;

		LoadWeaponTypeStoreOnce();

		SpearSkillShared::WeaponSkillKind kind = SpearSkillShared::kWeaponSkill_None;
		if (!g_weaponTypeStore.TryGet(ReadEditorFormId(form), &kind))
			return;

		const char* label = LabelForSidecarWeaponTypeKind(kind);
		if (!label || !label[0])
			return;

		_snprintf_s(data->item.pszText, data->item.cchTextMax, _TRUNCATE, "%s", label);
	}

	static void __fastcall ObjectWindowListViewGetDispInfoHook(void* treeEntry, void*, NMLVDISPINFOA* data)
	{
		if (g_objectWindowListViewGetDispInfoOriginal)
			g_objectWindowListViewGetDispInfoOriginal(treeEntry, data);
		ApplyObjectWindowWeaponTypeColumnText(treeEntry, data);
	}

	static const char* NativeFallbackLabelForSidecarWeaponTypeKind(SpearSkillShared::WeaponSkillKind kind)
	{
		return kind == SpearSkillShared::kWeaponSkill_Spear ? kNativeWeaponTypeBladeTwoHand : kNativeWeaponTypeBladeTwoHand;
	}

	static int GetActualWeaponTypeComboSelection(HWND combo)
	{
		WNDPROC original = GetOriginalWeaponTypeComboProc(combo);
		const LRESULT selection = original ?
			CallWindowProcA(original, combo, CB_GETCURSEL, 0, 0) :
			SendMessageA(combo, CB_GETCURSEL, 0, 0);
		return selection == CB_ERR ? CB_ERR : static_cast<int>(selection);
	}

	static LRESULT SetActualWeaponTypeComboSelection(HWND combo, int selection)
	{
		WNDPROC original = GetOriginalWeaponTypeComboProc(combo);
		return original ?
			CallWindowProcA(original, combo, CB_SETCURSEL, static_cast<WPARAM>(selection), 0) :
			SendMessageA(combo, CB_SETCURSEL, static_cast<WPARAM>(selection), 0);
	}

	static void BindWeaponTypeComboToForm(HWND combo, void* form)
	{
		if (!combo || ReadEditorFormType(form) != kFormTypeWeapon)
			return;

		SetPropA(combo, kWeaponTypeComboBoundFormPtrProp, reinterpret_cast<HANDLE>(form));

		const UInt32 formId = ReadEditorFormId(form);
		if (!formId)
			return;

		SetPropA(combo, kWeaponTypeComboBoundFormIdProp, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(formId)));
	}

	static void* GetBoundWeaponTypeComboForm(HWND combo)
	{
		void* form = reinterpret_cast<void*>(GetPropA(combo, kWeaponTypeComboBoundFormPtrProp));
		return ReadEditorFormType(form) == kFormTypeWeapon ? form : nullptr;
	}

	static UInt32 GetBoundWeaponTypeComboFormId(HWND combo)
	{
		const UInt32 formId = static_cast<UInt32>(reinterpret_cast<UINT_PTR>(GetPropA(combo, kWeaponTypeComboBoundFormIdProp)));
		if (formId)
		{
			void* form = LookupEditorFormById(formId);
			if (ReadEditorFormType(form) == kFormTypeWeapon)
				return formId;
		}

		void* form = GetBoundWeaponTypeComboForm(combo);
		const UInt32 boundFormId = ReadEditorFormId(form);
		if (boundFormId)
		{
			SetPropA(combo, kWeaponTypeComboBoundFormIdProp, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(boundFormId)));
			return boundFormId;
		}

		return 0;
	}

	static int __fastcall HookWeaponDialogInit(void* weapon, void*, HWND dialog)
	{
		const int result = g_weaponDialogInitOriginal ? g_weaponDialogInitOriginal(weapon, dialog) : 0;

		if (ReadEditorFormType(weapon) == kFormTypeWeapon)
		{
			HWND combo = GetDlgItem(dialog, kWeaponDialogTypeComboId);
			if (combo)
			{
				BindWeaponTypeComboToForm(combo, weapon);
				PatchWeaponTypeCombo(combo);
			}
		}

		return result;
	}

	static bool WindowTextContainsWeapon(HWND hwnd)
	{
		char title[256] = {};
		if (!hwnd || !GetWindowTextA(hwnd, title, sizeof(title)))
			return false;

		std::string normalized = NormalizeWeaponTypeLabel(title);
		return NormalizedContains(normalized, "weapon");
	}

	static HWND FindParentWeaponDialog(HWND combo)
	{
		for (HWND current = GetParent(combo); current; current = GetParent(current))
		{
			if (WindowTextContainsWeapon(current))
				return current;
		}

		return combo ? GetAncestor(combo, GA_ROOT) : nullptr;
	}

	static void* LookupWeaponFormByEditorId(const char* editorId)
	{
		if (!editorId || !editorId[0])
			return nullptr;

		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(editorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeWeapon ? form : nullptr;
	}

	static void* LookupUniqueWeaponFormByEditorId(const char* editorId)
	{
		if (!editorId || !editorId[0])
			return nullptr;

		void* dataHandler = GetEditorDataHandler();
		void* objectList = dataHandler ? *reinterpret_cast<void**>(dataHandler) : nullptr;
		if (!objectList)
			return LookupWeaponFormByEditorId(editorId);

		void* match = nullptr;
		void* current = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(objectList) + kBoundObjectListFirstOffset);
		const UInt32 objectCount = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(objectList) + kBoundObjectListCountOffset);
		const UInt32 scanLimit = objectCount && objectCount < 0x200000 ? objectCount + 16 : 0x200000;
		for (UInt32 scanned = 0; current && scanned < scanLimit; ++scanned)
		{
			if (ReadEditorFormType(current) == kFormTypeWeapon)
			{
				const char* candidateEditorId = ReadEditorFormEditorId(current);
				if (candidateEditorId && !_stricmp(candidateEditorId, editorId))
				{
					if (match && ReadEditorFormId(match) != ReadEditorFormId(current))
					{
						if (!g_loggedLegacySelfWeaponTypeAmbiguous)
						{
							LogWarning("ambiguous legacy $SELF Spear weapon Type sidecar editor ID %s; row was not resolved", editorId);
							g_loggedLegacySelfWeaponTypeAmbiguous = true;
						}
						return nullptr;
					}

					match = current;
				}
			}

			current = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(current) + kTesObjectNextOffset);
		}

		return match;
	}

	static void* LookupNpcFormByEditorId(const char* editorId)
	{
		if (!editorId || !editorId[0])
			return nullptr;

		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(editorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeNpc ? form : nullptr;
	}

	static void* LookupUniqueNpcFormByEditorId(const char* editorId)
	{
		if (!editorId || !editorId[0])
			return nullptr;

		void* dataHandler = GetEditorDataHandler();
		void* objectList = dataHandler ? *reinterpret_cast<void**>(dataHandler) : nullptr;
		if (!objectList)
			return LookupNpcFormByEditorId(editorId);

		void* match = nullptr;
		void* current = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(objectList) + kBoundObjectListFirstOffset);
		const UInt32 objectCount = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(objectList) + kBoundObjectListCountOffset);
		const UInt32 scanLimit = objectCount && objectCount < 0x200000 ? objectCount + 16 : 0x200000;
		for (UInt32 scanned = 0; current && scanned < scanLimit; ++scanned)
		{
			if (ReadEditorFormType(current) == kFormTypeNpc)
			{
				const char* candidateEditorId = ReadEditorFormEditorId(current);
				if (candidateEditorId && !_stricmp(candidateEditorId, editorId))
				{
					if (match && ReadEditorFormId(match) != ReadEditorFormId(current))
					{
						if (!g_loggedLegacySelfNpcSkillAmbiguous)
						{
							LogWarning("ambiguous legacy $SELF NPC Spear sidecar editor ID %s; row was not resolved", editorId);
							g_loggedLegacySelfNpcSkillAmbiguous = true;
						}
						return nullptr;
					}

					match = current;
				}
			}

			current = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(current) + kTesObjectNextOffset);
		}

		return match;
	}

	static UInt32 ReadNpcFormId(const void* npc)
	{
		return TryReadEditorFormType(npc) == kFormTypeNpc ? ReadEditorFormId(npc) : 0;
	}

	static bool ListViewItemTextEquals(HWND list, int index, int subItem, const char* text)
	{
		char buffer[128] = {};
		LVITEMA item = {};
		item.mask = LVIF_TEXT;
		item.iItem = index;
		item.iSubItem = subItem;
		item.pszText = buffer;
		item.cchTextMax = sizeof(buffer);
		SendMessageA(list, LVM_GETITEMTEXTA, index, reinterpret_cast<LPARAM>(&item));
		return _stricmp(buffer, text) == 0;
	}

	static void SetListViewItemText(HWND list, int row, int subItem, const char* text)
	{
		LVITEMA item = {};
		item.mask = LVIF_TEXT;
		item.iItem = row;
		item.iSubItem = subItem;
		item.pszText = const_cast<char*>(text);
		SendMessageA(list, LVM_SETITEMTEXTA, row, reinterpret_cast<LPARAM>(&item));
	}

	static bool GetListViewItemText(HWND list, int row, int subItem, char* buffer, UInt32 bufferSize)
	{
		if (!list || !buffer || !bufferSize)
			return false;

		buffer[0] = 0;
		LVITEMA item = {};
		item.mask = LVIF_TEXT;
		item.iItem = row;
		item.iSubItem = subItem;
		item.pszText = buffer;
		item.cchTextMax = bufferSize;
		SendMessageA(list, LVM_GETITEMTEXTA, row, reinterpret_cast<LPARAM>(&item));
		return buffer[0] != 0;
	}

	static void* CsFormHeapAllocate(UInt32 size)
	{
		return reinterpret_cast<CsFormHeapAllocateFn>(kCsFormHeapAllocate)(size);
	}

	static void CsFormHeapFree(void* ptr)
	{
		if (ptr)
			reinterpret_cast<CsFormHeapFreeFn>(kCsFormHeapFree)(ptr);
	}

	static LPARAM GetListViewItemData(HWND list, int row)
	{
		LVITEMA item = {};
		item.mask = LVIF_PARAM;
		item.iItem = row;
		SendMessageA(list, LVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item));
		return item.lParam;
	}

	static bool ParseNpcStatsSpearValue(const char* text, UInt8* value)
	{
		if (!text || !value)
			return false;

		while (*text == ' ' || *text == '\t')
			++text;
		if (!*text)
			return false;

		char* end = nullptr;
		const unsigned long parsed = std::strtoul(text, &end, 10);
		if (end == text)
			return false;

		*value = static_cast<UInt8>(SpearSkillShared::NpcSpearStore::ClampLevel(parsed > 100 ? 100 : parsed));
		return true;
	}

	static bool __stdcall IsNpcStatsSpearRowData(const NpcStatsSkillRowData* rowData)
	{
		return rowData == &g_npcStatsSpearListData ||
			(rowData &&
				rowData->value == &g_npcStatsSpearValue &&
				rowData->skillIndex == kNpcStatsSpearSortSkillIndex &&
				rowData->sidecarIndex == 0 &&
				rowData->marker == kNpcStatsSpearRowMarker);
	}

	static bool __stdcall IsNpcStatsSpearEditRow(NMLVDISPINFOA* data)
	{
		if (!data)
			return false;

		if (IsNpcStatsSpearRowData(reinterpret_cast<NpcStatsSkillRowData*>(data->item.lParam)))
			return true;

		return data->hdr.hwndFrom && data->item.iItem >= 0 &&
			IsNpcStatsSpearListRow(data->hdr.hwndFrom, data->item.iItem);
	}

	static NpcStatsSkillRowData* InitializeNpcStatsSpearRowData(NpcStatsSkillRowData* rowData)
	{
		if (!rowData)
			return nullptr;

		rowData->value = &g_npcStatsSpearValue;
		rowData->skillIndex = kNpcStatsSpearSortSkillIndex;
		rowData->sidecarIndex = 0;
		rowData->marker = kNpcStatsSpearRowMarker;
		rowData->padding = 0;
		return rowData;
	}

	static NpcStatsSkillRowData* AllocateNpcStatsSpearRowData()
	{
		NpcStatsSkillRowData* rowData = reinterpret_cast<NpcStatsSkillRowData*>(
			CsFormHeapAllocate(sizeof(NpcStatsSkillRowData)));
		if (!rowData)
		{
			LogError("failed to allocate NPC stats Spear row data");
			return nullptr;
		}

		return InitializeNpcStatsSpearRowData(rowData);
	}

	static bool SetNpcStatsSpearRowData(HWND list, int row, NpcStatsSkillRowData* rowData)
	{
		if (!list || row < 0 || !InitializeNpcStatsSpearRowData(rowData))
			return false;

		LVITEMA item = {};
		item.mask = LVIF_PARAM;
		item.iItem = row;
		item.iSubItem = 0;
		item.lParam = reinterpret_cast<LPARAM>(rowData);
		return SendMessageA(list, LVM_SETITEMA, 0, reinterpret_cast<LPARAM>(&item)) != FALSE;
	}

	static bool EnsureNpcStatsSpearRowData(HWND list, int row)
	{
		NpcStatsSkillRowData* rowData = reinterpret_cast<NpcStatsSkillRowData*>(
			GetListViewItemData(list, row));
		if (IsNpcStatsSpearRowData(rowData) && rowData != &g_npcStatsSpearListData)
			return SetNpcStatsSpearRowData(list, row, rowData);

		rowData = AllocateNpcStatsSpearRowData();
		if (!rowData)
			return false;

		if (!SetNpcStatsSpearRowData(list, row, rowData))
		{
			CsFormHeapFree(rowData);
			return false;
		}

		return true;
	}

	static bool IsNpcStatsSpearListRow(HWND list, int row)
	{
		return IsNpcStatsSpearRowData(reinterpret_cast<NpcStatsSkillRowData*>(GetListViewItemData(list, row))) ||
			ListViewItemTextEquals(list, row, 1, kSpearEditorLabel);
	}

	static void FormatNpcStatsSpearValue(char* buffer, UInt32 bufferSize)
	{
		if (!buffer || !bufferSize)
			return;

		_snprintf_s(buffer, bufferSize, _TRUNCATE, "%u",
			static_cast<unsigned int>(g_npcStatsSpearValue));
	}

	static bool ConfigureNpcStatsSpearRow(HWND list, int row)
	{
		if (!EnsureNpcStatsSpearRowData(list, row))
			return false;

		char valueText[16] = {};
		FormatNpcStatsSpearValue(valueText, sizeof(valueText));
		SetListViewItemText(list, row, 0, valueText);
		SetListViewItemText(list, row, 1, kSpearEditorLabel);
		return true;
	}

	static void EnsureNpcStatsSkillsListScrollbar(HWND list, int itemCount)
	{
		if (!list)
			return;

		const LONG_PTR style = GetWindowLongPtrA(list, GWL_STYLE);
		const LONG_PTR newStyle = (style | WS_VSCROLL) & ~static_cast<LONG_PTR>(LVS_NOSCROLL);
		if (newStyle != style)
			SetWindowLongPtrA(list, GWL_STYLE, newStyle);

		ShowScrollBar(list, SB_VERT, TRUE);
		SetWindowPos(list, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		InvalidateRect(list, nullptr, TRUE);

		if (!g_loggedNpcStatsScrollbar)
		{
			LogMessage("NPC stats skills list scrollbar enabled hwnd=%08X items=%u style=%08X",
				reinterpret_cast<UInt32>(list),
				static_cast<unsigned int>(itemCount),
				static_cast<UInt32>(newStyle));
			g_loggedNpcStatsScrollbar = true;
		}
	}

	static void PrepareNpcStatsSpearSidecar(void* npc)
	{
		LoadNpcSpearStoreOnce();

		g_activeNpcStatsFormId = ReadNpcFormId(npc);
		g_npcStatsSpearValue = kNpcStatsSpearDefaultSkillValue;
		g_npcStatsSpearListData.value = &g_npcStatsSpearValue;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (g_npcSkillStore.TryGet(g_activeNpcStatsFormId, &entry))
		{
			g_npcStatsSpearValue = static_cast<UInt8>(SpearSkillShared::NpcSpearStore::ClampLevel(entry.level));
			g_npcStatsSpearListData.value = &g_npcStatsSpearValue;
		}
	}

	static bool TryReadNpcStatsSpearListValue(UInt8* value)
	{
		HWND list = g_activeNpcStatsSkillsList;
		if (!list && g_activeNpcStatsDialog)
			list = GetDlgItem(g_activeNpcStatsDialog, kNpcStatsSkillsList);
		if (!list || !value)
			return false;

		const int count = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0));
		for (int i = 0; i < count; ++i)
		{
			if (!IsNpcStatsSpearListRow(list, i))
				continue;

			char buffer[32] = {};
			if (GetListViewItemText(list, i, 0, buffer, sizeof(buffer)) &&
				ParseNpcStatsSpearValue(buffer, value))
			{
				return true;
			}

			LogWarning("could not parse NPC stats Spear value row=%d", i);
			return false;
		}

		return false;
	}

	static void SaveActiveNpcStatsSpearSidecarValue(const char* source)
	{
		if (!g_activeNpcStatsFormId)
			return;

		EnsureNpcSpearStoreConfigured();
		g_npcStatsSpearValue = static_cast<UInt8>(SpearSkillShared::NpcSpearStore::ClampLevel(
			g_npcStatsSpearListData.value ? *g_npcStatsSpearListData.value : g_npcStatsSpearValue));
		g_npcStatsSpearListData.value = &g_npcStatsSpearValue;

		if (g_npcSkillStore.SetLevel(g_activeNpcStatsFormId, g_npcStatsSpearValue))
		{
			LogMessage("saved NPC stats Spear form=%08X value=%u source=%s",
				g_activeNpcStatsFormId,
				static_cast<unsigned int>(g_npcStatsSpearValue),
				source ? source : "unknown");

			if (!GetActiveEditorFile())
			{
				if (!g_loggedNpcSkillNoActivePlugin)
				{
					LogWarning("NPC stats Spear sidecar values are affixed for this editor session only; activate a plugin to persist them to %s",
						kNpcSkillCarrierEditorId);
					g_loggedNpcSkillNoActivePlugin = true;
				}
				return;
			}

			if (!g_npcSkillStore.SaveEmbeddedCarrier())
			{
				LogWarning("NPC stats Spear form=%08X value=%u kept in memory; embedded save pending an active plugin",
					g_activeNpcStatsFormId,
					static_cast<unsigned int>(g_npcStatsSpearValue));
			}
		}
	}

	static void PersistActiveNpcStatsSpearSidecar()
	{
		UInt8 listValue = 0;
		if (TryReadNpcStatsSpearListValue(&listValue))
			g_npcStatsSpearValue = listValue;

		SaveActiveNpcStatsSpearSidecarValue("native-edit");
	}

	static bool __stdcall HandleNpcStatsSkillEditGate(void* npc, NMLVDISPINFOA* data, UInt32 autoCalc)
	{
		if (!data)
			return false;

		if (!IsNpcStatsSpearRowData(reinterpret_cast<NpcStatsSkillRowData*>(data->item.lParam)) &&
			(!data->hdr.hwndFrom || data->item.iItem < 0 || !IsNpcStatsSpearListRow(data->hdr.hwndFrom, data->item.iItem)))
		{
			return false;
		}

		UInt8 editedValue = 0;
		if (!ParseNpcStatsSpearValue(data->item.pszText, &editedValue))
		{
			LogWarning("could not parse NPC stats Spear edit");
			return false;
		}

		const UInt32 formId = ReadNpcFormId(npc);
		if (formId)
			g_activeNpcStatsFormId = formId;
		if (!g_activeNpcStatsFormId)
			return false;

		g_npcStatsSpearValue = editedValue;
		g_npcStatsSpearListData.value = &g_npcStatsSpearValue;

		char valueText[16] = {};
		FormatNpcStatsSpearValue(valueText, sizeof(valueText));
		if (data->item.pszText && data->item.cchTextMax > 0)
			_snprintf_s(data->item.pszText, data->item.cchTextMax, _TRUNCATE, "%s", valueText);
		if (data->hdr.hwndFrom && data->item.iItem >= 0)
			SetListViewItemText(data->hdr.hwndFrom, data->item.iItem, 0, valueText);

		SaveActiveNpcStatsSpearSidecarValue(autoCalc ? "autocalc-edit" : "pre-native-edit");
		return true;
	}

	static void FreeNpcStatsSpearRowDataForRow(HWND list, int row)
	{
		NpcStatsSkillRowData* rowData = reinterpret_cast<NpcStatsSkillRowData*>(GetListViewItemData(list, row));
		if (IsNpcStatsSpearRowData(rowData) && rowData != &g_npcStatsSpearListData)
			CsFormHeapFree(rowData);
	}

	static int FindNpcStatsSpearInsertIndex(HWND list)
	{
		if (!list)
			return 0;

		int insertIndex = 0;
		const int count = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0));
		for (int i = 0; i < count; ++i)
		{
			if (ListViewItemTextEquals(list, i, 1, "Long Blade") ||
				ListViewItemTextEquals(list, i, 1, "Short Blade") ||
				ListViewItemTextEquals(list, i, 1, "Axe") ||
				ListViewItemTextEquals(list, i, 1, "Medium Armor"))
			{
				insertIndex = i + 1;
				continue;
			}
			break;
		}

		return insertIndex;
	}

	static void EnsureNpcStatsSpearListRow(void* npc, HWND list)
	{
		if (!list)
			return;

		g_activeNpcStatsSkillsList = list;
		PrepareNpcStatsSpearSidecar(npc);

		for (int i = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0)) - 1; i >= 0; --i)
		{
			if (!IsNpcStatsSpearListRow(list, i))
				continue;

			FreeNpcStatsSpearRowDataForRow(list, i);
			SendMessageA(list, LVM_DELETEITEM, i, 0);
		}

		int count = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0));
		const int insertIndex = FindNpcStatsSpearInsertIndex(list);

		char valueText[16] = {};
		FormatNpcStatsSpearValue(valueText, sizeof(valueText));
		NpcStatsSkillRowData* rowData = AllocateNpcStatsSpearRowData();
		if (!rowData)
		{
			EnsureNpcStatsSkillsListScrollbar(list, count);
			return;
		}

		LVITEMA item = {};
		item.mask = LVIF_TEXT | LVIF_PARAM;
		item.iItem = insertIndex;
		item.iSubItem = 0;
		item.pszText = valueText;
		item.lParam = reinterpret_cast<LPARAM>(rowData);
		const int row = static_cast<int>(SendMessageA(list, LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&item)));
		if (row >= 0)
			ConfigureNpcStatsSpearRow(list, row);
		else
			CsFormHeapFree(rowData);

		count = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0));
		EnsureNpcStatsSkillsListScrollbar(list, count);
	}

	static void EnsureNpcStatsSpearRow(void* npc, HWND dialog)
	{
		if (!dialog)
			return;

		g_activeNpcStatsDialog = dialog;
		EnsureNpcStatsSpearListRow(npc, GetDlgItem(dialog, kNpcStatsSkillsList));
	}

	static int CallNpcStatsPopulateOriginal(void* npc, HWND dialog)
	{
		if (g_npcStatsPopulateOriginalIsFastcall)
			return reinterpret_cast<NpcStatsPopulateChainedFn>(g_npcStatsPopulateOriginalTarget)(npc, nullptr, dialog);

		return reinterpret_cast<NpcStatsPopulateFn>(g_npcStatsPopulateOriginalTarget)(npc, dialog);
	}

	static LRESULT CallNpcStatsInitSkillListOriginal(void* npc, HWND list)
	{
		if (g_npcStatsInitSkillListOriginalIsFastcall)
			return reinterpret_cast<NpcStatsInitSkillListChainedFn>(g_npcStatsInitSkillListOriginalTarget)(npc, nullptr, list);

		return reinterpret_cast<NpcStatsInitSkillListFn>(g_npcStatsInitSkillListOriginalTarget)(npc, list);
	}

	static int __fastcall NpcStatsPopulateHook(void* npc, void*, HWND dialog)
	{
		const int result = CallNpcStatsPopulateOriginal(npc, dialog);
		EnsureNpcStatsSpearRow(npc, dialog);
		return result;
	}

	static LRESULT __fastcall NpcStatsInitSkillListHook(void* npc, void*, HWND list)
	{
		const LRESULT result = CallNpcStatsInitSkillListOriginal(npc, list);
		EnsureNpcStatsSpearListRow(npc, list);
		return result;
	}

	static void __stdcall HandleNpcStatsSkillEditCommit(NpcStatsSkillRowData* rowData)
	{
		if (IsNpcStatsSpearRowData(rowData))
			PersistActiveNpcStatsSpearSidecar();
	}

	static void __declspec(naked) NpcStatsSkillEditAutoCalcGateHook()
	{
		__asm
		{
			test al, al
			jz notAutoCalc
			pushad
			push 1
			push ebp
			push edi
			call HandleNpcStatsSkillEditGate
			mov [esp+1Ch], eax
			popad
			test al, al
			jz autoCalcNotHandled
			mov ecx, [esp+20h]
			mov dword ptr [ecx], 1
			mov al, 1
			jmp kNpcStatsDialogSuccessReturn
		autoCalcNotHandled:
			cmp dword ptr [g_npcStatsSkillEditAutoCalcGateChainTarget], 0
			jne chainPrevious
			jmp kNpcStatsSkillEditAutoCalcRejectReturn
		notAutoCalc:
			pushad
			push 0
			push ebp
			push edi
			call HandleNpcStatsSkillEditGate
			mov [esp+1Ch], eax
			popad
			test al, al
			jz notAutoCalcNotHandled
			jmp kNpcStatsSkillEditAutoCalcContinue
		notAutoCalcNotHandled:
			cmp dword ptr [g_npcStatsSkillEditAutoCalcGateChainTarget], 0
			jne chainPrevious
			jmp kNpcStatsSkillEditAutoCalcContinue
		chainPrevious:
			jmp dword ptr [g_npcStatsSkillEditAutoCalcGateChainTarget]
		}
	}

	static void __declspec(naked) NpcStatsSkillEditCommitHook()
	{
		__asm
		{
			pushad
			push esi
			call IsNpcStatsSpearRowData
			mov [esp+1Ch], eax
			test al, al
			popad
			jnz handleSpear
			cmp dword ptr [g_npcStatsSkillEditCommitChainTarget], 0
			jne chainPrevious
			mov eax, [esp+2Ch]
			mov dword ptr [eax], 1
			jmp kNpcStatsSkillEditCommitReturn
		chainPrevious:
			jmp dword ptr [g_npcStatsSkillEditCommitChainTarget]
		handleSpear:
			pushad
			push esi
			call HandleNpcStatsSkillEditCommit
			popad
			mov eax, [esp+2Ch]
			mov dword ptr [eax], 1
			jmp kNpcStatsSkillEditCommitReturn
		}
	}

	static void* FindWeaponFormFromDialogTitle(HWND dialog)
	{
		char title[512] = {};
		if (!dialog || !GetWindowTextA(dialog, title, sizeof(title)))
			return nullptr;

		const char* open = std::strchr(title, '[');
		const char* close = open ? std::strchr(open + 1, ']') : nullptr;
		if (open && close && close > open + 1)
		{
			std::string editorId(open + 1, close);
			if (void* form = LookupWeaponFormByEditorId(editorId.c_str()))
				return form;
		}

		return LookupWeaponFormByEditorId(title);
	}

	struct FindWeaponFormInEditsContext
	{
		void* form;
	};

	static BOOL CALLBACK FindWeaponFormInEdits(HWND hwnd, LPARAM lParam)
	{
		FindWeaponFormInEditsContext* context = reinterpret_cast<FindWeaponFormInEditsContext*>(lParam);
		if (!context || context->form)
			return FALSE;

		char className[32] = {};
		if (!GetClassNameA(hwnd, className, sizeof(className)) || _stricmp(className, "Edit"))
			return TRUE;

		char text[256] = {};
		if (!GetWindowTextA(hwnd, text, sizeof(text)) || !text[0])
			return TRUE;

		context->form = LookupWeaponFormByEditorId(text);
		return context->form ? FALSE : TRUE;
	}

	static void* FindEditedWeaponFormForCombo(HWND combo)
	{
		HWND dialog = FindParentWeaponDialog(combo);
		if (!dialog)
			return nullptr;

		if (void* form = FindWeaponFormFromDialogTitle(dialog))
			return form;

		FindWeaponFormInEditsContext context = {};
		EnumChildWindows(dialog, FindWeaponFormInEdits, reinterpret_cast<LPARAM>(&context));
		return context.form;
	}

	static bool GetEditedWeaponFormIdForCombo(HWND combo, UInt32* formId)
	{
		if (formId)
			*formId = 0;

		if (const UInt32 boundFormId = GetBoundWeaponTypeComboFormId(combo))
		{
			if (formId)
				*formId = boundFormId;
			return true;
		}

		void* form = FindEditedWeaponFormForCombo(combo);
		if (ReadEditorFormType(form) != kFormTypeWeapon)
			return false;

		const UInt32 id = ReadEditorFormId(form);
		if (!id)
			return false;

		if (formId)
			*formId = id;
		return true;
	}

	static bool PersistWeaponTypeKindForFormId(UInt32 formId, SpearSkillShared::WeaponSkillKind kind, const char* value)
	{
		if (!formId)
			return false;

		bool changed = false;
		if (kind != SpearSkillShared::kWeaponSkill_None)
		{
			SpearSkillShared::WeaponSkillKind existing = SpearSkillShared::kWeaponSkill_None;
			const bool hadExisting = g_weaponTypeStore.TryGet(formId, &existing);
			if (!hadExisting || existing != kind)
				changed = true;
			if (!g_weaponTypeStore.Set(formId, kind))
			{
				LogWarning("Spear weapon Type sidecar table is full; cannot store %08X", formId);
				return false;
			}
		}
		else
		{
			changed = g_weaponTypeStore.Remove(formId);
		}

		if (changed)
			RefreshVisibleListViews();

		if (changed || g_weaponTypeStore.IsDirty())
		{
			if (!GetActiveEditorFile())
			{
				if (!g_loggedWeaponTypeNoActivePlugin)
				{
					LogWarning("weapon %08X Type sidecar=%s is affixed for this editor session only; activate a plugin to persist it to %s",
						formId,
						kind != SpearSkillShared::kWeaponSkill_None ? (value ? value : "Spear") : "native",
						kWeaponTypeCarrierEditorId);
					g_loggedWeaponTypeNoActivePlugin = true;
				}
				return true;
			}

			SaveWeaponTypeStore();
		}

		return true;
	}

	static bool GetSelectedWeaponTypeKind(HWND combo, SpearSkillShared::WeaponSkillKind* outKind, std::string* outValue)
	{
		if (outKind)
			*outKind = SpearSkillShared::kWeaponSkill_None;
		if (outValue)
			outValue->clear();
		if (!combo)
			return false;

		const int selection = GetActualWeaponTypeComboSelection(combo);
		if (selection == CB_ERR)
			return false;

		std::string value;
		if (!ReadComboString(combo, selection, value))
			return false;

		if (outKind)
			*outKind = KindForSidecarWeaponTypeLabel(value.c_str());
		if (outValue)
			*outValue = value;
		return true;
	}

	static void PersistWeaponTypeSelection(HWND combo)
	{
		if (!combo || GetPropA(combo, kWeaponTypeComboApplyingStoredSelectionProp))
			return;

		LoadWeaponTypeStoreOnce();

		SpearSkillShared::WeaponSkillKind kind = SpearSkillShared::kWeaponSkill_None;
		std::string value;
		if (!GetSelectedWeaponTypeKind(combo, &kind, &value))
			return;

		UInt32 formId = 0;
		if (!GetEditedWeaponFormIdForCombo(combo, &formId))
		{
			if (!g_loggedWeaponTypeNoForm)
			{
				LogWarning("cannot persist Spear weapon Type sidecar; edited WEAP form was not resolved for combo=%p", combo);
				g_loggedWeaponTypeNoForm = true;
			}
			return;
		}
		SetPropA(combo, kWeaponTypeComboStoredSelectionAppliedProp, reinterpret_cast<HANDLE>(1));

		PersistWeaponTypeKindForFormId(formId, kind, value.c_str());
	}

	static bool ApplyStoredWeaponTypeSelection(HWND combo)
	{
		if (!combo)
			return false;

		LoadWeaponTypeStoreOnce();

		UInt32 formId = 0;
		if (!GetEditedWeaponFormIdForCombo(combo, &formId))
			return false;

		SpearSkillShared::WeaponSkillKind kind = SpearSkillShared::kWeaponSkill_None;
		if (!g_weaponTypeStore.TryGet(formId, &kind))
			return true;

		const char* label = LabelForSidecarWeaponTypeKind(kind);
		const int index = FindComboStringExact(combo, label);
		if (index == CB_ERR)
			return true;

		const int current = GetActualWeaponTypeComboSelection(combo);
		std::string currentValue;
		if (current != CB_ERR && ReadComboString(combo, current, currentValue) && !_stricmp(currentValue.c_str(), label))
			return true;

		SetPropA(combo, kWeaponTypeComboApplyingStoredSelectionProp, reinterpret_cast<HANDLE>(1));
		SendMessageA(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
		RemovePropA(combo, kWeaponTypeComboApplyingStoredSelectionProp);
		RefreshWeaponTypeComboDisplay(combo);
		return true;
	}

	static bool PrepareNativeWeaponTypeSelectionForSave(HWND combo, int* restoreSelection)
	{
		if (restoreSelection)
			*restoreSelection = CB_ERR;
		if (!combo)
			return false;

		const int selection = GetActualWeaponTypeComboSelection(combo);
		if (selection == CB_ERR)
			return false;

		std::string value;
		if (!ReadComboString(combo, selection, value) || !IsSidecarWeaponTypeLabel(value.c_str()))
			return false;

		const int fallback = FindNativeWeaponTypeString(combo,
			NativeFallbackLabelForSidecarWeaponTypeKind(KindForSidecarWeaponTypeLabel(value.c_str())));
		if (fallback == CB_ERR)
			return false;

		SetPropA(combo, kWeaponTypeComboApplyingStoredSelectionProp, reinterpret_cast<HANDLE>(1));
		SetActualWeaponTypeComboSelection(combo, fallback);
		RemovePropA(combo, kWeaponTypeComboApplyingStoredSelectionProp);
		if (restoreSelection)
			*restoreSelection = selection;
		return true;
	}

	static void RestoreWeaponTypeSelectionAfterSave(HWND combo, int restoreSelection)
	{
		if (!combo || restoreSelection == CB_ERR || !IsWindow(combo))
			return;

		SetPropA(combo, kWeaponTypeComboApplyingStoredSelectionProp, reinterpret_cast<HANDLE>(1));
		SetActualWeaponTypeComboSelection(combo, restoreSelection);
		RemovePropA(combo, kWeaponTypeComboApplyingStoredSelectionProp);
		RefreshWeaponTypeComboDisplay(combo);
	}

	static WNDPROC GetOriginalWeaponTypeParentProc(HWND parent)
	{
		return reinterpret_cast<WNDPROC>(GetPropA(parent, kWeaponTypeParentOriginalProcProp));
	}

	static LRESULT CALLBACK WeaponTypeParentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC original = GetOriginalWeaponTypeParentProc(hwnd);
		if (!original)
			return DefWindowProcA(hwnd, msg, wParam, lParam);

		if (msg == WM_NCDESTROY)
		{
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
			RemovePropA(hwnd, kWeaponTypeParentPatchedProp);
			RemovePropA(hwnd, kWeaponTypeParentOriginalProcProp);
			RemovePropA(hwnd, kWeaponTypeParentComboProp);
			return CallWindowProcA(original, hwnd, msg, wParam, lParam);
		}

		if (msg == WM_COMMAND && lParam)
		{
			HWND combo = reinterpret_cast<HWND>(lParam);
			const UINT notification = HIWORD(wParam);
			if (GetPropA(combo, kWeaponTypeComboPatchedProp) &&
				(notification == CBN_SELENDOK || notification == CBN_CLOSEUP))
			{
				RememberNativeWeaponTypeSelection(combo);
				PersistWeaponTypeSelection(combo);
			}
		}

		if (msg == WM_COMMAND && LOWORD(wParam) == IDOK)
		{
			HWND combo = reinterpret_cast<HWND>(GetPropA(hwnd, kWeaponTypeParentComboProp));
			if (combo && GetPropA(combo, kWeaponTypeComboPatchedProp))
			{
				SpearSkillShared::WeaponSkillKind selectedKind = SpearSkillShared::kWeaponSkill_None;
				std::string selectedValue;
				GetSelectedWeaponTypeKind(combo, &selectedKind, &selectedValue);
				void* boundForm = GetBoundWeaponTypeComboForm(combo);
				PersistWeaponTypeSelection(combo);
				int restoreSelection = CB_ERR;
				PrepareNativeWeaponTypeSelectionForSave(combo, &restoreSelection);
				const LRESULT result = CallWindowProcA(original, hwnd, msg, wParam, lParam);
				RestoreWeaponTypeSelectionAfterSave(combo, restoreSelection);
				if (selectedKind != SpearSkillShared::kWeaponSkill_None)
				{
					UInt32 savedFormId = ReadEditorFormId(boundForm);
					if (!savedFormId && IsWindow(combo))
						GetEditedWeaponFormIdForCombo(combo, &savedFormId);
					if (savedFormId)
						PersistWeaponTypeKindForFormId(savedFormId, selectedKind, selectedValue.c_str());
				}
				return result;
			}
		}

		return CallWindowProcA(original, hwnd, msg, wParam, lParam);
	}

	static void PatchWeaponTypeParentWindow(HWND parent, HWND combo)
	{
		if (!parent)
			return;

		SetPropA(parent, kWeaponTypeParentComboProp, combo);
		if (GetPropA(parent, kWeaponTypeParentPatchedProp))
			return;

		WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(parent, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WeaponTypeParentProc)));
		if (!original)
		{
			RemovePropA(parent, kWeaponTypeParentComboProp);
			return;
		}

		SetPropA(parent, kWeaponTypeParentOriginalProcProp, reinterpret_cast<HANDLE>(original));
		SetPropA(parent, kWeaponTypeParentPatchedProp, reinterpret_cast<HANDLE>(1));
	}

	static void PatchWeaponTypeParentDialog(HWND combo)
	{
		PatchWeaponTypeParentWindow(GetParent(combo), combo);

		HWND dialog = FindParentWeaponDialog(combo);
		if (dialog && dialog != GetParent(combo))
			PatchWeaponTypeParentWindow(dialog, combo);
	}

	static bool ComboHasString(HWND combo, const char* text)
	{
		return FindNativeWeaponTypeString(combo, text) != CB_ERR;
	}

	static bool IsWeaponTypeCombo(HWND combo)
	{
		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		return IsComboBox(combo) &&
			GetDlgCtrlID(combo) == kWeaponDialogTypeComboId &&
			count >= 6 &&
			ComboHasString(combo, kNativeWeaponTypeBladeOneHand) &&
			ComboHasString(combo, kNativeWeaponTypeBladeTwoHand) &&
			ComboHasString(combo, kNativeWeaponTypeBluntOneHand) &&
			ComboHasString(combo, kNativeWeaponTypeBluntTwoHand) &&
			ComboHasString(combo, kNativeWeaponTypeStaff) &&
			ComboHasString(combo, kNativeWeaponTypeBow);
	}

	static void MakeWeaponTypeComboScrollable(HWND combo)
	{
		if (!combo)
			return;
		if (GetPropA(combo, kWeaponTypeComboScrollableProp))
			return;
		if (SendMessageA(combo, CB_GETDROPPEDSTATE, 0, 0))
			return;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		const int itemHeight = static_cast<int>(SendMessageA(combo, CB_GETITEMHEIGHT, 0, 0));
		const int selectionHeight = static_cast<int>(SendMessageA(combo, CB_GETITEMHEIGHT, static_cast<WPARAM>(-1), 0));
		if (count <= 0 || itemHeight <= 0 || selectionHeight <= 0)
			return;

		SendMessageA(combo, CB_SETMINVISIBLE, static_cast<WPARAM>(kWeaponTypeComboMinimumVisibleRows), 0);

		RECT rect = {};
		if (!GetWindowRect(combo, &rect))
			return;

		const int width = rect.right - rect.left;
		const int visibleRows = count > kWeaponTypeComboMinimumVisibleRows ? kWeaponTypeComboMinimumVisibleRows : count;
		const int height = selectionHeight + (itemHeight * visibleRows) + 10;
		const LONG_PTR style = GetWindowLongPtrA(combo, GWL_STYLE);
		SetWindowLongPtrA(combo, GWL_STYLE, style | WS_VSCROLL | CBS_DISABLENOSCROLL);
		SetWindowPos(combo, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		SetPropA(combo, kWeaponTypeComboScrollableProp, reinterpret_cast<HANDLE>(1));
	}

	static void RememberNativeWeaponTypeSelection(HWND combo)
	{
		const int selection = GetActualWeaponTypeComboSelection(combo);
		if (selection == CB_ERR)
			return;

		std::string value;
		if (ReadComboString(combo, selection, value) &&
			!IsSidecarWeaponTypeLabel(value.c_str()))
			SetPropA(combo, kWeaponTypeComboLastNativeSelectionProp, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(selection + 1)));
	}

	static WNDPROC GetOriginalWeaponTypeComboProc(HWND combo)
	{
		return reinterpret_cast<WNDPROC>(GetPropA(combo, kWeaponTypeComboOriginalProcProp));
	}

	static LRESULT CALLBACK WeaponTypeComboProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC original = GetOriginalWeaponTypeComboProc(hwnd);
		if (!original)
			return DefWindowProcA(hwnd, msg, wParam, lParam);

		if (msg == WM_NCDESTROY)
		{
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
			RemovePropA(hwnd, kWeaponTypeComboPatchedProp);
			RemovePropA(hwnd, kWeaponTypeComboOriginalProcProp);
			RemovePropA(hwnd, kWeaponTypeComboLastNativeSelectionProp);
			RemovePropA(hwnd, kWeaponTypeComboScrollableProp);
			RemovePropA(hwnd, kWeaponTypeComboApplyingStoredSelectionProp);
			RemovePropA(hwnd, kWeaponTypeComboStoredSelectionAppliedProp);
			RemovePropA(hwnd, kWeaponTypeComboRejectedProp);
			RemovePropA(hwnd, kWeaponTypeComboBoundFormPtrProp);
			RemovePropA(hwnd, kWeaponTypeComboBoundFormIdProp);
			return CallWindowProcA(original, hwnd, msg, wParam, lParam);
		}

		if (msg == WM_MBUTTONDOWN)
		{
			SetFocus(hwnd);
			SendMessageA(hwnd, CB_SHOWDROPDOWN, TRUE, 0);
			return 0;
		}

		const LRESULT result = CallWindowProcA(original, hwnd, msg, wParam, lParam);
		if (msg == CB_SETCURSEL || msg == WM_SETTEXT)
			RememberNativeWeaponTypeSelection(hwnd);
		if (msg == CBN_SELENDOK || msg == CBN_CLOSEUP || msg == WM_KILLFOCUS)
			PersistWeaponTypeSelection(hwnd);
		return result;
	}

	static void AddSidecarWeaponTypeComboItem(HWND combo, const char* label, const char* nativeFallback)
	{
		if (ComboHasString(combo, label))
			return;

		const int fallback = FindNativeWeaponTypeString(combo, nativeFallback);
		if (fallback != CB_ERR)
		{
			LRESULT fallbackData = SendMessageA(combo, CB_GETITEMDATA, fallback, 0);
			if (fallbackData == CB_ERR)
				fallbackData = fallback;
			SendMessageA(combo, kCseDeferredComboAddItem, reinterpret_cast<WPARAM>(label), fallbackData);
			SendMessageA(combo, CB_GETCOUNT, 0, 0);
			if (ComboHasString(combo, label))
				return;

			const int index = static_cast<int>(SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label)));
			if (index != CB_ERR && index != CB_ERRSPACE && index != kCseDeferredAddStringMarkerResult)
				SendMessageA(combo, CB_SETITEMDATA, index, fallbackData);
		}
	}

	static void PatchWeaponTypeCombo(HWND combo)
	{
		if (g_insideWeaponTypeComboPatch || !combo)
			return;

		if (GetPropA(combo, kWeaponTypeComboPatchedProp))
		{
			if (!GetPropA(combo, kWeaponTypeComboStoredSelectionAppliedProp) &&
				!SendMessageA(combo, CB_GETDROPPEDSTATE, 0, 0) &&
				ApplyStoredWeaponTypeSelection(combo))
			{
				SetPropA(combo, kWeaponTypeComboStoredSelectionAppliedProp, reinterpret_cast<HANDLE>(1));
			}
			return;
		}

		if (GetPropA(combo, kWeaponTypeComboRejectedProp))
			return;
		if (SendMessageA(combo, CB_GETDROPPEDSTATE, 0, 0))
			return;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		if (count < 6)
			return;
		if (!IsWeaponTypeCombo(combo))
		{
			SetPropA(combo, kWeaponTypeComboRejectedProp, reinterpret_cast<HANDLE>(1));
			return;
		}

		g_insideWeaponTypeComboPatch = true;
		RememberNativeWeaponTypeSelection(combo);
		AddSidecarWeaponTypeComboItem(combo, kSpearEditorLabel, kNativeWeaponTypeBladeTwoHand);
		MakeWeaponTypeComboScrollable(combo);

		if (!GetPropA(combo, kWeaponTypeComboPatchedProp))
		{
			WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(combo, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WeaponTypeComboProc)));
			if (original)
			{
				SetPropA(combo, kWeaponTypeComboOriginalProcProp, reinterpret_cast<HANDLE>(original));
				SetPropA(combo, kWeaponTypeComboPatchedProp, reinterpret_cast<HANDLE>(1));
			}
		}
		PatchWeaponTypeParentDialog(combo);
		if (ApplyStoredWeaponTypeSelection(combo))
			SetPropA(combo, kWeaponTypeComboStoredSelectionAppliedProp, reinterpret_cast<HANDLE>(1));
		g_insideWeaponTypeComboPatch = false;
	}

	static BOOL CALLBACK PatchWeaponTypeCombosInWindow(HWND hwnd, LPARAM)
	{
		if (!IsWindowVisible(hwnd))
			return TRUE;
		if (IsComboBox(hwnd))
			PatchWeaponTypeCombo(hwnd);
		else
			EnumChildWindows(hwnd, PatchWeaponTypeCombosInWindow, 0);
		return TRUE;
	}

	static void PatchVisibleWeaponTypeCombos()
	{
		EnumThreadWindows(GetCurrentThreadId(), PatchWeaponTypeCombosInWindow, 0);
	}

	static void CALLBACK WeaponTypeComboPollTimerProc(HWND, UINT, UINT_PTR, DWORD)
	{
		PatchVisibleWeaponTypeCombos();
	}

	static void StartWeaponTypeDropdownPolling()
	{
		if (!g_weaponTypeComboPollTimer)
			g_weaponTypeComboPollTimer = SetTimer(nullptr, kWeaponTypeComboPollTimerId, kWeaponTypeComboPollIntervalMs, WeaponTypeComboPollTimerProc);
		if (!g_weaponTypeComboPollTimer)
			LogWarning("failed to start Spear weapon Type dropdown sidecar polling gle=%u", GetLastError());
	}

	static bool BytesEqual(UInt32 address, const UInt8* expected, UInt32 length)
	{
		return std::memcmp(reinterpret_cast<const void*>(address), expected, length) == 0;
	}

	static UInt32 ReadRelJumpTarget(UInt32 address)
	{
		const SInt32 offset = *reinterpret_cast<const SInt32*>(address + 1);
		return address + 5 + offset;
	}

	static bool WriteRelJumpWithTrampolineChecked(const char* name, UInt32 address, const UInt8* expected,
		UInt32 expectedLength, UInt32 target, UInt8* trampoline, UInt32 trampolineLength, UInt32* originalOut)
	{
		if (expectedLength < 5 || trampolineLength < expectedLength + 5)
		{
			LogError("invalid trampoline patch length for %s at %08X length=%u trampoline=%u",
				name, address, expectedLength, trampolineLength);
			return false;
		}

		if (!BytesEqual(address, expected, expectedLength))
		{
			const UInt8* actual = reinterpret_cast<const UInt8*>(address);
			if (actual[0] == 0xE9)
			{
				const UInt32 existingTarget = ReadRelJumpTarget(address);
				if (existingTarget == target)
				{
					LogMessage("%s already installed at %08X", name, address);
					return true;
				}
				else
				{
					if (originalOut)
						*originalOut = existingTarget;
					LogMessage("%s chaining existing hook at %08X target=%08X",
						name, address, existingTarget);
				}
			}
			else
			{
				LogError("signature mismatch for %s at %08X expected %02X %02X %02X %02X %02X actual %02X %02X %02X %02X %02X",
					name, address,
					expected[0], expected[1], expected[2], expected[3], expected[4],
					actual[0], actual[1], actual[2], actual[3], actual[4]);
				return false;
			}
		}
		else
		{
			std::memcpy(trampoline, reinterpret_cast<const void*>(address), expectedLength);
			trampoline[expectedLength] = 0xE9;
			*reinterpret_cast<UInt32*>(trampoline + expectedLength + 1) =
				address + expectedLength - (reinterpret_cast<UInt32>(trampoline) + expectedLength + 5);

			DWORD oldTrampolineProtect = 0;
			if (!VirtualProtect(trampoline, trampolineLength, PAGE_EXECUTE_READWRITE, &oldTrampolineProtect))
			{
				LogError("VirtualProtect failed for %s trampoline gle=%u", name, GetLastError());
				return false;
			}
			FlushInstructionCache(GetCurrentProcess(), trampoline, trampolineLength);

			if (originalOut)
				*originalOut = reinterpret_cast<UInt32>(trampoline);
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, expectedLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			LogError("VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < expectedLength; ++i)
			code[i] = 0x90;

		FlushInstructionCache(GetCurrentProcess(), ptr, expectedLength);

		DWORD ignored = 0;
		VirtualProtect(ptr, expectedLength, oldProtect, &ignored);

		LogMessage("installed %s at %08X", name, address);
		return true;
	}

	static bool WriteRelJumpChainedChecked(const char* name, UInt32 address, const UInt8* expected,
		UInt32 expectedLength, UInt32 target, UInt32* chainTargetOut)
	{
		if (chainTargetOut)
			*chainTargetOut = 0;

		if (expectedLength < 5)
		{
			LogError("invalid patch length for %s at %08X length=%u", name, address, expectedLength);
			return false;
		}

		if (!BytesEqual(address, expected, expectedLength))
		{
			const UInt8* actual = reinterpret_cast<const UInt8*>(address);
			if (actual[0] == 0xE9)
			{
				const UInt32 existingTarget = ReadRelJumpTarget(address);
				if (existingTarget == target)
				{
					LogMessage("%s already installed at %08X", name, address);
					return true;
				}

				if (chainTargetOut)
					*chainTargetOut = existingTarget;
				LogMessage("%s chaining existing hook at %08X target=%08X",
					name, address, existingTarget);
			}
			else
			{
				LogError("signature mismatch for %s at %08X expected %02X %02X %02X %02X %02X actual %02X %02X %02X %02X %02X",
					name, address,
					expected[0], expected[1], expected[2], expected[3], expected[4],
					actual[0], actual[1], actual[2], actual[3], actual[4]);
				return false;
			}
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, expectedLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			LogError("VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < expectedLength; ++i)
			code[i] = 0x90;

		FlushInstructionCache(GetCurrentProcess(), ptr, expectedLength);

		DWORD ignored = 0;
		VirtualProtect(ptr, expectedLength, oldProtect, &ignored);

		LogMessage("installed %s at %08X", name, address);
		return true;
	}

	static bool WriteRelCallChainedChecked(const char* name, UInt32 address, const UInt8* expected,
		UInt32 expectedLength, UInt32 target, UInt32 expectedOriginalTarget, UInt32* originalOut, bool* originalIsFastcall)
	{
		if (originalOut)
			*originalOut = expectedOriginalTarget;
		if (originalIsFastcall)
			*originalIsFastcall = false;

		if (expectedLength < 5)
		{
			LogError("invalid patch length for %s at %08X length=%u", name, address, expectedLength);
			return false;
		}

		if (!BytesEqual(address, expected, expectedLength))
		{
			const UInt8* actual = reinterpret_cast<const UInt8*>(address);
			if (actual[0] == 0xE8)
			{
				const UInt32 existingTarget = ReadRelJumpTarget(address);
				if (existingTarget == target)
				{
					LogMessage("%s already installed at %08X", name, address);
					return true;
				}

				if (originalOut)
					*originalOut = existingTarget;
				if (originalIsFastcall)
					*originalIsFastcall = true;
				LogMessage("%s chaining existing call hook at %08X target=%08X",
					name, address, existingTarget);
			}
			else
			{
				LogError("signature mismatch for %s at %08X expected %02X %02X %02X %02X %02X actual %02X %02X %02X %02X %02X",
					name, address,
					expected[0], expected[1], expected[2], expected[3], expected[4],
					actual[0], actual[1], actual[2], actual[3], actual[4]);
				return false;
			}
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, expectedLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			LogError("VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE8;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < expectedLength; ++i)
			code[i] = 0x90;

		FlushInstructionCache(GetCurrentProcess(), ptr, expectedLength);

		DWORD ignored = 0;
		VirtualProtect(ptr, expectedLength, oldProtect, &ignored);

		LogMessage("installed %s at %08X", name, address);
		return true;
	}

	static bool WritePointerChainedChecked(const char* name, UInt32 address, UInt32 expected, UInt32 target,
		UInt32* originalOut, bool* originalIsFastcall)
	{
		if (originalOut)
			*originalOut = expected;
		if (originalIsFastcall)
			*originalIsFastcall = false;

		const UInt32 actual = *reinterpret_cast<const UInt32*>(address);
		if (actual != expected)
		{
			if (actual == target)
			{
				LogMessage("%s already installed at %08X", name, address);
				return true;
			}

			if (originalOut)
				*originalOut = actual;
			if (originalIsFastcall)
				*originalIsFastcall = true;
			LogMessage("%s chaining existing pointer hook at %08X target=%08X",
				name, address, actual);
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, sizeof(UInt32), PAGE_READWRITE, &oldProtect))
		{
			LogError("VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		*reinterpret_cast<UInt32*>(ptr) = target;
		FlushInstructionCache(GetCurrentProcess(), ptr, sizeof(UInt32));

		DWORD ignored = 0;
		VirtualProtect(ptr, sizeof(UInt32), oldProtect, &ignored);

		LogMessage("installed %s at %08X", name, address);
		return true;
	}

	static bool InstallWeaponDialogTypeComboHook()
	{
		UInt32 weaponDialogInitOriginal = 0;
		const bool ok = WriteRelJumpWithTrampolineChecked("weapon dialog Spear Type sidecar initialization",
			kWeaponDialogInitPatch, kExpectedWeaponDialogInit, sizeof(kExpectedWeaponDialogInit),
			reinterpret_cast<UInt32>(&HookWeaponDialogInit),
			g_weaponDialogInitTrampoline, sizeof(g_weaponDialogInitTrampoline),
			&weaponDialogInitOriginal);
		if (weaponDialogInitOriginal)
			g_weaponDialogInitOriginal = reinterpret_cast<WeaponDialogInitFn>(weaponDialogInitOriginal);
		return ok;
	}

	static bool InstallObjectWindowWeaponTypeColumnHook()
	{
		UInt32 objectWindowOriginal = 0;
		const bool ok = WriteRelJumpWithTrampolineChecked("object window Spear weapon Type sidecar display",
			kObjectWindowListViewGetDispInfoPatch, kExpectedObjectWindowListViewGetDispInfo,
			sizeof(kExpectedObjectWindowListViewGetDispInfo), reinterpret_cast<UInt32>(&ObjectWindowListViewGetDispInfoHook),
			g_objectWindowListViewGetDispInfoTrampoline, sizeof(g_objectWindowListViewGetDispInfoTrampoline),
			&objectWindowOriginal);
		if (objectWindowOriginal)
			g_objectWindowListViewGetDispInfoOriginal = reinterpret_cast<ObjectWindowListViewGetDispInfoFn>(objectWindowOriginal);
		return ok;
	}

	static bool InstallNpcStatsSidecarHooks()
	{
		LoadNpcSpearStoreOnce();

		bool ok = true;
		UInt32 populateOriginal = kNpcStatsPopulateOriginal;
		bool populateFastcall = false;
		ok &= WritePointerChainedChecked("NPC stats Spear sidecar row",
			kNpcStatsPopulateVtableEntry, kNpcStatsPopulateOriginal, reinterpret_cast<UInt32>(&NpcStatsPopulateHook),
			&populateOriginal, &populateFastcall);
		g_npcStatsPopulateOriginalTarget = populateOriginal;
		g_npcStatsPopulateOriginalIsFastcall = populateFastcall;

		UInt32 initOriginal = kNpcStatsInitSkillListOriginal;
		bool initFastcall = false;
		ok &= WriteRelCallChainedChecked("NPC stats Spear skill list open",
			kNpcStatsInitSkillListOpenCall, kExpectedNpcStatsInitSkillListOpenCall, sizeof(kExpectedNpcStatsInitSkillListOpenCall),
			reinterpret_cast<UInt32>(&NpcStatsInitSkillListHook), kNpcStatsInitSkillListOriginal,
			&initOriginal, &initFastcall);
		g_npcStatsInitSkillListOriginalTarget = initOriginal;
		g_npcStatsInitSkillListOriginalIsFastcall = initFastcall;

		initOriginal = kNpcStatsInitSkillListOriginal;
		initFastcall = false;
		ok &= WriteRelCallChainedChecked("NPC stats Spear skill list refresh",
			kNpcStatsInitSkillListCall, kExpectedNpcStatsInitSkillListCall, sizeof(kExpectedNpcStatsInitSkillListCall),
			reinterpret_cast<UInt32>(&NpcStatsInitSkillListHook), kNpcStatsInitSkillListOriginal,
			&initOriginal, &initFastcall);
		if (g_npcStatsInitSkillListOriginalTarget == kNpcStatsInitSkillListOriginal || initFastcall)
		{
			g_npcStatsInitSkillListOriginalTarget = initOriginal;
			g_npcStatsInitSkillListOriginalIsFastcall = initFastcall;
		}

		UInt32 editChainTarget = 0;
		ok &= WriteRelJumpChainedChecked("NPC stats Spear auto-calc sidecar value save",
			kNpcStatsSkillEditAutoCalcGatePatch, kExpectedNpcStatsSkillEditAutoCalcGate, sizeof(kExpectedNpcStatsSkillEditAutoCalcGate),
			reinterpret_cast<UInt32>(&NpcStatsSkillEditAutoCalcGateHook), &editChainTarget);
		g_npcStatsSkillEditAutoCalcGateChainTarget = editChainTarget;

		editChainTarget = 0;
		ok &= WriteRelJumpChainedChecked("NPC stats Spear sidecar value save",
			kNpcStatsSkillEditCommitPatch, kExpectedNpcStatsSkillEditCommit, sizeof(kExpectedNpcStatsSkillEditCommit),
			reinterpret_cast<UInt32>(&NpcStatsSkillEditCommitHook), &editChainTarget);
		g_npcStatsSkillEditCommitChainTarget = editChainTarget;
		return ok;
	}

	static bool InstallNpcAiTrainingSidecarHooks()
	{
		LoadNpcSpearTrainingStoreOnce();

		bool ok = true;
		UInt32 initOriginal = 0;
		ok &= WriteRelJumpWithTrampolineChecked("NPC AI Spear training dropdown initialization",
			kAiFormInitDialogControls,
			kExpectedAiFormInitDialogControls,
			sizeof(kExpectedAiFormInitDialogControls),
			reinterpret_cast<UInt32>(&AiFormInitDialogControlsHook),
			g_aiFormInitDialogControlsTrampoline,
			sizeof(g_aiFormInitDialogControlsTrampoline),
			&initOriginal);
		if (initOriginal)
			g_aiFormInitDialogControlsOriginal = reinterpret_cast<AiFormInitDialogControlsFn>(initOriginal);

		UInt32 saveOriginal = 0;
		ok &= WriteRelJumpWithTrampolineChecked("NPC AI Spear training sidecar save",
			kAiFormSaveDialogControls,
			kExpectedAiFormSaveDialogControls,
			sizeof(kExpectedAiFormSaveDialogControls),
			reinterpret_cast<UInt32>(&AiFormSaveDialogControlsHook),
			g_aiFormSaveDialogControlsTrampoline,
			sizeof(g_aiFormSaveDialogControlsTrampoline),
			&saveOriginal);
		if (saveOriginal)
			g_aiFormSaveDialogControlsOriginal = reinterpret_cast<AiFormSaveDialogControlsFn>(saveOriginal);

		return ok;
	}

	static bool InstallSavePluginSidecarFlushHook()
	{
		UInt32 chainTarget = 0;
		const bool ok = WriteRelJumpChainedChecked("Spear sidecar pre-save carrier flush",
			kSavePluginPreWritePatch,
			kExpectedSavePluginPreWrite,
			sizeof(kExpectedSavePluginPreWrite),
			reinterpret_cast<UInt32>(&SavePluginPreWriteHook),
			&chainTarget);
		g_savePluginPreWriteChainTarget = chainTarget;
		return ok;
	}

	static void InitializeEditorWeaponTypeSurface()
	{
		LoadWeaponTypeStoreOnce();
		StartWeaponTypeDropdownPolling();
		if (!InstallWeaponDialogTypeComboHook())
			LogWarning("weapon dialog Spear Type sidecar initialization hook was not installed");
		if (!InstallObjectWindowWeaponTypeColumnHook())
			LogWarning("object window Spear weapon Type sidecar display hook was not installed");
	}

	static void InitializeEditorNpcStatsSurface()
	{
		if (!InstallNpcStatsSidecarHooks())
			LogWarning("NPC stats Spear sidecar hooks were not fully installed");
	}

	static void InitializeEditorNpcAiTrainingSurface()
	{
		if (!InstallNpcAiTrainingSidecarHooks())
			LogWarning("NPC AI Spear training sidecar hooks were not fully installed");
	}

	static void InitializeEditorSaveSurface()
	{
		if (!InstallSavePluginSidecarFlushHook())
			LogWarning("Spear sidecar pre-save carrier flush hook was not installed");
	}

	static bool ProviderIsSpearSkill(unsigned long skillId)
	{
		return skillId == SpearSkillShared::kSpearSkillId;
	}

	static bool ProviderValueToLevel(double value, UInt32* outLevel)
	{
		if (outLevel)
			*outLevel = 0;
		if (!(value == value))
			return false;
		if (value <= 0.0)
			return true;
		if (value >= 100.0)
		{
			if (outLevel)
				*outLevel = 100;
			return true;
		}
		if (outLevel)
			*outLevel = static_cast<UInt32>(value);
		return true;
	}

	static bool ProviderValueToDelta(double value, SInt32* outDelta)
	{
		if (outDelta)
			*outDelta = 0;
		if (!(value == value))
			return false;
		if (value >= 2147483647.0)
		{
			if (outDelta)
				*outDelta = 2147483647;
			return true;
		}
		if (value <= -2147483648.0)
		{
			if (outDelta)
				*outDelta = static_cast<SInt32>(0x80000000);
			return true;
		}
		if (outDelta)
			*outDelta = static_cast<SInt32>(value);
		return true;
	}

	static bool ProviderIsSpearWeapon(unsigned long skillId, void* form, bool* outResult)
	{
		if (outResult)
			*outResult = false;
		if (!ProviderIsSpearSkill(skillId))
			return false;
		if (TryReadEditorFormType(form) != kFormTypeWeapon)
			return true;

		LoadWeaponTypeStoreOnce();
		SpearSkillShared::WeaponSkillKind kind = SpearSkillShared::kWeaponSkill_None;
		if (g_weaponTypeStore.TryGet(ReadEditorFormId(form), &kind) && kind == SpearSkillShared::kWeaponSkill_Spear)
		{
			if (outResult)
				*outResult = true;
		}
		return true;
	}

	static bool ProviderGetNpcSpearEntry(UInt32 formId, SpearSkillShared::NpcSpearEntry* outEntry)
	{
		if (outEntry)
			std::memset(outEntry, 0, sizeof(*outEntry));
		if (!formId)
			return false;

		LoadNpcSpearStoreOnce();
		return g_npcSkillStore.TryGet(formId, outEntry);
	}

	static bool ProviderSetNpcSpearEntry(UInt32 formId, UInt32 level, float progress, UInt32 levelUps)
	{
		if (!formId)
			return false;

		LoadNpcSpearStoreOnce();
		return g_npcSkillStore.Set(formId, level, progress, levelUps);
	}

	static bool ProviderGetNpcSpearAV(void* npc, unsigned long skillId, double* outValue)
	{
		if (outValue)
			*outValue = 0.0;
		if (!ProviderIsSpearSkill(skillId))
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!ProviderGetNpcSpearEntry(ReadNpcFormId(npc), &entry))
			return false;
		if (outValue)
			*outValue = static_cast<double>(SpearSkillShared::NpcSpearStore::ClampLevel(entry.level));
		return true;
	}

	static bool ProviderSetNpcSpearAV(void* npc, unsigned long skillId, double value)
	{
		UInt32 level = 0;
		const UInt32 formId = ReadNpcFormId(npc);
		if (!ProviderIsSpearSkill(skillId) || !ProviderValueToLevel(value, &level) || !formId)
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!ProviderGetNpcSpearEntry(formId, &entry))
		{
			entry.formId = formId;
			entry.progress = 0.0f;
			entry.levelUps = 0;
		}
		return ProviderSetNpcSpearEntry(formId, level, entry.progress, entry.levelUps);
	}

	static bool ProviderModNpcSpearAV(void* npc, unsigned long skillId, double value)
	{
		SInt32 delta = 0;
		const UInt32 formId = ReadNpcFormId(npc);
		if (!ProviderIsSpearSkill(skillId) || !ProviderValueToDelta(value, &delta) || !formId)
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!ProviderGetNpcSpearEntry(formId, &entry))
		{
			entry.formId = formId;
			entry.level = 0;
			entry.progress = 0.0f;
			entry.levelUps = 0;
		}

		const long long adjusted = static_cast<long long>(entry.level) + static_cast<long long>(delta);
		const UInt32 level = adjusted <= 0 ? 0 : adjusted >= 100 ? 100 : static_cast<UInt32>(adjusted);
		const UInt32 levelUps = delta > 0 && level > entry.level ? entry.levelUps + (level - entry.level) : entry.levelUps;
		return ProviderSetNpcSpearEntry(formId, level, entry.progress, levelUps);
	}

	static bool ProviderGetNpcSpearProgress(void* npc, unsigned long skillId, double* outValue)
	{
		if (outValue)
			*outValue = 0.0;
		if (!ProviderIsSpearSkill(skillId))
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!ProviderGetNpcSpearEntry(ReadNpcFormId(npc), &entry))
			return false;
		if (outValue)
			*outValue = static_cast<double>(entry.progress);
		return true;
	}

	static bool ProviderSetNpcSpearProgress(void* npc, unsigned long skillId, double value)
	{
		const UInt32 formId = ReadNpcFormId(npc);
		if (!ProviderIsSpearSkill(skillId) || !(value == value) || !formId)
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!ProviderGetNpcSpearEntry(formId, &entry))
		{
			entry.formId = formId;
			entry.level = 0;
			entry.levelUps = 0;
		}
		return ProviderSetNpcSpearEntry(formId, entry.level, static_cast<float>(value), entry.levelUps);
	}

	static bool ProviderGetNpcSpearLevelUps(void* npc, unsigned long skillId, double* outValue)
	{
		if (outValue)
			*outValue = 0.0;
		if (!ProviderIsSpearSkill(skillId))
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!ProviderGetNpcSpearEntry(ReadNpcFormId(npc), &entry))
			return false;
		if (outValue)
			*outValue = static_cast<double>(entry.levelUps);
		return true;
	}

	static bool ProviderHasNpcSpearSkill(void* npc, unsigned long skillId, bool* outResult)
	{
		if (outResult)
			*outResult = false;
		const UInt32 formId = ReadNpcFormId(npc);
		if (!ProviderIsSpearSkill(skillId) || !formId)
			return false;

		LoadNpcSpearStoreOnce();
		if (outResult)
			*outResult = g_npcSkillStore.Has(formId);
		return true;
	}

	static bool ProviderClearNpcSpearSkill(void* npc, unsigned long skillId)
	{
		const UInt32 formId = ReadNpcFormId(npc);
		if (!ProviderIsSpearSkill(skillId) || !formId)
			return false;

		LoadNpcSpearStoreOnce();
		return g_npcSkillStore.Remove(formId);
	}

	static const SidecarSkillCommandsShared::SidecarSkillProvider kSidecarSkillProviders[] =
	{
		{
			SidecarSkillCommandsShared::kSidecarSkillProviderApiVersion,
			sizeof(SidecarSkillCommandsShared::SidecarSkillProvider),
			SpearSkillShared::kSpearSkillId,
			"Spear",
			"SpearSkill|SPER",
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			&ProviderIsSpearWeapon,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			&ProviderGetNpcSpearAV,
			&ProviderGetNpcSpearAV,
			&ProviderSetNpcSpearAV,
			&ProviderModNpcSpearAV,
			&ProviderSetNpcSpearAV,
			&ProviderGetNpcSpearProgress,
			&ProviderSetNpcSpearProgress,
			nullptr,
			&ProviderGetNpcSpearLevelUps,
			&ProviderHasNpcSpearSkill,
			&ProviderClearNpcSpearSkill,
		},
	};

	static const SidecarSkillCommandsShared::SidecarSkillProviderTable kSidecarSkillProviderTable =
	{
		SidecarSkillCommandsShared::kSidecarSkillProviderApiVersion,
		sizeof(SidecarSkillCommandsShared::SidecarSkillProviderTable),
		sizeof(kSidecarSkillProviders) / sizeof(kSidecarSkillProviders[0]),
		kSidecarSkillProviders,
	};

	static const SidecarSkillCommandsShared::SidecarSkillProviderTable* GetSidecarSkillProviderTableInternal()
	{
		return &kSidecarSkillProviderTable;
	}
}

extern "C"
{
	const SidecarSkillCommandsShared::SidecarSkillProviderTable* GetSidecarSkillProviderTable()
	{
		return SpearSkillCSE::GetSidecarSkillProviderTableInternal();
	}

	bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info)
	{
		if (!obse || !info)
			return false;

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "Spear Skill CS";
		info->version = SpearSkillCSE::kPluginVersion;

		return obse->isEditor != 0;
	}

	bool OBSEPlugin_Load(const OBSEInterface* obse)
	{
		if (!obse || !obse->isEditor)
			return false;

		SpearSkillCSE::InitializeEditorWeaponTypeSurface();
		SpearSkillCSE::InitializeEditorNpcStatsSurface();
		SpearSkillCSE::InitializeEditorNpcAiTrainingSurface();
		SpearSkillCSE::InitializeEditorSaveSurface();
		return true;
	}
}
