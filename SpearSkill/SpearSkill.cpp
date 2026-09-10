#include "obse/PluginAPI.h"
#include "obse/GameAPI.h"
#include "obse/GameActorValues.h"
#include "obse/GameData.h"
#include "obse/GameForms.h"
#include "obse/GameObjects.h"
#include "obse/GameProcess.h"
#include "obse/GameTiles.h"
#include "obse/CommandTable.h"
#include "obse/ParamInfos.h"
#include "obse_common/SafeWrite.h"
#include "SpearNpcSkillSidecar.h"
#include "SpearWeaponTypeSidecar.h"
#include "TrueCustomSkills/TrueCustomSkillsInterface.h"

#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <intrin.h>
#include <vector>
#include <windows.h>

#pragma intrinsic(_ReturnAddress)

IDebugLog gLog("SpearSkill.log");

PluginHandle g_pluginHandle = kPluginHandle_Invalid;
OBSESerializationInterface* g_serialization = nullptr;

namespace SpearSkill
{
	static OBSEMessagingInterface* g_messaging = nullptr;
	static TrueCustomSkillsInterface* g_tcs = nullptr;

	static constexpr UInt32 kPluginVersion = 1;
	static constexpr UInt32 kSaveVersion = 1;
	static constexpr UInt32 kRecordState = ('S') | ('P' << 8) | ('E' << 16) | ('A' << 24);
	static constexpr UInt32 kRecordWeaponTypes = ('S') | ('P' << 8) | ('W' << 16) | ('T' << 24);
	static constexpr UInt32 kWeaponTypesRecordVersion = 1;
	static constexpr UInt32 kMaxSavedWeaponTypeEntries = 4096;
	static constexpr UInt32 kMaxSkillLevel = 100;
	static constexpr UInt32 kSkillCount = 1;
	static constexpr UInt32 kSpearSkillIndex = 0;
	static constexpr UInt32 kWeaponSuccessfulHitUseType = 0;
	static constexpr UInt32 kNativeClassMajorCount = 7;
	static constexpr UInt32 kNativeSkillCount = 21;
	static constexpr UInt32 kFirstNativeSkillAV = 0x0C;
	static constexpr UInt32 kLastNativeSkillAV = 0x20;
	static constexpr float kProgressEpsilon = 0.0001f;
	static constexpr const char* kSpearSkillDisplayName = "Spear";
	static constexpr const char* kSpearSkillIconPath = "Menus\\Class\\Attributes\\load_image_spear.dds";
	static constexpr const char* kSpearSkillRowIconPath = "Menus\\Class\\Attributes\\load_image_spear_small.dds";
	static constexpr const char* kVanillaBladeSkillIconPath = "Menus\\Class\\Attributes\\load_image_blade.dds";
	static constexpr const char* kVanillaBladeSkillSmallIconPath = "Menus\\Class\\Attributes\\load_image_blade_small.dds";
	static constexpr const char* kSkillProgressionPlaceholderDescription = "No new Spear mastery perk is unlocked at this level.";
	static constexpr const char* kSpearSkillDescription =
		"Warriors of reach and patience, they keep enemies at bay with long-hafted weapons.  "
		"Strong in endurance, they turn distance into defense and strike before their foes can close.";
	static constexpr const char* kSpearClassPickerDescription = kSpearSkillDescription;
	static constexpr const char* kSpearNoviceRankSummary = "No special Spear mastery perk is unlocked.";
	static constexpr const char* kSpearApprenticeRankSummary = "Standing power attack is unlocked and deals extra damage with Spear weapons.";
	static constexpr const char* kSpearJourneymanRankSummary = "Sidestep power attack has a chance to disarm the target with Spear weapons.";
	static constexpr const char* kSpearExpertRankSummary = "Sweeping/backward power attack has a chance to knock down the target with Spear weapons.";
	static constexpr const char* kSpearMasterRankSummary = "Rushing/forward power attack has a chance to paralyze the target with Spear weapons.";
	static constexpr const char* kSpearNoviceRankDescription =
		"You are a Novice with Spear weapons. You can use spears, halberds, pikes, and similar polearms, but your guard and footwork are still basic. "
		"You have not yet learned a special Spear power attack perk.";
	static constexpr const char* kSpearApprenticeRankDescription =
		"Hours of spear drill have taught you to set your feet, conserve your strength, and strike with the full length of the haft. "
		"You are now an Apprentice with Spear weapons. You have a new Standing power attack, which does extra damage. "
		"Press and hold Attack to use this power attack.";
	static constexpr const char* kSpearJourneymanRankDescription =
		"Hours of spear drill have taught you to turn an opponent's weapon aside while keeping them at the edge of your reach. "
		"You are now a Journeyman with Spear weapons. Your Sidestep power attack now has a chance to disarm your opponent. "
		"Press and hold Attack while moving left or right to use this power attack.";
	static constexpr const char* kSpearExpertRankDescription =
		"Hours of spear drill have taught you to stop an enemy's advance with a braced thrust before they can crowd your weapon. "
		"You are now an Expert with Spear weapons. Your Sweeping power attack now has a chance to knock down your opponent. "
		"Press and hold Attack while moving backward to use this power attack.";
	static constexpr const char* kSpearMasterRankDescription =
		"Hours of spear drill have taught you to finish a fight before your opponent can close the distance. "
		"You are now a Master with Spear weapons. Your Rushing power attack now has a chance to paralyze your opponent. "
		"Press and hold Attack while moving forward to use this power attack.";
	static constexpr const char* kSpearApprenticeUpgradeDescription = kSpearApprenticeRankSummary;
	static constexpr const char* kSpearJourneymanUpgradeDescription = kSpearJourneymanRankSummary;
	static constexpr const char* kSpearExpertUpgradeDescription = kSpearExpertRankSummary;
	static constexpr const char* kSpearMasterUpgradeDescription = kSpearMasterRankSummary;
	static constexpr UInt32 kSkillProgressionLevelCount = kMaxSkillLevel + 1;

	static constexpr UInt32 kPlayerModExperience = 0x668C30;
	static constexpr UInt32 kPlayerModExperiencePatchLength = 7;
	static constexpr UInt32 kTESActorBaseGetEquippableItemRating = 0x0051A120;
	static constexpr UInt32 kTESActorBaseGetEquippableItemRatingPatchLength = 5;
	static constexpr UInt32 kEquippableWeaponRatingSelector = 0x0048BDA0;
	static constexpr UInt32 kEquippableWeaponRatingSelectorPatchLength = 9;
	static constexpr UInt32 kMenuCreateTileFromTemplate = 0x00585410;
	static constexpr UInt32 kTileSetFloat = 0x0058CEB0;
	static constexpr UInt32 kTileSetString = 0x0058CED0;
	static constexpr UInt32 kTileGetFloat = 0x00588BD0;
	static constexpr UInt32 kTileAnimateTrait = 0x00589980;
	static constexpr UInt32 kTileGetParentMenu = 0x005898F0;
	static constexpr UInt32 kMenuGetOpenMenuTile = 0x00589B70;
	static constexpr UInt32 kTileGetGlobalX = 0x00588C50;
	static constexpr UInt32 kTileGetGlobalY = 0x00588CF0;
	static constexpr UInt32 kTileGetGlobalDepth = 0x00588D90;
	static constexpr UInt32 kActorGetGold = 0x005E4420;
	static constexpr UInt32 kActorValueGetName = 0x00565CC0;
	static constexpr UInt32 kActorValueGetIcon = 0x00565D10;
	static constexpr UInt32 kCalcMasteryFromSkill = 0x0056A300;
	static constexpr UInt32 kActorValueGetMasteryName = 0x0056A340;
	static constexpr UInt32 kActorGetBaseCalcAVi = 0x005F1910;
	static constexpr UInt32 kActorGetSkillMasteryLevel = 0x005F23B0;
	static constexpr UInt32 kOpenSkillPerkMenu = 0x0057B370;
	static constexpr UInt32 kPlayerMaybeStartNextAttributeBonusBucket = 0x0065FB30;
	static constexpr UInt32 kPlayerIncrementAttributeBonusBucket = 0x006648D0;
	static constexpr UInt32 kTESObjectREFRGetAnimData = 0x004D8370;
	static constexpr UInt32 kActorAnimDataRemovePowerAttackGroups = 0x00471990;
	static constexpr UInt32 kObservedActorAnimDataBuildPowerAttackKFList = 0x00476410;
	static constexpr UInt32 kTESObjectWEAPGetWeaponSkillAV = 0x004BB060;
	static constexpr UInt32 kCombatControllerWeaponSkillCall = 0x006137C9;
	static constexpr UInt32 kCombatControllerWeaponSkillContinue = 0x006137D5;
	static constexpr UInt32 kCombatSelectionHandToHandComparePatch = 0x00621F33;
	static constexpr UInt32 kCombatSelectionHandToHandPreferred = 0x00621F54;
	static constexpr UInt32 kCombatSelectionHandToHandCompareContinue = 0x00621F5E;
	static constexpr UInt32 kCalcWeaponDamage = 0x00547070;
	static constexpr UInt32 kCalcWeaponDamageContinue = kCalcWeaponDamage + 0x12;
	static constexpr UInt32 kCalcPowerAttackBonus = 0x00546BA0;
	static constexpr UInt32 kCalcPowerAttackBonusContinue = kCalcPowerAttackBonus + 0x15;
	static constexpr UInt32 kCalcLuckModifiedSkill = 0x00547B90;
	static constexpr UInt32 kMagicPopupWeaponLabelTrait = 0x00000FAE;
	static constexpr UInt32 kMagicPopupEnchantedWeaponLabelSetStringCall = 0x005B43D0;
	static constexpr UInt32 kMagicPopupEnchantedWeaponNullLabelSetStringCall = 0x005B43E5;
	static constexpr UInt32 kMagicPopupSimpleWeaponLabelSetStringCall = 0x005B55CA;
	static constexpr UInt32 kPowerAttackBuilderMasteryCall = 0x00476508;
	static constexpr UInt32 kWeaponDisarmBaseCalcCall = 0x005FC10D;
	static constexpr UInt32 kPostHitExpertMasteryCall = 0x006002F1;
	static constexpr UInt32 kPostHitMasterMasteryCall = 0x00600343;
	static constexpr UInt32 kPostHitExpertMasteryReturn = kPostHitExpertMasteryCall + 5;
	static constexpr UInt32 kTESAIFormOffersService = 0x00468240;
	static constexpr UInt32 kDialogueTrainingServiceOffersCall = 0x005E8A0C;
	static constexpr UInt32 kTrainingMenuOpenNative = 0x005DD4B0;
	static constexpr UInt32 kTrainingMenuOpenJump = 0x0057A99C;
	static constexpr UInt32 kTrainingMenuButton = 0x005DD3E0;
	static constexpr UInt32 kTrainingMenuClose = 0x005DD340;
	static constexpr UInt32 kTrainingMenuButtonPatchLength = 15;
	static constexpr UInt32 kRuntimeActorBaseAiFormOffset = 0x68;
	static constexpr UInt32 kTrainingServiceMask = TESAIForm::kService_Training;

	static constexpr UInt32 kRetWeaponSkillEquippedDamage = 0x00484FED;
	static constexpr UInt32 kRetWeaponSkillPlayerInventoryRating = 0x00489295;
	static constexpr UInt32 kRetWeaponSkillEquippableRatingA = 0x0048C115;
	static constexpr UInt32 kRetWeaponSkillEquippableRatingB = 0x0048C58A;
	static constexpr UInt32 kRetWeaponSkillActorBaseEquippableRating = 0x0051A1D6;
	static constexpr UInt32 kRetWeaponSkillPowerAttackBonus = 0x005FF4A2;
	static constexpr UInt32 kRetWeaponSkillProjectileDamage = 0x00612598;
	static constexpr UInt32 kRetEquippedWeaponDamage = 0x00485080;
	static constexpr UInt32 kRetPlayerInventoryWeaponRating = 0x00489382;
	static constexpr UInt32 kRetEquippableWeaponRating = 0x0048C7C0;
	static constexpr UInt32 kRetActorBaseEquippableWeaponRating = 0x0051A271;
	static constexpr UInt32 kRetPowerAttackBonus = 0x005FF4E6;
	static constexpr UInt32 kRetWeaponDamageWrapper = 0x005471CC;

	static constexpr UInt32 kTESFormFormIdOffset = 0x0C;
	static constexpr UInt32 kTrainingMenuTrainerOffset = 0x54;
	static constexpr UInt32 kTrainingMenuNativeSkillOffset = 0x58;
	static constexpr UInt32 kTrainingMenuCostOffset = 0x5C;
	static constexpr UInt32 kTrainingMenuTrainerLevelOffset = 0x60;
	static constexpr UInt32 kTrainingMenuSkillNameTileOffset = 0x2C;
	static constexpr UInt32 kTrainingMenuIconTileOffset = 0x30;
	static constexpr UInt32 kTrainingMenuAcceptTileOffset = 0x38;
	static constexpr UInt32 kTrainingMenuCostTileOffset = 0x40;
	static constexpr UInt32 kTrainingMenuDisabledReasonTileOffset = 0x48;
	static constexpr UInt32 kTrainingMenuTrainButton = 6;
	static constexpr UInt32 kTrainingMenuCloseButton = 7;
	static constexpr UInt32 kGoldFormId = 0x0000000F;
	static constexpr UInt32 kStatsMenuMasteryRankCount = 5;
	static constexpr UInt32 kGenericMenuArgInt = 0;
	static constexpr UInt32 kGenericMenuArgFloat = 1;
	static constexpr UInt32 kGenericMenuArgString = 2;
	static constexpr UInt32 kGenericMenuArgEnd = 3;
	static constexpr const char* kSkillPerkMenuXml = "skill_perk.xml";
	static constexpr const char* kSkillPerkOkText = "OK";

	static constexpr UInt32 kWeaponRatingNpcContextDepth = 8;

	static const UInt8 kPlayerModExperienceExpected[kPlayerModExperiencePatchLength] =
	{
		0x53, 0x8B, 0x5C, 0x24, 0x08, 0x56, 0x57
	};
	static const UInt8 kTESActorBaseGetEquippableItemRatingExpected[kTESActorBaseGetEquippableItemRatingPatchLength] =
	{
		0x83, 0xEC, 0x0C, 0xD9, 0xEE
	};
	static const UInt8 kEquippableWeaponRatingSelectorExpected[kEquippableWeaponRatingSelectorPatchLength] =
	{
		0x55, 0x8B, 0xEC,
		0x83, 0xE4, 0xF8,
		0x83, 0xEC, 0x54
	};
	static const UInt8 kTrainingMenuOpenJumpExpected[] =
	{
		0xE9, 0x0F, 0x2B, 0x06, 0x00
	};
	static const UInt8 kTrainingMenuButtonExpected[kTrainingMenuButtonPatchLength] =
	{
		0x68, 0x04, 0x04, 0x00, 0x00,
		0xE8, 0x86, 0xC7, 0xFA, 0xFF,
		0x83, 0xC4, 0x04,
		0x85, 0xC0
	};
	static const UInt8 kTESObjectWEAPGetWeaponSkillAVExpected[] =
	{
		0x0F, 0xBE, 0x81, 0x90, 0x00, 0x00, 0x00,
		0x8B, 0x04, 0x85, 0xA0, 0x86, 0xB0, 0x00,
		0xC3
	};
	static const UInt8 kCombatControllerWeaponSkillExpected[] =
	{
		0xE8, 0x92, 0x78, 0xEA, 0xFF,
		0x50,
		0x8B, 0x07,
		0x8B, 0xCB,
		0xFF, 0xD0
	};
	static const UInt8 kCombatSelectionHandToHandCompareExpected[] =
	{
		0x8B, 0x16,
		0x8B, 0x82, 0x84, 0x02, 0x00, 0x00,
		0x53,
		0x8B, 0xCE,
		0xFF, 0xD0,
		0x8B, 0x16,
		0x8B, 0xD8,
		0x8B, 0x82, 0x84, 0x02, 0x00, 0x00,
		0x6A, 0x11,
		0x8B, 0xCE,
		0xFF, 0xD0,
		0x3B, 0xC3,
		0x7E, 0x0A
	};
	static const UInt8 kCalcWeaponDamageExpected[] =
	{
		0x83, 0xEC, 0x08,
		0x8B, 0x44, 0x24, 0x10,
		0x8B, 0x4C, 0x24, 0x0C,
		0x50,
		0x51,
		0xE8, 0x0E, 0x0B, 0x00, 0x00
	};
	static const UInt8 kCalcPowerAttackBonusExpected[] =
	{
		0x51,
		0x8B, 0x44, 0x24, 0x08,
		0xD9, 0x05, 0x20, 0x6E, 0xB3, 0x00,
		0x50,
		0xD9, 0x5C, 0x24, 0x04,
		0xE8, 0x4B, 0x37, 0x02, 0x00
	};

	struct SkillDefinition
	{
		UInt32 skillId;
		const char* editorId;
		const char* name;
		UInt32 specialization;
		UInt32 governingAttributeAV;
		UInt32 fallbackActorValue;
		const char* iconPath;
		const char* rowIconPath;
		const char* fallbackIconPath;
		const char* fallbackRowIconPath;
		const char* description;
		const char* classPickerDescription;
	};

	struct SkillState
	{
		UInt32 level;
		float progress;
		float requiredProgress;
		UInt32 levelUps;
		UInt32 governingAttributeIncreaseCount;
		UInt8 major;
		UInt8 padding[3];
	};

	struct SkillUseCondition
	{
		SpearSkillShared::WeaponSkillKind kind;
		UInt32 nativeActorValue;
		UInt32 useType;
	};

	struct SavedWeaponTypeEntry
	{
		UInt32 formId;
		UInt32 kind;
	};

	struct PendingWeaponSkillConsumer
	{
		bool active;
		bool playerFacing;
		bool baseNpcFacing;
		UInt32 sourceReturnAddress;
		TESObjectWEAP* weapon;
		TESNPC* npcContext;
		SpearSkillShared::WeaponSkillKind kind;
	};

	static const SkillDefinition kSkills[kSkillCount] =
	{
		{ SpearSkillShared::kSpearSkillId, "Spear", kSpearSkillDisplayName, TESClass::eSpec_Combat, kActorVal_Endurance, kActorVal_Blade, kSpearSkillIconPath, kSpearSkillRowIconPath, kVanillaBladeSkillIconPath, kVanillaBladeSkillSmallIconPath, kSpearSkillDescription, kSpearClassPickerDescription },
	};

	static const SkillUseCondition kSkillUseConditions[kSkillCount] =
	{
		{ SpearSkillShared::kWeaponSkill_Spear, kActorVal_Blade, kWeaponSuccessfulHitUseType },
	};

	static SpearSkillShared::WeaponTypeSidecarStore g_weaponTypeStore;
	static SpearSkillShared::NpcSpearStore g_npcSkillStore;
	static SpearSkillShared::NpcSpearTrainingStore g_npcTrainingStore;
	static bool g_npcSkillStoreConfigured = false;
	static bool g_statsOrderingDirty = false;
	static bool g_loggedLegacySelfWeaponTypeFallback = false;
	static bool g_loggedWeaponTypeCarrierStoreFull = false;
	static bool g_loggedLegacySelfNpcSkillFallback = false;
	static bool g_loggedNpcSkillCarrierStoreFull = false;
	static bool g_loggedNpcTrainingCarrierStoreFull = false;
	static UInt32 g_appliedPatches = 0;
	static UInt32 g_failedPatches = 0;
	static PendingWeaponSkillConsumer g_pendingWeaponSkillConsumer = {};
	static TESNPC* g_weaponRatingNpcContext[kWeaponRatingNpcContextDepth] = {};
	static UInt32 g_weaponRatingNpcContextDepth = 0;
	static UInt32 g_weaponRatingNpcContextOverflowDepth = 0;
	static void* g_playerModExperienceOriginal = nullptr;
	static void* g_actorBaseGetEquippableItemRatingOriginal = nullptr;
	static void* g_equippableWeaponRatingSelectorOriginal = nullptr;
	static void* g_getWeaponSkillAVOriginal = nullptr;
	static void* g_calcWeaponDamageOriginal = nullptr;
	static void* g_calcPowerAttackBonusOriginal = nullptr;
	static void* g_trainingMenuOpenOriginal = nullptr;
	static void* g_trainingMenuButtonOriginal = nullptr;
	static UInt32 g_weaponPerkMasteryOriginalTarget = kActorGetSkillMasteryLevel;
	static UInt32 g_weaponPerkBaseCalcOriginalTarget = kActorGetBaseCalcAVi;
	static UInt32 g_dialogueTrainingOffersServiceOriginalTarget = kTESAIFormOffersService;
	static UInt32 g_magicPopupEbpLabelOriginalTarget = kTileSetString;
	static UInt32 g_magicPopupEdiLabelOriginalTarget = kTileSetString;
	static UInt32 g_combatControllerWeaponSkillChainTarget = 0;
	static UInt32 g_combatSelectionHandToHandChainTarget = 0;
	static bool g_hooksInstalled = false;
	static bool g_hookInstallAttempted = false;

	using PlayerModExperienceFn = void(__thiscall*)(PlayerCharacter* player, UInt32 actorValue, UInt32 useType, float baseDelta);
	using TESActorBaseGetEquippableItemRatingFn = double(__thiscall*)(TESForm* actorBase, TESForm* item);
	using MenuCreateTileFromTemplateFn = Tile * (__thiscall*)(void* menu, Tile* parent, const char* templateName, UInt32 unk);
	using TileSetFloatFn = void(__thiscall*)(Tile* tile, UInt32 trait, float value);
	using TileSetStringFn = void(__thiscall*)(Tile* tile, UInt32 trait, const char* value);
	using TileGetFloatFn = double(__thiscall*)(Tile* tile, UInt32 trait);
	using TileAnimateTraitFn = void(__thiscall*)(Tile* tile, UInt32 trait, float fromValue, float toValue, float duration);
	using TileGetParentMenuFn = void* (__thiscall*)(Tile* tile);
	using MenuGetOpenMenuTileFn = Tile * (__cdecl*)(UInt32 menuType);
	using TileGetGlobalValueFn = double(__thiscall*)(Tile* tile);
	using ActorGetGoldFn = int(__thiscall*)(Actor* actor);
	using ActorValueGetNameFn = const char* (__cdecl*)(UInt32 actorValue);
	using ActorValueGetIconFn = const char* (__cdecl*)(UInt32 actorValue);
	using CalcMasteryFromSkillFn = UInt32(__cdecl*)(SInt32 skillLevel);
	using ActorValueGetMasteryNameFn = const char* (__cdecl*)(UInt32 masteryLevel);
	using ActorGetBaseCalcAViFn = UInt32(__thiscall*)(Actor* actor, UInt32 actorValue);
	using ActorGetSkillMasteryLevelFn = UInt32(__thiscall*)(Actor* actor, UInt32 actorValue);
	using OpenSkillPerkMenuFn = char(__cdecl*)(const char* xml, UInt32 unk1, UInt32 unk2, UInt32 unk3, UInt32 firstArgType, ...);
	using TESObjectREFRGetAnimDataFn = ActorAnimData * (__thiscall*)(TESObjectREFR* refr);
	using ActorAnimDataRemovePowerAttackGroupsFn = void(__thiscall*)(ActorAnimData* animData);
	using ObservedActorAnimDataBuildPowerAttackKFListFn = void(__thiscall*)(ActorAnimData* animData, TESObjectREFR* refr, UInt32 unk);
	using GetWeaponSkillAVFn = UInt32(__thiscall*)(TESObjectWEAP* weapon);
	using CalcWeaponDamageFn = double(__cdecl*)(int weaponSkill, int luck, int strengthOrAgility, float fatigue, int weaponDamage, float condition, float multiplier, float ignoreFatigue);
	using CalcPowerAttackBonusFn = double(__cdecl*)(int skillLevel, int attackType);
	using TESAIFormOffersServiceFn = bool(__thiscall*)(void* aiForm, UInt32 serviceMask);
	using TrainingMenuCloseFn = void(__cdecl*)();
	using PlayerMaybeStartNextAttributeBonusBucketFn = void(__thiscall*)(PlayerCharacter* player);
	using PlayerIncrementAttributeBonusBucketFn = UInt32(__thiscall*)(PlayerCharacter* player, UInt32 attributeAV);

	static PlayerCharacter* GetPlayer()
	{
		return g_thePlayer ? *g_thePlayer : nullptr;
	}

	static PlayerModExperienceFn PlayerModExperienceOriginal()
	{
		return reinterpret_cast<PlayerModExperienceFn>(g_playerModExperienceOriginal);
	}

	static TESActorBaseGetEquippableItemRatingFn TESActorBaseGetEquippableItemRatingOriginal()
	{
		return reinterpret_cast<TESActorBaseGetEquippableItemRatingFn>(g_actorBaseGetEquippableItemRatingOriginal);
	}

	static TileSetFloatFn TileSetFloat()
	{
		return reinterpret_cast<TileSetFloatFn>(kTileSetFloat);
	}

	static TileSetStringFn TileSetString()
	{
		return reinterpret_cast<TileSetStringFn>(kTileSetString);
	}

	static TileGetFloatFn TileGetFloat()
	{
		return reinterpret_cast<TileGetFloatFn>(kTileGetFloat);
	}

	static TileAnimateTraitFn TileAnimateTrait()
	{
		return reinterpret_cast<TileAnimateTraitFn>(kTileAnimateTrait);
	}

	static TileGetParentMenuFn TileGetParentMenu()
	{
		return reinterpret_cast<TileGetParentMenuFn>(kTileGetParentMenu);
	}

	static MenuGetOpenMenuTileFn MenuGetOpenMenuTile()
	{
		return reinterpret_cast<MenuGetOpenMenuTileFn>(kMenuGetOpenMenuTile);
	}

	static TileGetGlobalValueFn TileGetGlobalX()
	{
		return reinterpret_cast<TileGetGlobalValueFn>(kTileGetGlobalX);
	}

	static TileGetGlobalValueFn TileGetGlobalY()
	{
		return reinterpret_cast<TileGetGlobalValueFn>(kTileGetGlobalY);
	}

	static TileGetGlobalValueFn TileGetGlobalDepth()
	{
		return reinterpret_cast<TileGetGlobalValueFn>(kTileGetGlobalDepth);
	}

	static ActorValueGetNameFn ActorValueGetName()
	{
		return reinterpret_cast<ActorValueGetNameFn>(kActorValueGetName);
	}

	static CalcMasteryFromSkillFn CalcMasteryFromSkill()
	{
		return reinterpret_cast<CalcMasteryFromSkillFn>(kCalcMasteryFromSkill);
	}

	static ActorValueGetMasteryNameFn ActorValueGetMasteryName()
	{
		return reinterpret_cast<ActorValueGetMasteryNameFn>(kActorValueGetMasteryName);
	}

	static ActorGetBaseCalcAViFn ActorGetBaseCalcAVi()
	{
		return reinterpret_cast<ActorGetBaseCalcAViFn>(kActorGetBaseCalcAVi);
	}

	static ActorGetSkillMasteryLevelFn ActorGetSkillMasteryLevel()
	{
		return reinterpret_cast<ActorGetSkillMasteryLevelFn>(kActorGetSkillMasteryLevel);
	}

	static ActorGetBaseCalcAViFn WeaponPerkBaseCalcOriginal()
	{
		return reinterpret_cast<ActorGetBaseCalcAViFn>(g_weaponPerkBaseCalcOriginalTarget);
	}

	static ActorGetSkillMasteryLevelFn WeaponPerkMasteryOriginal()
	{
		return reinterpret_cast<ActorGetSkillMasteryLevelFn>(g_weaponPerkMasteryOriginalTarget);
	}

	static TESObjectREFRGetAnimDataFn TESObjectREFRGetAnimData()
	{
		return reinterpret_cast<TESObjectREFRGetAnimDataFn>(kTESObjectREFRGetAnimData);
	}

	static ActorAnimDataRemovePowerAttackGroupsFn ActorAnimDataRemovePowerAttackGroups()
	{
		return reinterpret_cast<ActorAnimDataRemovePowerAttackGroupsFn>(kActorAnimDataRemovePowerAttackGroups);
	}

	static ObservedActorAnimDataBuildPowerAttackKFListFn ObservedActorAnimDataBuildPowerAttackKFList()
	{
		return reinterpret_cast<ObservedActorAnimDataBuildPowerAttackKFListFn>(kObservedActorAnimDataBuildPowerAttackKFList);
	}

	static GetWeaponSkillAVFn GetWeaponSkillAVOriginal()
	{
		return reinterpret_cast<GetWeaponSkillAVFn>(g_getWeaponSkillAVOriginal);
	}

	static CalcWeaponDamageFn CalcWeaponDamageOriginal()
	{
		return reinterpret_cast<CalcWeaponDamageFn>(g_calcWeaponDamageOriginal);
	}

	static CalcPowerAttackBonusFn CalcPowerAttackBonusOriginal()
	{
		return reinterpret_cast<CalcPowerAttackBonusFn>(g_calcPowerAttackBonusOriginal);
	}

	static UInt32 GetSkillIndexById(UInt32 skillId)
	{
		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (kSkills[i].skillId == skillId)
				return i;
		}

		return 0xFFFFFFFF;
	}

	static UInt32 GetSkillIndexForKind(SpearSkillShared::WeaponSkillKind kind)
	{
		return GetSkillIndexById(SpearSkillShared::SkillIdForKind(kind));
	}

	static const char* GetWeaponSkillDisplayName(SpearSkillShared::WeaponSkillKind kind)
	{
		return kind == SpearSkillShared::kWeaponSkill_Spear ? kSpearSkillDisplayName : nullptr;
	}

	static bool AddUniqueUInt32(UInt32* values, UInt32& count, UInt32 maxCount, UInt32 value)
	{
		for (UInt32 i = 0; i < count; ++i)
		{
			if (values[i] == value)
				return false;
		}

		if (count >= maxCount)
			return false;

		values[count++] = value;
		return true;
	}

	static bool IsNativeSkillActorValue(UInt32 actorValue)
	{
		return actorValue >= kFirstNativeSkillAV && actorValue <= kLastNativeSkillAV;
	}

	static bool IsVisibleNativeSkillActorValue(UInt32 actorValue)
	{
		return IsNativeSkillActorValue(actorValue);
	}

	static Tile* GetMenuTileAtOffset(void* menu, UInt32 offset)
	{
		return menu ? *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(menu) + offset) : nullptr;
	}

	static float GetTileFloat(Tile* tile, UInt32 trait)
	{
		return tile ? static_cast<float>(TileGetFloat()(tile, trait)) : 0.0f;
	}

	static void SetTileFloat(Tile* tile, UInt32 trait, float value)
	{
		if (tile)
			TileSetFloat()(tile, trait, value);
	}

	static void SetTileString(Tile* tile, UInt32 trait, const char* value)
	{
		if (tile)
			TileSetString()(tile, trait, value ? value : "");
	}

	static const char* GetSafeActorValueName(UInt32 actorValue)
	{
		if (const char* name = ActorValueGetName()(actorValue))
			return name;

		return "";
	}

	static const char* GetSafeMasteryName(UInt32 level)
	{
		if (const char* name = ActorValueGetMasteryName()(CalcMasteryFromSkill()(static_cast<SInt32>(level))))
			return name;

		return "";
	}

	static void SetSpearProgress(float progress);
	static void ModSpearSkill(SInt32 delta);

	static bool AddSkillProgress(UInt32 index, float progressDelta)
	{
		if (index >= kSkillCount || !std::isfinite(progressDelta))
			return false;

		if (!g_tcs || !g_tcs->AddSkillXP)
			return false;

		return g_tcs->AddSkillXP(SpearSkillShared::kSpearSkillName, progressDelta);
	}

	static bool IsWeaponForm(const TESForm* form)
	{
		return form && form->typeID == kFormType_Weapon;
	}

	static bool IsNpcForm(const TESForm* form)
	{
		return form && form->typeID == kFormType_NPC;
	}

	static TESNPC* AsNpcForm(TESForm* form)
	{
		return IsNpcForm(form) ? reinterpret_cast<TESNPC*>(form) : nullptr;
	}

	static bool IsPlayerBaseForm(TESForm* form)
	{
		PlayerCharacter* player = GetPlayer();
		return form && player && player->baseForm == form;
	}

	static TESNPC* AsSidecarNpcOwner(TESForm* form)
	{
		if (IsPlayerBaseForm(form))
			return nullptr;

		return AsNpcForm(form);
	}

	static TESObjectWEAP* GetPlayerEquippedWeapon()
	{
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return nullptr;

		EquippedItemsList equippedItems = player->GetEquippedItems();
		for (EquippedItemsList::const_iterator it = equippedItems.begin(); it != equippedItems.end(); ++it)
		{
			TESForm* form = *it;
			if (IsWeaponForm(form))
				return static_cast<TESObjectWEAP*>(form);
		}

		return nullptr;
	}

	static const char* GetWeaponDisplayName(TESObjectWEAP* weapon)
	{
		if (!weapon || !weapon->fullName.name.m_data || !weapon->fullName.name.m_dataLen)
			return "";

		return weapon->fullName.name.m_data;
	}

	static TESObjectWEAP* FindWeaponByEditorID(const char* editorId)
	{
		if (!editorId || !editorId[0] || !g_dataHandler || !*g_dataHandler || !(*g_dataHandler)->boundObjects)
			return nullptr;

		TESObjectWEAP* match = nullptr;
		for (TESBoundObject* object = (*g_dataHandler)->boundObjects->first; object; object = object->next)
		{
			TESForm* form = reinterpret_cast<TESForm*>(object);
			if (!IsWeaponForm(form))
				continue;

			TESObjectWEAP* weapon = reinterpret_cast<TESObjectWEAP*>(object);
			const char* candidateEditorId = weapon->GetEditorID();
			if (!candidateEditorId || _stricmp(candidateEditorId, editorId))
				continue;

			if (match && match->refID != weapon->refID)
				return nullptr;

			match = weapon;
		}

		return match;
	}

	static void WeaponTypeStoreLog(void*, const char* message)
	{
		_MESSAGE("SpearSkill: %s", message ? message : "");
	}

	static TESForm* FindNpcByEditorID(const char* editorId)
	{
		if (!editorId || !editorId[0] || !g_dataHandler || !*g_dataHandler || !(*g_dataHandler)->boundObjects)
			return nullptr;

		TESForm* match = nullptr;
		for (TESBoundObject* object = (*g_dataHandler)->boundObjects->first; object; object = object->next)
		{
			TESForm* form = reinterpret_cast<TESForm*>(object);
			if (!IsNpcForm(form))
				continue;

			const char* candidateEditorId = form->GetEditorID();
			if (!candidateEditorId || _stricmp(candidateEditorId, editorId))
				continue;

			if (match && match->refID != form->refID)
				return nullptr;

			match = form;
		}

		return match;
	}

	static void NpcSkillStoreLog(void*, const char* message)
	{
		_MESSAGE("SpearSkill: %s", message ? message : "");
	}

	static void EnsureNpcSpearStoreConfigured()
	{
		if (g_npcSkillStoreConfigured)
			return;

		g_npcSkillStore.Configure(nullptr, NpcSkillStoreLog, nullptr);
		g_npcSkillStoreConfigured = true;
	}

	static void ClearWeaponRatingNpcContext()
	{
		std::memset(g_weaponRatingNpcContext, 0, sizeof(g_weaponRatingNpcContext));
		g_weaponRatingNpcContextDepth = 0;
		g_weaponRatingNpcContextOverflowDepth = 0;
	}

	static void PushWeaponRatingNpcContext(TESNPC* npc)
	{
		if (g_weaponRatingNpcContextDepth >= kWeaponRatingNpcContextDepth)
		{
			++g_weaponRatingNpcContextOverflowDepth;
			return;
		}

		g_weaponRatingNpcContext[g_weaponRatingNpcContextDepth++] = npc;
	}

	static void PopWeaponRatingNpcContext()
	{
		if (g_weaponRatingNpcContextOverflowDepth)
		{
			--g_weaponRatingNpcContextOverflowDepth;
			return;
		}

		if (!g_weaponRatingNpcContextDepth)
			return;

		--g_weaponRatingNpcContextDepth;
		g_weaponRatingNpcContext[g_weaponRatingNpcContextDepth] = nullptr;
	}

	static TESNPC* GetCurrentWeaponRatingNpcContext()
	{
		return g_weaponRatingNpcContextDepth ?
			g_weaponRatingNpcContext[g_weaponRatingNpcContextDepth - 1] :
			nullptr;
	}

	static void __cdecl PushBaseWeaponRatingContext(TESForm* actorBase)
	{
		PushWeaponRatingNpcContext(AsSidecarNpcOwner(actorBase));
	}

	static void __cdecl PopWeaponRatingContext()
	{
		PopWeaponRatingNpcContext();
	}

	static bool TryGetNpcSpearSkill(const TESNPC* npc, UInt32* outLevel)
	{
		if (outLevel)
			*outLevel = 0;
		if (!npc)
			return false;

		EnsureNpcSpearStoreConfigured();
		SpearSkillShared::NpcSpearEntry entry = {};
		if (!g_npcSkillStore.TryGet(npc->refID, &entry))
			return false;

		if (outLevel)
			*outLevel = SpearSkillShared::NpcSpearStore::ClampLevel(entry.level);
		return true;
	}

	static bool ResolveWeaponTypeKeyToRuntimeFormID(const SpearSkillShared::WeaponTypeSidecarKey& key, const char* carrierModName, UInt32* outFormId)
	{
		if (outFormId)
			*outFormId = 0;

		const char* sourceMod = key.sourceMod;
		if (!_stricmp(sourceMod, "$SELF"))
			sourceMod = carrierModName;
		if (!sourceMod || !sourceMod[0] || !g_dataHandler || !*g_dataHandler)
			return false;

		const UInt8 modIndex = (*g_dataHandler)->GetModIndex(sourceMod);
		if (modIndex == 0xFF)
			return false;

		const UInt32 formId = (static_cast<UInt32>(modIndex) << 24) | (key.objectId & 0x00FFFFFF);
		TESForm* form = LookupFormByID(formId);
		if (IsWeaponForm(form))
		{
			if (outFormId)
				*outFormId = formId;
			return true;
		}

		if (!_stricmp(key.sourceMod, "$SELF") && key.editorId[0])
		{
			TESObjectWEAP* fallbackWeapon = FindWeaponByEditorID(key.editorId);
			if (fallbackWeapon)
			{
				if (outFormId)
					*outFormId = fallbackWeapon->refID;
				if (!g_loggedLegacySelfWeaponTypeFallback)
				{
					_MESSAGE("SpearSkill: resolved legacy $SELF weapon Type sidecar row by editor ID %s", key.editorId);
					g_loggedLegacySelfWeaponTypeFallback = true;
				}
				return true;
			}
		}

		if (outFormId)
			*outFormId = 0;
		return false;
	}

	static void LoadEditorWeaponTypeSidecars(bool preserveExistingIfNoCarriers)
	{
		if (!g_dataHandler || !*g_dataHandler || !(*g_dataHandler)->boundObjects)
		{
			if (!preserveExistingIfNoCarriers)
				g_weaponTypeStore.Clear();
			return;
		}

		SpearSkillShared::WeaponTypeSidecarStore importedStore;
		SpearSkillShared::WeaponTypeSidecarPayloadStats totals = {};
		const UInt8 activeModCount = (*g_dataHandler)->GetActiveModCount();
		for (UInt32 modIndex = 0; modIndex < activeModCount; ++modIndex)
		{
			const char* carrierModName = (*g_dataHandler)->GetNthModName(modIndex);
			for (TESBoundObject* object = (*g_dataHandler)->boundObjects->first; object; object = object->next)
			{
				if (object->typeID != kFormType_Book || (object->refID >> 24) != modIndex)
					continue;

				TESObjectBOOK* book = reinterpret_cast<TESObjectBOOK*>(object);
				const char* payload = book->description.GetDescription();
				if (!SpearSkillShared::WeaponTypeSidecarPayloadCodec::LooksLikePayload(payload))
					continue;

				++totals.carrierRecords;
				std::vector<SpearSkillShared::WeaponTypeSidecarPayloadRow> rows;
				SpearSkillShared::WeaponTypeSidecarPayloadStats stats = {};
				if (!SpearSkillShared::WeaponTypeSidecarPayloadCodec::Parse(payload, rows, &stats, WeaponTypeStoreLog, nullptr))
				{
					totals.skippedRows += stats.skippedRows;
					continue;
				}

				totals.parsedEntries += stats.parsedEntries;
				totals.skippedRows += stats.skippedRows;
				for (size_t i = 0; i < rows.size(); ++i)
				{
					UInt32 resolvedFormId = 0;
					if (!ResolveWeaponTypeKeyToRuntimeFormID(rows[i].key, carrierModName, &resolvedFormId))
					{
						++totals.unresolvedEntries;
						continue;
					}

					if (importedStore.SetLoaded(resolvedFormId, rows[i].kind))
					{
						++totals.resolvedEntries;
					}
					else
					{
						++totals.unresolvedEntries;
						if (!g_loggedWeaponTypeCarrierStoreFull)
						{
							_WARNING("SpearSkill: embedded weapon Type carrier row could not be stored form=%08X kind=%u", resolvedFormId, rows[i].kind);
							g_loggedWeaponTypeCarrierStoreFull = true;
						}
					}
				}
			}
		}

		if (totals.carrierRecords || !preserveExistingIfNoCarriers)
		{
			g_weaponTypeStore.Clear();
			for (UInt32 i = 0; i < importedStore.Count(); ++i)
			{
				const SpearSkillShared::WeaponTypeSidecarEntry& entry = importedStore.EntryAt(i);
				g_weaponTypeStore.SetLoaded(entry.formId, entry.kind);
			}
		}

		g_weaponTypeStore.ClearDirty();
		_MESSAGE("SpearSkill: embedded weapon Type carriers=%u parsed=%u resolved=%u unresolved=%u skipped=%u",
			totals.carrierRecords,
			totals.parsedEntries,
			totals.resolvedEntries,
			totals.unresolvedEntries,
			totals.skippedRows);
	}

	static bool ResolveNpcSpearKeyToRuntimeFormID(const SpearSkillShared::NpcSpearKey& key, const char* carrierModName, UInt32* outFormId)
	{
		if (outFormId)
			*outFormId = 0;

		const char* sourceMod = key.sourceMod;
		if (!_stricmp(sourceMod, "$SELF"))
			sourceMod = carrierModName;
		if (!sourceMod || !sourceMod[0] || !g_dataHandler || !*g_dataHandler)
			return false;

		const UInt8 modIndex = (*g_dataHandler)->GetModIndex(sourceMod);
		if (modIndex == 0xFF)
			return false;

		const UInt32 formId = (static_cast<UInt32>(modIndex) << 24) | (key.objectId & 0x00FFFFFF);
		TESForm* form = LookupFormByID(formId);
		if (IsNpcForm(form))
		{
			if (outFormId)
				*outFormId = formId;
			return true;
		}

		if (!_stricmp(key.sourceMod, "$SELF") && key.editorId[0])
		{
			TESForm* fallbackNpc = FindNpcByEditorID(key.editorId);
			if (fallbackNpc)
			{
				if (outFormId)
					*outFormId = fallbackNpc->refID;
				if (!g_loggedLegacySelfNpcSkillFallback)
				{
					_MESSAGE("SpearSkill: resolved legacy $SELF NPC Spear sidecar row by editor ID %s", key.editorId);
					g_loggedLegacySelfNpcSkillFallback = true;
				}
				return true;
			}
		}

		if (outFormId)
			*outFormId = 0;
		return false;
	}

	static void LoadEditorNpcSpearSidecars(bool preserveExistingIfNoCarriers)
	{
		EnsureNpcSpearStoreConfigured();
		if (!g_dataHandler || !*g_dataHandler || !(*g_dataHandler)->boundObjects)
		{
			if (!preserveExistingIfNoCarriers)
				g_npcSkillStore.Clear();
			return;
		}

		SpearSkillShared::NpcSpearStore importedStore;
		importedStore.Configure(nullptr, NpcSkillStoreLog, nullptr);
		SpearSkillShared::NpcSpearPayloadStats totals = {};
		const UInt8 activeModCount = (*g_dataHandler)->GetActiveModCount();
		for (UInt32 modIndex = 0; modIndex < activeModCount; ++modIndex)
		{
			const char* carrierModName = (*g_dataHandler)->GetNthModName(modIndex);
			for (TESBoundObject* object = (*g_dataHandler)->boundObjects->first; object; object = object->next)
			{
				if (object->typeID != kFormType_Book || (object->refID >> 24) != modIndex)
					continue;

				TESObjectBOOK* book = reinterpret_cast<TESObjectBOOK*>(object);
				const char* payload = book->description.GetDescription();
				if (!SpearSkillShared::NpcSpearPayloadCodec::LooksLikePayload(payload))
					continue;

				++totals.carrierRecords;
				std::vector<SpearSkillShared::NpcSpearPayloadRow> rows;
				SpearSkillShared::NpcSpearPayloadStats stats = {};
				if (!SpearSkillShared::NpcSpearPayloadCodec::Parse(payload, rows, &stats, NpcSkillStoreLog, nullptr))
				{
					totals.skippedRows += stats.skippedRows;
					continue;
				}

				totals.parsedEntries += stats.parsedEntries;
				totals.skippedRows += stats.skippedRows;
				for (size_t i = 0; i < rows.size(); ++i)
				{
					UInt32 resolvedFormId = 0;
					if (!ResolveNpcSpearKeyToRuntimeFormID(rows[i].key, carrierModName, &resolvedFormId))
					{
						++totals.unresolvedEntries;
						continue;
					}

					SpearSkillShared::NpcSpearEntry entry = {};
					entry.formId = resolvedFormId;
					entry.level = rows[i].level;
					entry.progress = rows[i].progress;
					entry.levelUps = rows[i].levelUps;
					if (importedStore.SetLoaded(resolvedFormId, entry.level, entry.progress, entry.levelUps))
					{
						++totals.resolvedEntries;
					}
					else
					{
						++totals.unresolvedEntries;
						if (!g_loggedNpcSkillCarrierStoreFull)
						{
							_WARNING("SpearSkill: embedded NPC Spear carrier row could not be stored form=%08X", resolvedFormId);
							g_loggedNpcSkillCarrierStoreFull = true;
						}
					}
				}
			}
		}

		if (totals.carrierRecords || !preserveExistingIfNoCarriers)
		{
			g_npcSkillStore.Clear();
			for (UInt32 i = 0; i < importedStore.Count(); ++i)
			{
				const SpearSkillShared::NpcSpearEntry& entry = importedStore.EntryAt(i);
				g_npcSkillStore.SetLoaded(entry.formId, entry.level, entry.progress, entry.levelUps);
			}
		}

		g_npcSkillStore.ClearDirty();
		_MESSAGE("SpearSkill: embedded NPC Spear carriers=%u parsed=%u resolved=%u unresolved=%u skipped=%u",
			totals.carrierRecords,
			totals.parsedEntries,
			totals.resolvedEntries,
			totals.unresolvedEntries,
			totals.skippedRows);
	}

	static bool TryGetAuthoredWeaponType(TESObjectWEAP* weapon, SpearSkillShared::WeaponSkillKind* outKind)
	{
		if (outKind)
			*outKind = SpearSkillShared::kWeaponSkill_None;
		if (!weapon || !weapon->refID)
			return false;

		SpearSkillShared::WeaponSkillKind kind = SpearSkillShared::kWeaponSkill_None;
		if (!g_weaponTypeStore.TryGet(weapon->refID, &kind) || kind == SpearSkillShared::kWeaponSkill_None)
			return false;

		if (outKind)
			*outKind = kind;
		return true;
	}

	static SpearSkillShared::WeaponSkillKind ClassifySidecarWeapon(TESObjectWEAP* weapon)
	{
		if (!weapon)
			return SpearSkillShared::kWeaponSkill_None;

		SpearSkillShared::WeaponSkillKind authoredKind = SpearSkillShared::kWeaponSkill_None;
		if (TryGetAuthoredWeaponType(weapon, &authoredKind))
			return authoredKind;

		return SpearSkillShared::ClassifyWeapon(weapon->type, weapon->GetEditorID(), GetWeaponDisplayName(weapon));
	}

	static UInt32 GetPlayerWeaponSidecarIndexForActorValueContext(Actor* actor, UInt32 actorValue)
	{
		PlayerCharacter* player = GetPlayer();
		if (!player || actor != static_cast<Actor*>(player))
			return 0xFFFFFFFF;

		const SpearSkillShared::WeaponSkillKind kind = ClassifySidecarWeapon(GetPlayerEquippedWeapon());
		const UInt32 index = GetSkillIndexForKind(kind);
		return index < kSkillCount && kSkills[index].fallbackActorValue == actorValue ? index : 0xFFFFFFFF;
	}

	static bool TryGetPlayerWeaponSidecarLevel(Actor* actor, UInt32 actorValue, UInt32* outIndex, UInt32* outLevel)
	{
		if (outIndex)
			*outIndex = 0xFFFFFFFF;
		if (outLevel)
			*outLevel = 0;

		const UInt32 index = GetPlayerWeaponSidecarIndexForActorValueContext(actor, actorValue);
		if (index >= kSkillCount)
			return false;

		if (!g_tcs || !g_tcs->GetSkillLevel)
			return false;

		if (outIndex)
			*outIndex = index;
		if (outLevel)
			*outLevel = g_tcs->GetSkillLevel(SpearSkillShared::kSpearSkillName);
		return true;
	}

	static UInt32 NativeWeaponSkillAV(TESObjectWEAP* weapon);

	static bool TryGetPlayerWeaponSidecarLevelForWeapon(Actor* actor, TESObjectWEAP* weapon, UInt32 actorValue, UInt32* outLevel)
	{
		if (outLevel)
			*outLevel = 0;

		PlayerCharacter* player = GetPlayer();
		if (!player || actor != static_cast<Actor*>(player) || !weapon || GetPlayerEquippedWeapon() != weapon)
			return false;

		const SpearSkillShared::WeaponSkillKind kind = ClassifySidecarWeapon(weapon);
		const UInt32 index = GetSkillIndexForKind(kind);
		if (index >= kSkillCount || kSkills[index].fallbackActorValue != actorValue)
			return false;

		if (!g_tcs || !g_tcs->GetSkillLevel)
			return false;

		if (outLevel)
			*outLevel = g_tcs->GetSkillLevel(SpearSkillShared::kSpearSkillName);
		return true;
	}

	static UInt32 GetCurrentActorValue(Actor* actor, UInt32 actorValue)
	{
		if (!actor)
			return 0;

		return actor->GetActorValue(actorValue);
	}

	static constexpr UInt32 kNotOurWeaponSkill = 0xFFFFFFFF;

	static UInt32 __cdecl TryGetOwnCombatScoringWeaponSkillLevel(Actor* actor, TESObjectWEAP* weapon)
	{
		const UInt32 nativeActorValue = NativeWeaponSkillAV(weapon);
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevelForWeapon(actor, weapon, nativeActorValue, &sidecarLevel))
			return sidecarLevel;

		return kNotOurWeaponSkill;
	}

	static UInt32 __cdecl GetNativeCombatScoringWeaponSkillLevel(Actor* actor, TESObjectWEAP* weapon)
	{
		return GetCurrentActorValue(actor, NativeWeaponSkillAV(weapon));
	}

	static UInt32 __cdecl TryGetOwnCombatSelectionActorValueSkill(Actor* actor, UInt32 actorValue)
	{
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevel(actor, actorValue, nullptr, &sidecarLevel))
			return sidecarLevel;

		return kNotOurWeaponSkill;
	}

	static __declspec(naked) void HookCombatControllerWeaponSkillLevel()
	{
		__asm
		{
			push ecx
			push ecx
			push ebx
			call TryGetOwnCombatScoringWeaponSkillLevel
			add esp, 8
			cmp eax, 0FFFFFFFFh
			je notMine
			add esp, 4
			mov edx, kCombatControllerWeaponSkillContinue
			jmp edx
			notMine :
			mov edx, dword ptr[g_combatControllerWeaponSkillChainTarget]
				test edx, edx
				jz noChain
				pop ecx
				jmp edx
				noChain :
			pop ecx
				push ecx
				push ebx
				call GetNativeCombatScoringWeaponSkillLevel
				add esp, 8
				mov edx, kCombatControllerWeaponSkillContinue
				jmp edx
		}
	}

	static __declspec(naked) void HookCombatSelectionHandToHandSkillCompare()
	{
		__asm
		{
			push ebx
			push esi
			call TryGetOwnCombatSelectionActorValueSkill
			add esp, 8
			cmp eax, 0FFFFFFFFh
			je notMine
			mov ebx, eax
			jmp doCompare
			notMine :
			mov edx, dword ptr[g_combatSelectionHandToHandChainTarget]
				test edx, edx
				jnz haveChain
				push ebx
				push esi
				call GetCurrentActorValue
				add esp, 8
				mov ebx, eax
				jmp doCompare
				haveChain :
			jmp edx
				doCompare :
			mov edx, [esi]
				mov eax, [edx + 284h]
				push 11h
				mov ecx, esi
				call eax
				cmp eax, ebx
				jle keepCandidate
				mov edx, kCombatSelectionHandToHandPreferred
				jmp edx
				keepCandidate :
			mov edx, kCombatSelectionHandToHandCompareContinue
				jmp edx
		}
	}

	static UInt32 __fastcall HookActorGetBaseCalcAViForWeaponSidecarPerk(Actor* actor, void*, UInt32 actorValue)
	{
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevel(actor, actorValue, nullptr, &sidecarLevel))
			return sidecarLevel;

		return WeaponPerkBaseCalcOriginal()(actor, actorValue);
	}

	static UInt32 __fastcall HookActorGetSkillMasteryLevelForWeaponSidecarPerk(Actor* actor, void*, UInt32 actorValue)
	{
		UInt32 sidecarIndex = 0xFFFFFFFF;
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevel(actor, actorValue, &sidecarIndex, &sidecarLevel))
			return CalcMasteryFromSkill()(static_cast<SInt32>(sidecarLevel));

		return WeaponPerkMasteryOriginal()(actor, actorValue);
	}

	static void RefreshPowerAttackAnimData(ActorAnimData* animData, PlayerCharacter* player)
	{
		if (!animData || !player)
			return;

		ActorAnimDataRemovePowerAttackGroups()(animData);
		ObservedActorAnimDataBuildPowerAttackKFList()(animData, static_cast<TESObjectREFR*>(player), 0);
	}

	static void RefreshPlayerWeaponSidecarPowerAttackGroups()
	{
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;

		RefreshPowerAttackAnimData(TESObjectREFRGetAnimData()(static_cast<TESObjectREFR*>(player)), player);
		RefreshPowerAttackAnimData(player->firstPersonAnimData, player);
	}

	static bool IsSpearWeapon(TESForm* form)
	{
		return IsWeaponForm(form) &&
			ClassifySidecarWeapon(static_cast<TESObjectWEAP*>(form)) == SpearSkillShared::kWeaponSkill_Spear;
	}

	static bool __cdecl TrySetMagicPopupWeaponTypeLabel(Tile* tile, UInt32 trait, const char* nativeLabel, TESForm* form)
	{
		const char* label = nativeLabel;
		if (trait == kMagicPopupWeaponLabelTrait && IsWeaponForm(form))
		{
			const SpearSkillShared::WeaponSkillKind kind = ClassifySidecarWeapon(static_cast<TESObjectWEAP*>(form));
			if (const char* splitLabel = GetWeaponSkillDisplayName(kind))
				label = splitLabel;
		}

		if (label == nativeLabel)
			return false;

		if (tile)
			TileSetString()(tile, trait, label);
		return true;
	}

	static __declspec(naked) void HookMagicPopupWeaponTypeLabelSetStringFromEdi()
	{
		__asm
		{
			push ecx
			push edi
			push dword ptr[esp + 0x10]
			push dword ptr[esp + 0x10]
			push ecx
			call TrySetMagicPopupWeaponTypeLabel
			add esp, 0x10
			pop ecx
			test al, al
			jz chainOriginal
			ret 0x08
			chainOriginal:
			jmp dword ptr[g_magicPopupEdiLabelOriginalTarget]
		}
	}

	static __declspec(naked) void HookMagicPopupWeaponTypeLabelSetStringFromEbp()
	{
		__asm
		{
			push ecx
			push ebp
			push dword ptr[esp + 0x10]
			push dword ptr[esp + 0x10]
			push ecx
			call TrySetMagicPopupWeaponTypeLabel
			add esp, 0x10
			pop ecx
			test al, al
			jz chainOriginal
			ret 0x08
			chainOriginal:
			jmp dword ptr[g_magicPopupEbpLabelOriginalTarget]
		}
	}

	static float GetSkillUseIncrement(UInt32 index, UInt32 useType)
	{
		float nativeUseIncrement = 1.0f;
		if (TESSkill* fallbackSkill = TESSkill::SkillForActorVal(kSkills[index].fallbackActorValue))
			nativeUseIncrement = useType == 1 ? fallbackSkill->useValue1 : fallbackSkill->useValue0;

		if (!std::isfinite(nativeUseIncrement) || nativeUseIncrement < 0.0f)
			return 0.0f;

		return nativeUseIncrement;
	}

	static bool AddWeaponProgress(SpearSkillShared::WeaponSkillKind kind, UInt32 useType, float baseDelta)
	{
		const UInt32 index = GetSkillIndexForKind(kind);
		if (index >= kSkillCount)
			return false;

		float gain = GetSkillUseIncrement(index, useType);
		if (baseDelta != 0.0f)
			gain *= baseDelta;
		if (!std::isfinite(gain) || gain <= 0.0f)
			return true;

		return AddSkillProgress(index, gain);
	}

	static const SkillUseCondition* GetSkillUseCondition(SpearSkillShared::WeaponSkillKind kind)
	{
		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (kSkillUseConditions[i].kind == kind)
				return &kSkillUseConditions[i];
		}
		return nullptr;
	}

	static bool SkillConditionAllowsProgress(SpearSkillShared::WeaponSkillKind kind, UInt32 actorValue, UInt32 useType)
	{
		const SkillUseCondition* condition = GetSkillUseCondition(kind);
		return condition &&
			condition->nativeActorValue == actorValue &&
			condition->useType == useType;
	}

	static double __fastcall HookTESActorBaseGetEquippableItemRating(TESForm* actorBase, void*, TESForm* item)
	{
		PushBaseWeaponRatingContext(actorBase);

		double result = 0.0;
		if (TESActorBaseGetEquippableItemRatingFn original = TESActorBaseGetEquippableItemRatingOriginal())
			result = original(actorBase, item);

		PopWeaponRatingContext();
		return result;
	}

	static __declspec(naked) void HookEquippableWeaponRatingSelector()
	{
		__asm
		{
			pushad
			push dword ptr[esp + 36]
			call PushBaseWeaponRatingContext
			add esp, 4
			popad

			push dword ptr[esp + 16]
			push dword ptr[esp + 16]
			push dword ptr[esp + 16]
			push dword ptr[esp + 16]
			call dword ptr[g_equippableWeaponRatingSelectorOriginal]

			push eax
			pushad
			call PopWeaponRatingContext
			popad
			pop eax
			ret 16
		}
	}

	static void __fastcall HookPlayerModExperience(PlayerCharacter* player, void*, UInt32 actorValue, UInt32 useType, float baseDelta)
	{
		if (player == GetPlayer() && (actorValue == kActorVal_Blade || actorValue == kActorVal_Blunt))
		{
			TESObjectWEAP* equipped = GetPlayerEquippedWeapon();
			const SpearSkillShared::WeaponSkillKind kind = ClassifySidecarWeapon(equipped);
			
			const bool conditionAllows = SkillConditionAllowsProgress(kind, actorValue, useType);
	
			if (conditionAllows && AddWeaponProgress(kind, useType, baseDelta))
				return;
		}

		if (PlayerModExperienceOriginal())
			PlayerModExperienceOriginal()(player, actorValue, useType, baseDelta);
	}

	static void ClearPendingWeaponSkillConsumer()
	{
		std::memset(&g_pendingWeaponSkillConsumer, 0, sizeof(g_pendingWeaponSkillConsumer));
	}

	static bool IsEquippedByPlayer(TESObjectWEAP* weapon)
	{
		return weapon && GetPlayerEquippedWeapon() == weapon;
	}

	static UInt32 NativeWeaponSkillAV(TESObjectWEAP* weapon)
	{
		static constexpr UInt32 kNativeWeaponSkillActorValues[] =
		{
			kActorVal_Blade,
			kActorVal_Blade,
			kActorVal_Blunt,
			kActorVal_Blunt,
			kActorVal_Blunt,
			kActorVal_Marksman
		};

		if (!weapon || weapon->type >= sizeof(kNativeWeaponSkillActorValues) / sizeof(kNativeWeaponSkillActorValues[0]))
			return kActorVal_Blunt;

		return kNativeWeaponSkillActorValues[weapon->type];
	}

	static UInt32 GetNativeWeaponSkillAV(TESObjectWEAP* weapon)
	{
		if (GetWeaponSkillAVFn original = GetWeaponSkillAVOriginal())
			return original(weapon);

		return NativeWeaponSkillAV(weapon);
	}

	static bool ShouldCaptureWeaponSkillConsumer(UInt32 sourceReturnAddress)
	{
		return sourceReturnAddress == kRetWeaponSkillEquippedDamage ||
			sourceReturnAddress == kRetWeaponSkillPlayerInventoryRating ||
			sourceReturnAddress == kRetWeaponSkillEquippableRatingA ||
			sourceReturnAddress == kRetWeaponSkillEquippableRatingB ||
			sourceReturnAddress == kRetWeaponSkillActorBaseEquippableRating ||
			sourceReturnAddress == kRetWeaponSkillPowerAttackBonus ||
			sourceReturnAddress == kRetWeaponSkillProjectileDamage;
	}

	static bool SourceCanUsePlayerSidecarSkill(UInt32 sourceReturnAddress, TESObjectWEAP* weapon)
	{
		if (sourceReturnAddress == kRetWeaponSkillPlayerInventoryRating)
			return true;

		if (sourceReturnAddress == kRetWeaponSkillActorBaseEquippableRating)
			return false;

		if ((sourceReturnAddress == kRetWeaponSkillEquippableRatingA ||
			sourceReturnAddress == kRetWeaponSkillEquippableRatingB) &&
			GetCurrentWeaponRatingNpcContext())
			return false;

		return IsEquippedByPlayer(weapon);
	}

	static TESNPC* SourceBaseNpcSidecarContext(UInt32 sourceReturnAddress)
	{
		if (sourceReturnAddress != kRetWeaponSkillActorBaseEquippableRating &&
			sourceReturnAddress != kRetWeaponSkillEquippableRatingA &&
			sourceReturnAddress != kRetWeaponSkillEquippableRatingB)
			return nullptr;

		return GetCurrentWeaponRatingNpcContext();
	}

	static void CapturePendingWeaponSkillConsumer(TESObjectWEAP* weapon, SpearSkillShared::WeaponSkillKind kind, UInt32 sourceReturnAddress)
	{
		g_pendingWeaponSkillConsumer.active = true;
		g_pendingWeaponSkillConsumer.playerFacing = SourceCanUsePlayerSidecarSkill(sourceReturnAddress, weapon);
		g_pendingWeaponSkillConsumer.npcContext = SourceBaseNpcSidecarContext(sourceReturnAddress);
		g_pendingWeaponSkillConsumer.baseNpcFacing =
			!g_pendingWeaponSkillConsumer.playerFacing &&
			g_pendingWeaponSkillConsumer.npcContext != nullptr;
		g_pendingWeaponSkillConsumer.sourceReturnAddress = sourceReturnAddress;
		g_pendingWeaponSkillConsumer.weapon = weapon;
		g_pendingWeaponSkillConsumer.kind = kind;
	}

	static bool PendingWeaponSkillMatchesDamageReturn(UInt32 sourceReturnAddress, UInt32 damageReturnAddress)
	{
		switch (sourceReturnAddress)
		{
		case kRetWeaponSkillEquippedDamage:
			return damageReturnAddress == kRetEquippedWeaponDamage;
		case kRetWeaponSkillPlayerInventoryRating:
			return damageReturnAddress == kRetPlayerInventoryWeaponRating;
		case kRetWeaponSkillEquippableRatingA:
		case kRetWeaponSkillEquippableRatingB:
			return damageReturnAddress == kRetEquippableWeaponRating;
		case kRetWeaponSkillActorBaseEquippableRating:
			return damageReturnAddress == kRetActorBaseEquippableWeaponRating;
		case kRetWeaponSkillProjectileDamage:
			return damageReturnAddress == kRetWeaponDamageWrapper;
		default:
			return false;
		}
	}

	static bool PendingWeaponSkillMatchesPowerAttackReturn(UInt32 sourceReturnAddress, UInt32 powerAttackReturnAddress)
	{
		return sourceReturnAddress == kRetWeaponSkillPowerAttackBonus &&
			powerAttackReturnAddress == kRetPowerAttackBonus;
	}

	static bool TryGetPendingSidecarWeaponSkill(UInt32 returnAddress, bool powerAttack, UInt32* outLevel)
	{
		if (outLevel)
			*outLevel = 0;
		if (!g_pendingWeaponSkillConsumer.active)
			return false;

		const bool matches = powerAttack ?
			PendingWeaponSkillMatchesPowerAttackReturn(g_pendingWeaponSkillConsumer.sourceReturnAddress, returnAddress) :
			PendingWeaponSkillMatchesDamageReturn(g_pendingWeaponSkillConsumer.sourceReturnAddress, returnAddress);
		if (!matches)
			return false;

		const UInt32 index = GetSkillIndexForKind(g_pendingWeaponSkillConsumer.kind);
		if (index >= kSkillCount)
			return false;

		if (g_pendingWeaponSkillConsumer.playerFacing)
		{
			if (outLevel)
				*outLevel = (g_tcs && g_tcs->GetSkillLevel) ? g_tcs->GetSkillLevel(SpearSkillShared::kSpearSkillName) : 5;
			return true;
		}

		if (!powerAttack && g_pendingWeaponSkillConsumer.baseNpcFacing)
		{
			UInt32 npcSkill = 0;
			if (TryGetNpcSpearSkill(g_pendingWeaponSkillConsumer.npcContext, &npcSkill))
			{
				if (outLevel)
					*outLevel = npcSkill;
				return true;
			}
		}

		return false;
	}

	static UInt32 __fastcall HookGetWeaponSkillAV(TESObjectWEAP* weapon, void*)
	{
		const UInt32 nativeActorValue = GetNativeWeaponSkillAV(weapon);
		const UInt32 returnAddress = reinterpret_cast<UInt32>(_ReturnAddress());
		ClearPendingWeaponSkillConsumer();

		if (weapon && ShouldCaptureWeaponSkillConsumer(returnAddress))
		{
			const SpearSkillShared::WeaponSkillKind kind = ClassifySidecarWeapon(weapon);
			if (kind != SpearSkillShared::kWeaponSkill_None)
				CapturePendingWeaponSkillConsumer(weapon, kind, returnAddress);
		}

		return nativeActorValue;
	}

	static double CallOriginalWeaponDamage(int weaponSkill, int luck, int strengthOrAgility, float fatigue, int weaponDamage, float condition, float multiplier, float ignoreFatigue)
	{
		if (CalcWeaponDamageFn original = CalcWeaponDamageOriginal())
			return original(weaponSkill, luck, strengthOrAgility, fatigue, weaponDamage, condition, multiplier, ignoreFatigue);

		return 0.0;
	}

	static double __cdecl HookCalcWeaponDamage(int weaponSkill, int luck, int strengthOrAgility, float fatigue, int weaponDamage, float condition, float multiplier, float ignoreFatigue)
	{
		UInt32 sidecarSkill = 0;
		if (TryGetPendingSidecarWeaponSkill(reinterpret_cast<UInt32>(_ReturnAddress()), false, &sidecarSkill))
			weaponSkill = static_cast<int>(sidecarSkill);
		ClearPendingWeaponSkillConsumer();
		return CallOriginalWeaponDamage(weaponSkill, luck, strengthOrAgility, fatigue, weaponDamage, condition, multiplier, ignoreFatigue);
	}

	static double CallOriginalPowerAttackBonus(int skillLevel, int attackType)
	{
		if (CalcPowerAttackBonusFn original = CalcPowerAttackBonusOriginal())
			return original(skillLevel, attackType);

		return 1.0;
	}

	static double __cdecl HookCalcPowerAttackBonus(int skillLevel, int attackType)
	{
		UInt32 sidecarSkill = 0;
		if (TryGetPendingSidecarWeaponSkill(reinterpret_cast<UInt32>(_ReturnAddress()), true, &sidecarSkill))
			skillLevel = static_cast<int>(sidecarSkill);
		ClearPendingWeaponSkillConsumer();
		return CallOriginalPowerAttackBonus(skillLevel, attackType);
	}

	static UInt32 GetFormId(void* form)
	{
		return form ? *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(form) + kTESFormFormIdOffset) : 0;
	}

	static bool BytesEqual(UInt32 address, const UInt8* expected, UInt32 length)
	{
		return std::memcmp(reinterpret_cast<const void*>(address), expected, length) == 0;
	}

	static UInt32 ReadRelCallTarget(UInt32 address)
	{
		const SInt32 offset = *reinterpret_cast<const SInt32*>(address + 1);
		return address + 5 + offset;
	}

	static UInt32 ReadRelJumpTarget(UInt32 address)
	{
		const SInt32 offset = *reinterpret_cast<const SInt32*>(address + 1);
		return address + 5 + offset;
	}

	static void WriteRel32(UInt8* code, UInt32 offset, UInt8 opcode, UInt32 target)
	{
		code[offset] = opcode;
		*reinterpret_cast<UInt32*>(code + offset + 1) =
			target - (reinterpret_cast<UInt32>(code) + offset) - 5;
	}

	static bool WriteRelCallChecked(const char* name, UInt32 address, UInt32 expectedTarget, UInt32 hookTarget)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] != 0xE8)
		{
			_ERROR("SpearSkill: %s at %08X is not a call", name, address);
			++g_failedPatches;
			return false;
		}

		const UInt32 currentTarget = ReadRelCallTarget(address);
		if (currentTarget == hookTarget)
			return true;
		if (currentTarget != expectedTarget)
		{
			_ERROR("SpearSkill: %s target mismatch at %08X expected %08X actual %08X", name, address, expectedTarget, currentTarget);
			++g_failedPatches;
			return false;
		}

		WriteRelCall(address, hookTarget);
		++g_appliedPatches;
		return true;
	}

	static bool WriteRelCallChained(const char* name, UInt32 address, UInt32 expectedTarget, UInt32 hookTarget, UInt32& originalTarget)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] != 0xE8)
		{
			_ERROR("SpearSkill: %s at %08X is not a call", name, address);
			++g_failedPatches;
			return false;
		}

		const UInt32 currentTarget = ReadRelCallTarget(address);
		if (currentTarget == hookTarget)
			return true;
		if (currentTarget != expectedTarget)
		{
			if (originalTarget != expectedTarget && originalTarget != currentTarget)
			{
				_ERROR("SpearSkill: %s target mismatch at %08X expected %08X or chained %08X actual %08X",
					name, address, expectedTarget, originalTarget, currentTarget);
				++g_failedPatches;
				return false;
			}

			originalTarget = currentTarget;
			_MESSAGE("SpearSkill: chaining existing %s target=%08X", name, currentTarget);
		}

		WriteRelCall(address, hookTarget);
		++g_appliedPatches;
		return true;
	}

	static void* CreateCalcWeaponDamageGateway()
	{
		if (!BytesEqual(kCalcWeaponDamage, kCalcWeaponDamageExpected, sizeof(kCalcWeaponDamageExpected)))
		{
			_ERROR("SpearSkill: cannot build Calc_WeaponDamage gateway because signature does not match");
			++g_failedPatches;
			return nullptr;
		}

		static constexpr UInt32 kGatewayLength = 23;
		UInt8* gateway = static_cast<UInt8*>(VirtualAlloc(nullptr, kGatewayLength, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!gateway)
		{
			_ERROR("SpearSkill: VirtualAlloc failed creating Calc_WeaponDamage gateway gle=%u", GetLastError());
			++g_failedPatches;
			return nullptr;
		}

		std::memcpy(gateway, kCalcWeaponDamageExpected, 13);
		WriteRel32(gateway, 13, 0xE8, kCalcLuckModifiedSkill);
		WriteRel32(gateway, 18, 0xE9, kCalcWeaponDamageContinue);
		FlushInstructionCache(GetCurrentProcess(), gateway, kGatewayLength);
		return gateway;
	}

	static void* CreateCalcPowerAttackBonusGateway()
	{
		if (!BytesEqual(kCalcPowerAttackBonus, kCalcPowerAttackBonusExpected, sizeof(kCalcPowerAttackBonusExpected)))
		{
			_ERROR("SpearSkill: cannot build Calc_PowerAttackBonus gateway because signature does not match");
			++g_failedPatches;
			return nullptr;
		}

		static constexpr UInt32 kGatewayLength = 26;
		UInt8* gateway = static_cast<UInt8*>(VirtualAlloc(nullptr, kGatewayLength, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!gateway)
		{
			_ERROR("SpearSkill: VirtualAlloc failed creating Calc_PowerAttackBonus gateway gle=%u", GetLastError());
			++g_failedPatches;
			return nullptr;
		}

		std::memcpy(gateway, kCalcPowerAttackBonusExpected, 16);
		WriteRel32(gateway, 16, 0xE8, kCalcMasteryFromSkill);
		WriteRel32(gateway, 21, 0xE9, kCalcPowerAttackBonusContinue);
		FlushInstructionCache(GetCurrentProcess(), gateway, kGatewayLength);
		return gateway;
	}

	static bool WriteRelJumpChecked(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength);
	static bool WriteRelJumpRaw(const char* name, UInt32 address, UInt32 target, UInt32 patchLength = 5);

	static bool WriteRelJumpChainable(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 hookTarget, UInt32 patchLength, UInt32& chainTarget)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(address);
			if (currentTarget == hookTarget)
				return true;

			chainTarget = currentTarget;
			_MESSAGE("SpearSkill: chaining existing %s target=%08X", name, currentTarget);
			return WriteRelJumpRaw(name, address, hookTarget, patchLength);
		}

		return WriteRelJumpChecked(name, address, expected, expectedLength, hookTarget, patchLength);
	}

	static bool InstallCalcWeaponDamageHook()
	{
		const UInt32 hookTarget = reinterpret_cast<UInt32>(&HookCalcWeaponDamage);
		const UInt8* actual = reinterpret_cast<const UInt8*>(kCalcWeaponDamage);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(kCalcWeaponDamage);
			if (currentTarget == hookTarget)
				return true;

			g_calcWeaponDamageOriginal = reinterpret_cast<void*>(currentTarget);
			_MESSAGE("SpearSkill: chaining existing Calc_WeaponDamage sidecar skill substitution target=%08X", currentTarget);
			return WriteRelJumpRaw("Calc_WeaponDamage sidecar skill substitution chained",
				kCalcWeaponDamage,
				hookTarget,
				sizeof(kCalcWeaponDamageExpected));
		}

		if (!g_calcWeaponDamageOriginal)
			g_calcWeaponDamageOriginal = CreateCalcWeaponDamageGateway();
		if (!g_calcWeaponDamageOriginal)
			return false;

		return WriteRelJumpChecked("Calc_WeaponDamage sidecar skill substitution",
			kCalcWeaponDamage,
			kCalcWeaponDamageExpected,
			sizeof(kCalcWeaponDamageExpected),
			hookTarget,
			sizeof(kCalcWeaponDamageExpected));
	}

	static bool InstallCalcPowerAttackBonusHook()
	{
		const UInt32 hookTarget = reinterpret_cast<UInt32>(&HookCalcPowerAttackBonus);
		const UInt8* actual = reinterpret_cast<const UInt8*>(kCalcPowerAttackBonus);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(kCalcPowerAttackBonus);
			if (currentTarget == hookTarget)
				return true;

			g_calcPowerAttackBonusOriginal = reinterpret_cast<void*>(currentTarget);
			_MESSAGE("SpearSkill: chaining existing Calc_PowerAttackBonus sidecar skill substitution target=%08X", currentTarget);
			return WriteRelJumpRaw("Calc_PowerAttackBonus sidecar skill substitution chained",
				kCalcPowerAttackBonus,
				hookTarget,
				sizeof(kCalcPowerAttackBonusExpected));
		}

		if (!g_calcPowerAttackBonusOriginal)
			g_calcPowerAttackBonusOriginal = CreateCalcPowerAttackBonusGateway();
		if (!g_calcPowerAttackBonusOriginal)
			return false;

		return WriteRelJumpChecked("Calc_PowerAttackBonus sidecar skill substitution",
			kCalcPowerAttackBonus,
			kCalcPowerAttackBonusExpected,
			sizeof(kCalcPowerAttackBonusExpected),
			hookTarget,
			sizeof(kCalcPowerAttackBonusExpected));
	}

	static bool WriteRelJumpChecked(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] == 0xE9 && ReadRelJumpTarget(address) == target)
			return true;
		if (!BytesEqual(address, expected, expectedLength))
		{
			_ERROR("SpearSkill: signature mismatch for %s at %08X", name, address);
			++g_failedPatches;
			return false;
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, patchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			++g_failedPatches;
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < patchLength; ++i)
			code[i] = 0x90;
		FlushInstructionCache(GetCurrentProcess(), ptr, patchLength);
		DWORD ignored = 0;
		VirtualProtect(ptr, patchLength, oldProtect, &ignored);
		++g_appliedPatches;
		return true;
	}

	static bool WriteRelJumpRaw(const char* name, UInt32 address, UInt32 target, UInt32 patchLength)
	{
		if (patchLength < 5)
		{
			_ERROR("SpearSkill: invalid raw jump patch length for %s at %08X length=%u", name, address, patchLength);
			++g_failedPatches;
			return false;
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, patchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			_ERROR("SpearSkill: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			++g_failedPatches;
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < patchLength; ++i)
			code[i] = 0x90;
		FlushInstructionCache(GetCurrentProcess(), ptr, patchLength);
		DWORD ignored = 0;
		VirtualProtect(ptr, patchLength, oldProtect, &ignored);
		++g_appliedPatches;
		_MESSAGE("SpearSkill: installed %s at %08X", name, address);
		return true;
	}

	static void* CreateTrampoline(UInt32 address, UInt32 stolenLength)
	{
		const UInt32 length = stolenLength + 5;
		UInt8* trampoline = static_cast<UInt8*>(VirtualAlloc(nullptr, length, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!trampoline)
			return nullptr;

		std::memcpy(trampoline, reinterpret_cast<const void*>(address), stolenLength);
		trampoline[stolenLength] = 0xE9;
		*reinterpret_cast<UInt32*>(trampoline + stolenLength + 1) =
			(address + stolenLength) - (reinterpret_cast<UInt32>(trampoline) + stolenLength) - 5;
		FlushInstructionCache(GetCurrentProcess(), trampoline, length);
		return trampoline;
	}

	static bool InstallFunctionJumpHook(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength, void*& original)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(address);
			if (currentTarget == target)
				return true;

			original = reinterpret_cast<void*>(currentTarget);
			_MESSAGE("SpearSkill: chaining existing %s target=%08X", name, currentTarget);
			return WriteRelJumpRaw(name, address, target);
		}

		if (!original)
			original = CreateTrampoline(address, patchLength);
		if (!original)
		{
			_ERROR("SpearSkill: failed to create trampoline for %s at %08X", name, address);
			++g_failedPatches;
			return false;
		}

		return WriteRelJumpChecked(name, address, expected, expectedLength, target, patchLength);
	}

	static bool InstallHooks()
	{
		g_appliedPatches = 0;
		g_failedPatches = 0;
		ClearPendingWeaponSkillConsumer();
		ClearWeaponRatingNpcContext();
		bool ok = true;

		ok &= InstallFunctionJumpHook("Player_ModExperience weapon sidecar progress",
			kPlayerModExperience,
			kPlayerModExperienceExpected,
			sizeof(kPlayerModExperienceExpected),
			reinterpret_cast<UInt32>(&HookPlayerModExperience),
			kPlayerModExperiencePatchLength,
			g_playerModExperienceOriginal);

		ok &= InstallFunctionJumpHook("TESObjectWEAP_GetWeaponSkillAV sidecar consumer context",
			kTESObjectWEAPGetWeaponSkillAV,
			kTESObjectWEAPGetWeaponSkillAVExpected,
			sizeof(kTESObjectWEAPGetWeaponSkillAVExpected),
			reinterpret_cast<UInt32>(&HookGetWeaponSkillAV),
			sizeof(kTESObjectWEAPGetWeaponSkillAVExpected),
			g_getWeaponSkillAVOriginal);

		ok &= InstallFunctionJumpHook("TESActorBase Spear NPC sidecar weapon-rating context",
			kTESActorBaseGetEquippableItemRating,
			kTESActorBaseGetEquippableItemRatingExpected,
			sizeof(kTESActorBaseGetEquippableItemRatingExpected),
			reinterpret_cast<UInt32>(&HookTESActorBaseGetEquippableItemRating),
			kTESActorBaseGetEquippableItemRatingPatchLength,
			g_actorBaseGetEquippableItemRatingOriginal);

		ok &= InstallFunctionJumpHook("Equippable weapon Spear NPC sidecar rating context",
			kEquippableWeaponRatingSelector,
			kEquippableWeaponRatingSelectorExpected,
			sizeof(kEquippableWeaponRatingSelectorExpected),
			reinterpret_cast<UInt32>(&HookEquippableWeaponRatingSelector),
			kEquippableWeaponRatingSelectorPatchLength,
			g_equippableWeaponRatingSelectorOriginal);

		ok &= WriteRelJumpChainable("CombatController weapon skill sidecar scoring",
			kCombatControllerWeaponSkillCall,
			kCombatControllerWeaponSkillExpected,
			sizeof(kCombatControllerWeaponSkillExpected),
			reinterpret_cast<UInt32>(&HookCombatControllerWeaponSkillLevel),
			sizeof(kCombatControllerWeaponSkillExpected),
			g_combatControllerWeaponSkillChainTarget);

		ok &= WriteRelJumpChainable("Combat selection weapon skill sidecar scoring",
			kCombatSelectionHandToHandComparePatch,
			kCombatSelectionHandToHandCompareExpected,
			sizeof(kCombatSelectionHandToHandCompareExpected),
			reinterpret_cast<UInt32>(&HookCombatSelectionHandToHandSkillCompare),
			sizeof(kCombatSelectionHandToHandCompareExpected),
			g_combatSelectionHandToHandChainTarget);

		ok &= InstallCalcWeaponDamageHook();
		ok &= InstallCalcPowerAttackBonusHook();

		ok &= WriteRelCallChained("PowerAttack KF list weapon sidecar mastery hook", kPowerAttackBuilderMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk), g_weaponPerkMasteryOriginalTarget);
		ok &= WriteRelCallChained("Weapon sidecar sidestep disarm perk level hook", kWeaponDisarmBaseCalcCall, kActorGetBaseCalcAVi, reinterpret_cast<UInt32>(&HookActorGetBaseCalcAViForWeaponSidecarPerk), g_weaponPerkBaseCalcOriginalTarget);
		ok &= WriteRelCallChained("Weapon sidecar sweeping perk mastery hook", kPostHitExpertMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk), g_weaponPerkMasteryOriginalTarget);
		ok &= WriteRelCallChained("Weapon sidecar rushing paralyze mastery hook", kPostHitMasterMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk), g_weaponPerkMasteryOriginalTarget);

		ok &= WriteRelCallChained("MagicPopupMenu enchanted weapon Type sidecar label hook", kMagicPopupEnchantedWeaponLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEbp), g_magicPopupEbpLabelOriginalTarget);
		ok &= WriteRelCallChained("MagicPopupMenu enchanted weapon Type sidecar null-label hook", kMagicPopupEnchantedWeaponNullLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEbp), g_magicPopupEbpLabelOriginalTarget);
		ok &= WriteRelCallChained("MagicPopupMenu weapon Type sidecar label hook", kMagicPopupSimpleWeaponLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEdi), g_magicPopupEdiLabelOriginalTarget);

		_MESSAGE("SpearSkill: native hooks installed applied=%u failed=%u", g_appliedPatches, g_failedPatches);
		return ok && g_failedPatches == 0;
	}

	static bool InstallHooksOnce()
	{
		if (g_hooksInstalled)
			return true;
		if (g_hookInstallAttempted)
			return false;

		g_hookInstallAttempted = true;
		g_hooksInstalled = InstallHooks();
		return g_hooksInstalled;
	}

	static void SetGameSettingStringIfPresent(const char* settingName, const char* value)
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>(settingName), &setting) && setting)
			setting->Set(value);
	}

	static void PatchVisibleGameSettingLabels()
	{
	}

	static UInt32 GetSpearSkill()
	{
		if (!g_tcs || !g_tcs->GetSkillLevel)
			return 5;
		return g_tcs->GetSkillLevel(SpearSkillShared::kSpearSkillName);
	}

	static void SetSpearSkillClamped(SInt64 level)
	{
		if (!g_tcs || !g_tcs->SetSkillLevel || !g_tcs->GetSkillLevel)
			return;

		const UInt32 previousLevel = g_tcs->GetSkillLevel(SpearSkillShared::kSpearSkillName);

		UInt32 clamped;
		if (level < 0)
			clamped = 0;
		else if (level > static_cast<SInt64>(kMaxSkillLevel))
			clamped = kMaxSkillLevel;
		else
			clamped = static_cast<UInt32>(level);

		g_tcs->SetSkillLevel(SpearSkillShared::kSpearSkillName, clamped);

		if (clamped != previousLevel)
			RefreshPlayerWeaponSidecarPowerAttackGroups();
	}

	static void ModSpearSkill(SInt32 delta)
	{
		if (!g_tcs || !g_tcs->GetSkillLevel)
			return;

		const SInt64 currentLevel = static_cast<SInt64>(g_tcs->GetSkillLevel(SpearSkillShared::kSpearSkillName));
		SetSpearSkillClamped(currentLevel + static_cast<SInt64>(delta));
	}

	static float GetSpearProgress()
	{
		if (!g_tcs || !g_tcs->GetSkillProgress)
			return 0.0f;
		return g_tcs->GetSkillProgress(SpearSkillShared::kSpearSkillName);
	}

	static void SetSpearProgress(float progress)
	{
		if (!g_tcs || !g_tcs->SetSkillProgress)
			return;
		g_tcs->SetSkillProgress(SpearSkillShared::kSpearSkillName, progress);
	}

	static float GetSpearRequiredProgress()
	{
		if (!g_tcs || !g_tcs->GetSkillRequiredProgress)
			return 1.0f;
		return g_tcs->GetSkillRequiredProgress(SpearSkillShared::kSpearSkillName);
	}

	static UInt32 GetSpearLevelUps()
	{
		if (!g_tcs || !g_tcs->GetSkillLevelUps)
			return 0;
		return g_tcs->GetSkillLevelUps(SpearSkillShared::kSpearSkillName);
	}

	static UInt32 GetSpearGoverningAttributeIncreases()
	{
		if (!g_tcs || !g_tcs->GetSkillGoverningAttributeIncreases)
			return 0;
		return g_tcs->GetSkillGoverningAttributeIncreases(SpearSkillShared::kSpearSkillName);
	}

	static UInt32 GetSpearMastery()
	{
		if (!g_tcs || !g_tcs->GetSkillMastery)
			return 0;
		return g_tcs->GetSkillMastery(SpearSkillShared::kSpearSkillName);
	}

	static UInt32 GetSpearPerkMask()
	{
		const UInt32 level = GetSpearSkill();
		UInt32 mask = 0;
		if (level >= 25)
			mask |= 1u;
		if (level >= 50)
			mask |= 2u;
		if (level >= 75)
			mask |= 4u;
		if (level >= 100)
			mask |= 8u;
		return mask;
	}

	static bool IsSpearMajorSkill()
	{
		if (!g_tcs || !g_tcs->IsSkillMajor)
			return false;
		return g_tcs->IsSkillMajor(SpearSkillShared::kSpearSkillName);
	}

	static bool SaveWeaponTypeSidecars()
	{
		if (!g_serialization)
			return false;

		const UInt32 count = g_weaponTypeStore.Count();
		if (!g_serialization->OpenRecord(kRecordWeaponTypes, kWeaponTypesRecordVersion))
			return false;
		if (!g_serialization->WriteRecordData(&count, sizeof(count)))
			return false;

		for (UInt32 i = 0; i < count; ++i)
		{
			const SpearSkillShared::WeaponTypeSidecarEntry& source = g_weaponTypeStore.EntryAt(i);
			SavedWeaponTypeEntry entry = {};
			entry.formId = source.formId;
			entry.kind = static_cast<UInt32>(source.kind);
			if (!g_serialization->WriteRecordData(&entry, sizeof(entry)))
				return false;
		}

		return true;
	}

	static void LoadWeaponTypeSidecars(UInt32 version, UInt32 length)
	{
		if (!g_serialization)
			return;
		if (version != kWeaponTypesRecordVersion || length < sizeof(UInt32))
		{
			_WARNING("SpearSkill: ignored incompatible weapon Type save record version=%u length=%u", version, length);
			return;
		}

		UInt32 savedCount = 0;
		if (g_serialization->ReadRecordData(&savedCount, sizeof(savedCount)) != sizeof(savedCount))
			return;

		const UInt32 maxByLength = (length - sizeof(savedCount)) / sizeof(SavedWeaponTypeEntry);
		const UInt32 entriesToRead = savedCount < maxByLength ? savedCount : maxByLength;
		UInt32 resolved = 0;
		UInt32 unresolved = 0;
		for (UInt32 i = 0; i < entriesToRead; ++i)
		{
			SavedWeaponTypeEntry entry = {};
			if (g_serialization->ReadRecordData(&entry, sizeof(entry)) != sizeof(entry))
				break;

			UInt32 resolvedFormId = 0;
			if (!g_serialization->ResolveRefID(entry.formId, &resolvedFormId))
			{
				++unresolved;
				continue;
			}

			TESForm* form = LookupFormByID(resolvedFormId);
			const SpearSkillShared::WeaponSkillKind kind = static_cast<SpearSkillShared::WeaponSkillKind>(entry.kind);
			if (!IsWeaponForm(form) || SpearSkillShared::SkillIdForKind(kind) == 0xFFFFFFFF)
			{
				++unresolved;
				continue;
			}

			if (g_weaponTypeStore.Count() >= kMaxSavedWeaponTypeEntries)
			{
				++unresolved;
				continue;
			}

			if (g_weaponTypeStore.SetLoaded(resolvedFormId, kind))
				++resolved;
		}

		if (savedCount > entriesToRead)
			_WARNING("SpearSkill: weapon Type save record count=%u length only contained %u entries", savedCount, entriesToRead);

		g_weaponTypeStore.ClearDirty();
		_MESSAGE("SpearSkill: loaded weapon Type co-save sidecars resolved=%u unresolved=%u", resolved, unresolved);
	}

	static void SaveCallback(void*)
	{
		if (!SaveWeaponTypeSidecars())
			_WARNING("SpearSkill: failed to write weapon Type sidecar save record");
	}

	static void LoadCallback(void*)
	{
		ClearPendingWeaponSkillConsumer();
		ClearWeaponRatingNpcContext();
		g_weaponTypeStore.Clear();
		EnsureNpcSpearStoreConfigured();
		g_npcSkillStore.Clear();
		g_npcTrainingStore.Clear();
		if (!g_serialization)
			return;

		UInt32 type = 0;
		UInt32 version = 0;
		UInt32 length = 0;
		while (g_serialization->GetNextRecordInfo(&type, &version, &length))
		{
			if (type == kRecordWeaponTypes)
			{
				LoadWeaponTypeSidecars(version, length);
			}
			else
			{
				continue;
			}
		}

		LoadEditorWeaponTypeSidecars(true);
		LoadEditorNpcSpearSidecars(true);
		PatchVisibleGameSettingLabels();
	}

	static void NewGameCallback(void*)
	{
		ClearPendingWeaponSkillConsumer();
		ClearWeaponRatingNpcContext();
		g_weaponTypeStore.Clear();
		EnsureNpcSpearStoreConfigured();
		g_npcSkillStore.Clear();
		g_npcTrainingStore.Clear();
		LoadEditorWeaponTypeSidecars(false);
		LoadEditorNpcSpearSidecars(false);
		PatchVisibleGameSettingLabels();
	}

	static void RegisterSerializationCallbacks()
	{
		if (!g_serialization)
			return;

		g_serialization->SetSaveCallback(g_pluginHandle, SaveCallback);
		g_serialization->SetLoadCallback(g_pluginHandle, LoadCallback);
		g_serialization->SetNewGameCallback(g_pluginHandle, NewGameCallback);
	}

	static void MessageHandler(OBSEMessagingInterface::Message* message)
	{
		if (!message)
			return;

		switch (message->type)
		{
		case OBSEMessagingInterface::kMessage_PostPostLoad:
			if (g_messaging && g_messaging->Dispatch)
				g_messaging->Dispatch(g_pluginHandle, kMessage_TCSGetInterface, &g_tcs, sizeof(g_tcs), "TrueCustomSkills");
			if (!g_tcs)
				_ERROR("SpearSkill: failed to obtain True Custom Skills interface -- is TCS installed?");
			else if (g_tcs->interfaceVersion < 1)
				_ERROR("SpearSkill: True Custom Skills interface is older than expected (version %u, need >= 2) -- AddSkillXP unavailable", g_tcs->interfaceVersion);

			if (!InstallHooksOnce())
				_ERROR("SpearSkill: failed to install native hooks after OBSE plugin load");
			break;
		case OBSEMessagingInterface::kMessage_PostLoadGame:
			ClearPendingWeaponSkillConsumer();
			ClearWeaponRatingNpcContext();
			LoadEditorWeaponTypeSidecars(true);
			LoadEditorNpcSpearSidecars(true);
			PatchVisibleGameSettingLabels();
			break;
		}
	}

	static void RegisterMessaging(const OBSEInterface* obse)
	{
		if (!obse || !obse->QueryInterface || g_pluginHandle == kPluginHandle_Invalid)
			return;

		g_messaging =
			static_cast<OBSEMessagingInterface*>(obse->QueryInterface(kInterface_Messaging));
		if (g_messaging && g_messaging->RegisterListener)
			g_messaging->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
	}

	bool Cmd_SpearSetLevel_Execute(COMMAND_ARGS)
	{
		UInt32 level = 0;
		*result = 0.0;

		if (!ExtractArgs(PASS_EXTRACT_ARGS, &level))
			return true;

		SetSpearSkillClamped(static_cast<SInt64>(level));
		*result = static_cast<double>(GetSpearSkill());
		Console_Print("Spear level set to %u", GetSpearSkill());
		_MESSAGE("SpearSkill: SpearSetLevel requested=%u actual=%u", level, GetSpearSkill());
		return true;
	}

	DEFINE_COMMAND_PLUGIN(SpearSetLevel,
		"debug: directly sets the Spear skill level via TCS's SetSkillLevel (temporary, for testing the TCS migration)",
		0, 1, kParams_OneInt);

	bool Cmd_SpearGetInfo_Execute(COMMAND_ARGS)
	{
		*result = static_cast<double>(GetSpearSkill());

		Console_Print("Spear level=%u progress=%.2f/%.2f levelUps=%u governingAttrIncreases=%u mastery=%u major=%s perkMask=%u",
			GetSpearSkill(),
			GetSpearProgress(),
			GetSpearRequiredProgress(),
			GetSpearLevelUps(),
			GetSpearGoverningAttributeIncreases(),
			GetSpearMastery(),
			IsSpearMajorSkill() ? "true" : "false",
			GetSpearPerkMask());

		_MESSAGE("SpearSkill: SpearGetInfo level=%u progress=%.2f/%.2f levelUps=%u governingAttrIncreases=%u mastery=%u major=%d perkMask=%u",
			GetSpearSkill(),
			GetSpearProgress(),
			GetSpearRequiredProgress(),
			GetSpearLevelUps(),
			GetSpearGoverningAttributeIncreases(),
			GetSpearMastery(),
			IsSpearMajorSkill() ? 1 : 0,
			GetSpearPerkMask());
		return true;
	}

	DEFINE_COMMAND_PLUGIN(SpearGetInfo,
		"debug: prints every TCS-backed Spear skill value at once (temporary, for testing the TCS migration)",
		0, 0, NULL);

}

extern "C"
{
	bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info)
	{
		if (!obse || !info)
			return false;

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "Spear Skill";
		info->version = SpearSkill::kPluginVersion;

		if (obse->isEditor)
			return false;

		if (obse->obseVersion < OBSE_VERSION_INTEGER)
			return false;
		if (obse->oblivionVersion != OBLIVION_VERSION)
			return false;

		g_serialization = static_cast<OBSESerializationInterface*>(obse->QueryInterface(kInterface_Serialization));
		return g_serialization != nullptr;
	}

	bool OBSEPlugin_Load(const OBSEInterface* obse)
	{
		if (!obse)
			return false;

		g_pluginHandle = obse->GetPluginHandle();
		SpearSkill::RegisterSerializationCallbacks();
		SpearSkill::RegisterMessaging(obse);

		if (!obse->RegisterCommand(&SpearSkill::kCommandInfo_SpearSetLevel))
			_ERROR("SpearSkill: failed to register SpearSetLevel debug command");
		if (!obse->RegisterCommand(&SpearSkill::kCommandInfo_SpearGetInfo))
			_ERROR("SpearSkill: failed to register SpearGetInfo debug command");

		return true;
	}
}