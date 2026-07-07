#include "obse/PluginAPI.h"
#include "obse/GameAPI.h"
#include "obse/GameActorValues.h"
#include "obse/GameData.h"
#include "obse/GameForms.h"
#include "obse/GameObjects.h"
#include "obse/GameProcess.h"
#include "obse/GameTiles.h"
#include "obse_common/SafeWrite.h"
#include "SpearNpcSkillSidecar.h"
#include "SpearWeaponTypeSidecar.h"
#include "..\shared\SidecarSkillProvider.h"

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
	static constexpr const char* kSpearApprenticePerkText =
		kSpearApprenticeRankDescription;
	static constexpr const char* kSpearJourneymanPerkText =
		kSpearJourneymanRankDescription;
	static constexpr const char* kSpearExpertPerkText =
		kSpearExpertRankDescription;
	static constexpr const char* kSpearMasterPerkText =
		kSpearMasterRankDescription;
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
	static constexpr UInt32 kStatsMenuCreateRows = 0x005DC630;
	static constexpr UInt32 kStatsMenuRefresh = 0x005DA1A0;
	static constexpr UInt32 kStatsMenuDetails = 0x005DBBD0;
	static constexpr UInt32 kStatsMenuMasteryCountsPatch = 0x005DAAA0;
	static constexpr UInt32 kSkillsMenuPreselect = 0x005D5D40;
	static constexpr UInt32 kSkillsMenuUpdateAccept = 0x005D5AB0;
	static constexpr UInt32 kSkillsMenuDetails = 0x005D5B40;
	static constexpr UInt32 kSkillsMenuAccept = 0x005D5E50;
	static constexpr UInt32 kSkillsMenuCreateSkillRow = 0x005D6270;
	static constexpr UInt32 kSkillsMenuClose = 0x005D5720;
	static constexpr UInt32 kClassMenuCommit = 0x005973F0;
	static constexpr UInt32 kClassMenuRefreshDetails = 0x00596CF0;
	static constexpr UInt32 kClassMenuStepRefresh = 0x00584390;
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

	static constexpr UInt32 kStatsMenuSummaryTileOffset = 0x30;
	static constexpr UInt32 kStatsMenuSkillParentOffset = 0x3C;
	static constexpr UInt32 kStatsMenuFocusTileOffset = 0x54;
	static constexpr UInt32 kStatsMenuDetailTileOffset = 0x58;
	static constexpr UInt32 kStatsMenuSkillRowsOffset = 0x60;
	static constexpr UInt32 kSkillsMenuTileOffset = 0x04;
	static constexpr UInt32 kSkillsMenuListTileOffset = 0x28;
	static constexpr UInt32 kSkillsMenuAcceptButtonOffset = 0x34;
	static constexpr UInt32 kSkillsMenuModeOffset = 0x3C;
	static constexpr UInt32 kSkillsMenuCurrentValueOffset = 0x40;
	static constexpr UInt32 kSkillsMenuSelectionCapOffset = 0x44;
	static constexpr UInt32 kSkillsMenuSelectedTileOffset = 0x48;
	static constexpr UInt32 kSkillsMenuClassMenuOffset = 0x4C;
	static constexpr UInt32 kClassMenuTileOffset = 0x04;
	static constexpr UInt32 kClassMenuSelectedClassOffset = 0x3C;
	static constexpr UInt32 kClassMenuCustomClassOffset = 0x40;
	static constexpr UInt32 kClassMenuCurrentPickerValueOffset = 0x48;
	static constexpr UInt32 kClassMenuStepOffset = 0x58;
	static constexpr UInt32 kClassMenuSelectedSkillsOffset = 0x68;
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
	static constexpr UInt32 kPickerRowSelectedTrait = kTileValue_user3;
	static constexpr UInt32 kPickerRowValueTrait = kTileValue_user2;
	static constexpr UInt32 kPickerSyntheticSkillIdTrait = kTileValue_user22;
	static constexpr UInt32 kPickerSyntheticMarkerTrait = kTileValue_user23;
	static constexpr UInt32 kStatsRowSyntheticSkillIdTrait = kTileValue_user22;
	static constexpr UInt32 kStatsRowSyntheticMarkerTrait = kTileValue_user23;
	static constexpr UInt32 kSkillsMenuForwardEnabledTrait = kTileValue_user9;
	static constexpr UInt32 kTileValue_heightRaw = 0x00000FC9;
	static constexpr float kSyntheticPickerNativeValueSentinel = 0.0f;
	static constexpr UInt32 kStatsMenuMasteryRankCount = 5;
	static constexpr UInt32 kGenericMenuArgInt = 0;
	static constexpr UInt32 kGenericMenuArgFloat = 1;
	static constexpr UInt32 kGenericMenuArgString = 2;
	static constexpr UInt32 kGenericMenuArgEnd = 3;
	static constexpr float kStatsFocusDepthInset = 0.5f;
	static constexpr float kStatsFocusSizeInset = 12.0f;
	static constexpr float kStatsFocusYOffset = 10.0f;
	static constexpr const char* kStatsSkillTemplate = "stat_skill_template";
	static constexpr const char* kSkillPerkMenuXml = "skill_perk.xml";
	static constexpr const char* kSkillPerkOkText = "OK";

	static constexpr UInt32 kStatsMenuCreateRowsCall = 0x005DCCA3;
	static constexpr UInt32 kSkillsMenuPreselectCall = 0x005D65E3;
	static constexpr UInt32 kClassMenuCommitCall = 0x00597521;
	static constexpr UInt32 kClassMenuRefreshDetailsCalls[] =
	{
		0x00597682,
		0x00597121,
		0x00597189,
		0x005971C9,
		0x005974C1,
	};
	static constexpr UInt32 kStatsMenuRefreshCalls[] =
	{
		0x0057A78F,
		0x005DC8CB,
		0x005DCC9C,
		0x005DCE34,
		0x005DCEE5,
		0x005DCEFC,
	};

	static constexpr UInt32 kActorValueGetNamePatchLength = 7;
	static constexpr UInt32 kStatsMenuDetailsPatchLength = 14;
	static constexpr UInt32 kStatsMenuMasteryCountsPatchLength = 10;
	static constexpr UInt32 kSkillsMenuUpdateAcceptPatchLength = 5;
	static constexpr UInt32 kSkillsMenuDetailsPatchLength = 7;
	static constexpr UInt32 kSkillsMenuAcceptPatchLength = 14;
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
	static const UInt8 kActorValueGetNameExpected[kActorValueGetNamePatchLength] =
	{
		0x8B, 0x44, 0x24, 0x04,
		0x83, 0xF8, 0x27
	};
	static const UInt8 kStatsMenuDetailsExpected[kStatsMenuDetailsPatchLength] =
	{
		0x6A, 0xFF,
		0x68, 0xF0, 0x20, 0x9C, 0x00,
		0x64, 0xA1, 0x00, 0x00, 0x00, 0x00,
		0x50
	};
	static const UInt8 kStatsMenuMasteryCountsExpected[kStatsMenuMasteryCountsPatchLength] =
	{
		0xC7, 0x44, 0x24, 0x14, 0x05, 0x00, 0x00, 0x00,
		0xEB, 0x08
	};
	static const UInt8 kSkillsMenuUpdateAcceptExpected[kSkillsMenuUpdateAcceptPatchLength] =
	{
		0x56, 0x8B, 0xF1, 0x8B, 0x06
	};
	static const UInt8 kSkillsMenuDetailsExpected[kSkillsMenuDetailsPatchLength] =
	{
		0x8B, 0x44, 0x24, 0x04,
		0x83, 0xF8, 0xFF
	};
	static const UInt8 kSkillsMenuAcceptExpected[kSkillsMenuAcceptPatchLength] =
	{
		0x6A, 0xFF,
		0x68, 0xD8, 0x5B, 0x9B, 0x00,
		0x64, 0xA1, 0x00, 0x00, 0x00, 0x00,
		0x50
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

	struct SkillLevelProgression
	{
		UInt32 level;
		float progressionValue;
		float scalingValue;
		const char* upgradeDescription;
	};

	struct SkillProgressionDefinition
	{
		UInt32 skillId;
		const SkillLevelProgression* levels;
		UInt32 levelCount;
	};

	struct SkillUseCondition
	{
		SpearSkillShared::WeaponSkillKind kind;
		UInt32 nativeActorValue;
		UInt32 useType;
	};

	struct MasteryPerkText
	{
		const char* apprentice;
		const char* journeyman;
		const char* expert;
		const char* master;
	};

	struct SaveState
	{
		UInt32 version;
		SkillState states[kSkillCount];
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

	struct StatsRow
	{
		Tile* tile;
		bool placementLogged;
		float lastLoggedOrder;
	};

	struct PickerRow
	{
		Tile* tile;
		bool placementLogged;
	};

	struct TrainingPolicyState
	{
		void* menu;
		TESNPC* trainerBase;
		UInt32 trainerLevel;
		UInt32 cost;
		bool used;
	};

	static const SkillDefinition kSkills[kSkillCount] =
	{
		{ SpearSkillShared::kSpearSkillId, "Spear", kSpearSkillDisplayName, TESClass::eSpec_Combat, kActorVal_Endurance, kActorVal_Blade, kSpearSkillIconPath, kSpearSkillRowIconPath, kVanillaBladeSkillIconPath, kVanillaBladeSkillSmallIconPath, kSpearSkillDescription, kSpearClassPickerDescription },
	};

#define SPEAR_LEVEL_PROGRESS(level) { level, static_cast<float>(level), static_cast<float>(level), kSkillProgressionPlaceholderDescription }
	static const SkillLevelProgression kDefaultSkillLevelProgression[kSkillProgressionLevelCount] =
	{
		{ 0, 0.0f, 1.0f, kSkillProgressionPlaceholderDescription },
		SPEAR_LEVEL_PROGRESS(1),
		SPEAR_LEVEL_PROGRESS(2),
		SPEAR_LEVEL_PROGRESS(3),
		SPEAR_LEVEL_PROGRESS(4),
		SPEAR_LEVEL_PROGRESS(5),
		SPEAR_LEVEL_PROGRESS(6),
		SPEAR_LEVEL_PROGRESS(7),
		SPEAR_LEVEL_PROGRESS(8),
		SPEAR_LEVEL_PROGRESS(9),
		SPEAR_LEVEL_PROGRESS(10),
		SPEAR_LEVEL_PROGRESS(11),
		SPEAR_LEVEL_PROGRESS(12),
		SPEAR_LEVEL_PROGRESS(13),
		SPEAR_LEVEL_PROGRESS(14),
		SPEAR_LEVEL_PROGRESS(15),
		SPEAR_LEVEL_PROGRESS(16),
		SPEAR_LEVEL_PROGRESS(17),
		SPEAR_LEVEL_PROGRESS(18),
		SPEAR_LEVEL_PROGRESS(19),
		SPEAR_LEVEL_PROGRESS(20),
		SPEAR_LEVEL_PROGRESS(21),
		SPEAR_LEVEL_PROGRESS(22),
		SPEAR_LEVEL_PROGRESS(23),
		SPEAR_LEVEL_PROGRESS(24),
		SPEAR_LEVEL_PROGRESS(25),
		SPEAR_LEVEL_PROGRESS(26),
		SPEAR_LEVEL_PROGRESS(27),
		SPEAR_LEVEL_PROGRESS(28),
		SPEAR_LEVEL_PROGRESS(29),
		SPEAR_LEVEL_PROGRESS(30),
		SPEAR_LEVEL_PROGRESS(31),
		SPEAR_LEVEL_PROGRESS(32),
		SPEAR_LEVEL_PROGRESS(33),
		SPEAR_LEVEL_PROGRESS(34),
		SPEAR_LEVEL_PROGRESS(35),
		SPEAR_LEVEL_PROGRESS(36),
		SPEAR_LEVEL_PROGRESS(37),
		SPEAR_LEVEL_PROGRESS(38),
		SPEAR_LEVEL_PROGRESS(39),
		SPEAR_LEVEL_PROGRESS(40),
		SPEAR_LEVEL_PROGRESS(41),
		SPEAR_LEVEL_PROGRESS(42),
		SPEAR_LEVEL_PROGRESS(43),
		SPEAR_LEVEL_PROGRESS(44),
		SPEAR_LEVEL_PROGRESS(45),
		SPEAR_LEVEL_PROGRESS(46),
		SPEAR_LEVEL_PROGRESS(47),
		SPEAR_LEVEL_PROGRESS(48),
		SPEAR_LEVEL_PROGRESS(49),
		SPEAR_LEVEL_PROGRESS(50),
		SPEAR_LEVEL_PROGRESS(51),
		SPEAR_LEVEL_PROGRESS(52),
		SPEAR_LEVEL_PROGRESS(53),
		SPEAR_LEVEL_PROGRESS(54),
		SPEAR_LEVEL_PROGRESS(55),
		SPEAR_LEVEL_PROGRESS(56),
		SPEAR_LEVEL_PROGRESS(57),
		SPEAR_LEVEL_PROGRESS(58),
		SPEAR_LEVEL_PROGRESS(59),
		SPEAR_LEVEL_PROGRESS(60),
		SPEAR_LEVEL_PROGRESS(61),
		SPEAR_LEVEL_PROGRESS(62),
		SPEAR_LEVEL_PROGRESS(63),
		SPEAR_LEVEL_PROGRESS(64),
		SPEAR_LEVEL_PROGRESS(65),
		SPEAR_LEVEL_PROGRESS(66),
		SPEAR_LEVEL_PROGRESS(67),
		SPEAR_LEVEL_PROGRESS(68),
		SPEAR_LEVEL_PROGRESS(69),
		SPEAR_LEVEL_PROGRESS(70),
		SPEAR_LEVEL_PROGRESS(71),
		SPEAR_LEVEL_PROGRESS(72),
		SPEAR_LEVEL_PROGRESS(73),
		SPEAR_LEVEL_PROGRESS(74),
		SPEAR_LEVEL_PROGRESS(75),
		SPEAR_LEVEL_PROGRESS(76),
		SPEAR_LEVEL_PROGRESS(77),
		SPEAR_LEVEL_PROGRESS(78),
		SPEAR_LEVEL_PROGRESS(79),
		SPEAR_LEVEL_PROGRESS(80),
		SPEAR_LEVEL_PROGRESS(81),
		SPEAR_LEVEL_PROGRESS(82),
		SPEAR_LEVEL_PROGRESS(83),
		SPEAR_LEVEL_PROGRESS(84),
		SPEAR_LEVEL_PROGRESS(85),
		SPEAR_LEVEL_PROGRESS(86),
		SPEAR_LEVEL_PROGRESS(87),
		SPEAR_LEVEL_PROGRESS(88),
		SPEAR_LEVEL_PROGRESS(89),
		SPEAR_LEVEL_PROGRESS(90),
		SPEAR_LEVEL_PROGRESS(91),
		SPEAR_LEVEL_PROGRESS(92),
		SPEAR_LEVEL_PROGRESS(93),
		SPEAR_LEVEL_PROGRESS(94),
		SPEAR_LEVEL_PROGRESS(95),
		SPEAR_LEVEL_PROGRESS(96),
		SPEAR_LEVEL_PROGRESS(97),
		SPEAR_LEVEL_PROGRESS(98),
		SPEAR_LEVEL_PROGRESS(99),
		SPEAR_LEVEL_PROGRESS(100),
	};
#undef SPEAR_LEVEL_PROGRESS

	static const SkillProgressionDefinition kSkillProgressions[kSkillCount] =
	{
		{ SpearSkillShared::kSpearSkillId, kDefaultSkillLevelProgression, kSkillProgressionLevelCount },
	};

	static const SkillUseCondition kSkillUseConditions[kSkillCount] =
	{
		{ SpearSkillShared::kWeaponSkill_Spear, kActorVal_Blade, kWeaponSuccessfulHitUseType },
	};

	static const MasteryPerkText kMasteryPerkTexts[kSkillCount] =
	{
		{
			kSpearApprenticePerkText,
			kSpearJourneymanPerkText,
			kSpearExpertPerkText,
			kSpearMasterPerkText,
		},
	};

	static SaveState g_state = {};
	static SpearSkillShared::WeaponTypeSidecarStore g_weaponTypeStore;
	static SpearSkillShared::NpcSpearStore g_npcSkillStore;
	static SpearSkillShared::NpcSpearTrainingStore g_npcTrainingStore;
	static bool g_npcSkillStoreConfigured = false;
	static StatsRow g_statsRows[kSkillCount] = {};
	static PickerRow g_pickerRows[kSkillCount] = {};
	static void* g_statsMenu = nullptr;
	static void* g_classPickerSkillsMenu = nullptr;
	static void* g_stagedClassMenu = nullptr;
	static bool g_stagedSelections[kSkillCount] = {};
	static UInt32 g_stagedForeignSyntheticSelectionCount = 0;
	static UInt32 g_stagedSelectedSyntheticSkillIds[kNativeClassMajorCount] = {};
	static UInt32 g_stagedSelectedSyntheticSkillCount = 0;
	static bool g_stagedUsed = false;
	static bool g_statsOrderingDirty = false;
	static bool g_insideStatsMenuCreateRows = false;
	static bool g_loggedStatsRowCreateFailure = false;
	static bool g_loggedLegacySelfWeaponTypeFallback = false;
	static bool g_loggedWeaponTypeCarrierStoreFull = false;
	static bool g_loggedLegacySelfNpcSkillFallback = false;
	static bool g_loggedNpcSkillCarrierStoreFull = false;
	static bool g_loggedNpcTrainingCarrierStoreFull = false;
	static UInt32 g_appliedPatches = 0;
	static UInt32 g_failedPatches = 0;
	static PendingWeaponSkillConsumer g_pendingWeaponSkillConsumer = {};
	static TrainingPolicyState g_trainingPolicyState = {};
	static TESNPC* g_weaponRatingNpcContext[kWeaponRatingNpcContextDepth] = {};
	static UInt32 g_weaponRatingNpcContextDepth = 0;
	static UInt32 g_weaponRatingNpcContextOverflowDepth = 0;
	static void* g_playerModExperienceOriginal = nullptr;
	static void* g_actorBaseGetEquippableItemRatingOriginal = nullptr;
	static void* g_equippableWeaponRatingSelectorOriginal = nullptr;
	static void* g_actorValueGetNameOriginal = nullptr;
	static void* g_getWeaponSkillAVOriginal = nullptr;
	static void* g_calcWeaponDamageOriginal = nullptr;
	static void* g_calcPowerAttackBonusOriginal = nullptr;
	static void* g_statsMenuDetailsOriginal = nullptr;
	static void* g_skillsMenuUpdateAcceptOriginal = nullptr;
	static void* g_skillsMenuDetailsOriginal = nullptr;
	static void* g_skillsMenuAcceptOriginal = nullptr;
	static void* g_trainingMenuOpenOriginal = nullptr;
	static void* g_trainingMenuButtonOriginal = nullptr;
	static bool g_skillsMenuAcceptChainedExisting = false;
	static UInt32 g_statsMenuCreateRowsOriginalTarget = kStatsMenuCreateRows;
	static UInt32 g_statsMenuRefreshOriginalTarget = kStatsMenuRefresh;
	static UInt32 g_skillsMenuPreselectOriginalTarget = kSkillsMenuPreselect;
	static UInt32 g_classMenuCommitOriginalTarget = kClassMenuCommit;
	static UInt32 g_classMenuRefreshDetailsOriginalTarget = kClassMenuRefreshDetails;
	static UInt32 g_weaponPerkMasteryOriginalTarget = kActorGetSkillMasteryLevel;
	static UInt32 g_weaponPerkBaseCalcOriginalTarget = kActorGetBaseCalcAVi;
	static UInt32 g_dialogueTrainingOffersServiceOriginalTarget = kTESAIFormOffersService;
	static UInt32 g_magicPopupEbpLabelOriginalTarget = kTileSetString;
	static UInt32 g_magicPopupEdiLabelOriginalTarget = kTileSetString;
	static bool g_hooksInstalled = false;
	static bool g_hookInstallAttempted = false;

	using PlayerModExperienceFn = void(__thiscall*)(PlayerCharacter* player, UInt32 actorValue, UInt32 useType, float baseDelta);
	using TESActorBaseGetEquippableItemRatingFn = double(__thiscall*)(TESForm* actorBase, TESForm* item);
	using StatsMenuCreateRowsFn = void(__thiscall*)(void* statsMenu);
	using StatsMenuRefreshFn = void(__thiscall*)(void* statsMenu, UInt32 actorValue);
	using MenuCreateTileFromTemplateFn = Tile*(__thiscall*)(void* menu, Tile* parent, const char* templateName, UInt32 unk);
	using TileSetFloatFn = void(__thiscall*)(Tile* tile, UInt32 trait, float value);
	using TileSetStringFn = void(__thiscall*)(Tile* tile, UInt32 trait, const char* value);
	using TileGetFloatFn = double(__thiscall*)(Tile* tile, UInt32 trait);
	using TileAnimateTraitFn = void(__thiscall*)(Tile* tile, UInt32 trait, float fromValue, float toValue, float duration);
	using TileGetParentMenuFn = void*(__thiscall*)(Tile* tile);
	using MenuGetOpenMenuTileFn = Tile*(__cdecl*)(UInt32 menuType);
	using TileGetGlobalValueFn = double(__thiscall*)(Tile* tile);
	using ActorGetGoldFn = int(__thiscall*)(Actor* actor);
	using ActorValueGetNameFn = const char*(__cdecl*)(UInt32 actorValue);
	using ActorValueGetIconFn = const char*(__cdecl*)(UInt32 actorValue);
	using CalcMasteryFromSkillFn = UInt32(__cdecl*)(SInt32 skillLevel);
	using ActorValueGetMasteryNameFn = const char*(__cdecl*)(UInt32 masteryLevel);
	using ActorGetBaseCalcAViFn = UInt32(__thiscall*)(Actor* actor, UInt32 actorValue);
	using ActorGetSkillMasteryLevelFn = UInt32(__thiscall*)(Actor* actor, UInt32 actorValue);
	using OpenSkillPerkMenuFn = char(__cdecl*)(const char* xml, UInt32 unk1, UInt32 unk2, UInt32 unk3, UInt32 firstArgType, ...);
	using TESObjectREFRGetAnimDataFn = ActorAnimData*(__thiscall*)(TESObjectREFR* refr);
	using ActorAnimDataRemovePowerAttackGroupsFn = void(__thiscall*)(ActorAnimData* animData);
	using ObservedActorAnimDataBuildPowerAttackKFListFn = void(__thiscall*)(ActorAnimData* animData, TESObjectREFR* refr, UInt32 unk);
	using GetWeaponSkillAVFn = UInt32(__thiscall*)(TESObjectWEAP* weapon);
	using CalcWeaponDamageFn = double(__cdecl*)(int weaponSkill, int luck, int strengthOrAgility, float fatigue, int weaponDamage, float condition, float multiplier, float ignoreFatigue);
	using CalcPowerAttackBonusFn = double(__cdecl*)(int skillLevel, int attackType);
	using SkillsMenuPreselectFn = void(__thiscall*)(void* skillsMenu);
	using SkillsMenuUpdateAcceptFn = void(__thiscall*)(void* skillsMenu);
	using SkillsMenuDetailsFn = void(__thiscall*)(void* skillsMenu, UInt32 value);
	using SkillsMenuAcceptFn = void(__thiscall*)(void* skillsMenu, UInt32 buttonId, Tile* tile);
	using SkillsMenuCreateSkillRowFn = Tile*(__thiscall*)(void* skillsMenu, const char* displayName, UInt32 rowValue);
	using SkillsMenuCloseFn = void(__cdecl*)();
	using ClassMenuCommitFn = void(__thiscall*)(void* classMenu);
	using ClassMenuRefreshDetailsFn = void(__thiscall*)(void* classMenu, void* displayedClass);
	using ClassMenuStepRefreshFn = void(__thiscall*)(void* classMenu);
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

	static StatsMenuCreateRowsFn StatsMenuCreateRowsOriginal()
	{
		return reinterpret_cast<StatsMenuCreateRowsFn>(g_statsMenuCreateRowsOriginalTarget);
	}

	static StatsMenuRefreshFn StatsMenuRefreshOriginal()
	{
		return reinterpret_cast<StatsMenuRefreshFn>(g_statsMenuRefreshOriginalTarget);
	}

	static MenuCreateTileFromTemplateFn MenuCreateTileFromTemplate()
	{
		return reinterpret_cast<MenuCreateTileFromTemplateFn>(kMenuCreateTileFromTemplate);
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
		if (g_actorValueGetNameOriginal)
			return reinterpret_cast<ActorValueGetNameFn>(g_actorValueGetNameOriginal);

		return reinterpret_cast<ActorValueGetNameFn>(kActorValueGetName);
	}

	static ActorValueGetIconFn ActorValueGetIcon()
	{
		return reinterpret_cast<ActorValueGetIconFn>(kActorValueGetIcon);
	}

	static ActorGetGoldFn ActorGetGold()
	{
		return reinterpret_cast<ActorGetGoldFn>(kActorGetGold);
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

	static OpenSkillPerkMenuFn OpenSkillPerkMenu()
	{
		return reinterpret_cast<OpenSkillPerkMenuFn>(kOpenSkillPerkMenu);
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

	static SkillsMenuPreselectFn SkillsMenuPreselectOriginal()
	{
		return reinterpret_cast<SkillsMenuPreselectFn>(g_skillsMenuPreselectOriginalTarget);
	}

	static SkillsMenuUpdateAcceptFn SkillsMenuUpdateAcceptOriginal()
	{
		return reinterpret_cast<SkillsMenuUpdateAcceptFn>(g_skillsMenuUpdateAcceptOriginal);
	}

	static SkillsMenuDetailsFn SkillsMenuDetailsOriginal()
	{
		return reinterpret_cast<SkillsMenuDetailsFn>(g_skillsMenuDetailsOriginal);
	}

	static SkillsMenuAcceptFn SkillsMenuAcceptOriginal()
	{
		return reinterpret_cast<SkillsMenuAcceptFn>(g_skillsMenuAcceptOriginal);
	}

	static SkillsMenuCreateSkillRowFn SkillsMenuCreateSkillRow()
	{
		return reinterpret_cast<SkillsMenuCreateSkillRowFn>(kSkillsMenuCreateSkillRow);
	}

	static SkillsMenuCloseFn SkillsMenuClose()
	{
		return reinterpret_cast<SkillsMenuCloseFn>(kSkillsMenuClose);
	}

	static ClassMenuCommitFn ClassMenuCommitOriginal()
	{
		return reinterpret_cast<ClassMenuCommitFn>(g_classMenuCommitOriginalTarget);
	}

	static ClassMenuRefreshDetailsFn ClassMenuRefreshDetailsOriginal()
	{
		return reinterpret_cast<ClassMenuRefreshDetailsFn>(g_classMenuRefreshDetailsOriginalTarget);
	}

	static ClassMenuStepRefreshFn ClassMenuStepRefresh()
	{
		return reinterpret_cast<ClassMenuStepRefreshFn>(kClassMenuStepRefresh);
	}

	static TESAIFormOffersServiceFn DialogueTrainingOffersServiceOriginal()
	{
		return reinterpret_cast<TESAIFormOffersServiceFn>(g_dialogueTrainingOffersServiceOriginalTarget);
	}

	static PlayerMaybeStartNextAttributeBonusBucketFn PlayerMaybeStartNextAttributeBonusBucket()
	{
		return reinterpret_cast<PlayerMaybeStartNextAttributeBonusBucketFn>(kPlayerMaybeStartNextAttributeBonusBucket);
	}

	static PlayerIncrementAttributeBonusBucketFn PlayerIncrementAttributeBonusBucket()
	{
		return reinterpret_cast<PlayerIncrementAttributeBonusBucketFn>(kPlayerIncrementAttributeBonusBucket);
	}

	static TrainingMenuCloseFn TrainingMenuClose()
	{
		return reinterpret_cast<TrainingMenuCloseFn>(kTrainingMenuClose);
	}

	static float GetGameSettingFloatOrDefault(const char* name, float fallback)
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>(name), &setting) && setting && std::isfinite(setting->f))
			return setting->f;

		return fallback;
	}

	static UInt32 GetGameSettingUIntOrDefault(const char* name, UInt32 fallback)
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>(name), &setting) && setting && setting->i >= 0)
			return static_cast<UInt32>(setting->i);

		return fallback;
	}

	static UInt32 GetLevelUpSkillCount()
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>("iLevelUpSkillCount"), &setting) && setting && setting->i > 0)
			return static_cast<UInt32>(setting->i);

		return 10;
	}

	static bool SkillClassUsesSpecialization(const SkillDefinition& skill)
	{
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return false;

		TESClass* playerClass = player->GetPlayerClass();
		return playerClass && playerClass->specialization == skill.specialization &&
			skill.governingAttributeAV <= kActorVal_Luck;
	}

	static bool PlayerClassHasNativeMajor(UInt32 actorValue)
	{
		PlayerCharacter* player = GetPlayer();
		TESClass* playerClass = player ? player->GetPlayerClass() : nullptr;
		if (!playerClass)
			return false;

		for (UInt32 i = 0; i < kNativeClassMajorCount; ++i)
		{
			if (playerClass->majorSkills[i] == actorValue)
				return true;
		}
		return false;
	}

	static bool InheritsLegacyNativeMajor(UInt32 index)
	{
		return false;
	}

	static bool IsEffectiveMajor(UInt32 index)
	{
		return index < kSkillCount && (g_state.states[index].major || InheritsLegacyNativeMajor(index));
	}

	static UInt32 ClampProgressionLevel(UInt32 level)
	{
		if (!level)
			return 1;
		if (level > kMaxSkillLevel)
			return kMaxSkillLevel;
		return level;
	}

	static const SkillLevelProgression& GetSkillLevelProgression(UInt32 skillIndex, UInt32 level)
	{
		static const SkillLevelProgression fallbackProgression =
		{
			1,
			1.0f,
			1.0f,
			kSkillProgressionPlaceholderDescription
		};

		if (skillIndex >= kSkillCount)
			return fallbackProgression;

		const SkillProgressionDefinition& progression = kSkillProgressions[skillIndex];
		const UInt32 clampedLevel = ClampProgressionLevel(level);
		if (progression.levels && progression.levelCount > clampedLevel)
			return progression.levels[clampedLevel];

		return fallbackProgression;
	}

	static float RequiredProgressForLevel(UInt32 skillIndex, UInt32 level)
	{
		if (skillIndex >= kSkillCount)
			return 1.0f;

		const SkillLevelProgression& progression = GetSkillLevelProgression(skillIndex, level);
		const float atLevel = progression.scalingValue > 0.0f ? progression.scalingValue : static_cast<float>(level ? level : 1);
		const float skillUseFactor = GetGameSettingFloatOrDefault("fSkillUseFactor", 1.0f);
		const float skillUseExp = GetGameSettingFloatOrDefault("fSkillUseExp", 1.0f);
		const float skillUseMajorMult = GetGameSettingFloatOrDefault("fSkillUseMajorMult", 1.0f);
		const float skillUseMinorMult = GetGameSettingFloatOrDefault("fSkillUseMinorMult", 1.0f);
		const float skillUseSpecMult = GetGameSettingFloatOrDefault("fSkillUseSpecMult", 1.0f);
		const float classMultiplier = IsEffectiveMajor(skillIndex) ? skillUseMajorMult : skillUseMinorMult;
		const float specMultiplier = SkillClassUsesSpecialization(kSkills[skillIndex]) ? skillUseSpecMult : 1.0f;
		const double requirement =
			std::pow(static_cast<double>(skillUseFactor) * atLevel, static_cast<double>(skillUseExp)) *
			static_cast<double>(classMultiplier) *
			static_cast<double>(specMultiplier);

		if (std::isfinite(requirement) && requirement > 0.0)
			return static_cast<float>(requirement);

		return 1.0f + atLevel;
	}

	static void NormalizeState(UInt32 index)
	{
		SkillState& state = g_state.states[index];
		if (state.level > kMaxSkillLevel)
			state.level = kMaxSkillLevel;
		if (!std::isfinite(state.progress) || state.progress < 0.0f)
			state.progress = 0.0f;
		state.requiredProgress = RequiredProgressForLevel(index, state.level);
		if (!std::isfinite(state.requiredProgress) || state.requiredProgress <= 0.0f)
			state.requiredProgress = 1.0f;
		if (state.level >= kMaxSkillLevel)
			state.progress = 0.0f;
		state.major = state.major ? 1 : 0;
	}

	static void ResetState()
	{
		std::memset(&g_state, 0, sizeof(g_state));
		g_state.version = kSaveVersion;
		g_stagedClassMenu = nullptr;
		std::memset(g_stagedSelections, 0, sizeof(g_stagedSelections));
		g_stagedForeignSyntheticSelectionCount = 0;
		std::memset(g_stagedSelectedSyntheticSkillIds, 0, sizeof(g_stagedSelectedSyntheticSkillIds));
		g_stagedSelectedSyntheticSkillCount = 0;
		g_stagedUsed = false;
		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			g_state.states[i].level = 5;
			g_state.states[i].requiredProgress = 1.0f;
		}
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

	static Tile* GetStatsMenuSummaryTile(void* statsMenu)
	{
		return GetMenuTileAtOffset(statsMenu, kStatsMenuSummaryTileOffset);
	}

	static Tile* GetStatsMenuSkillParent(void* statsMenu)
	{
		return GetMenuTileAtOffset(statsMenu, kStatsMenuSkillParentOffset);
	}

	static Tile* GetStatsMenuFocusTile(void* statsMenu)
	{
		return GetMenuTileAtOffset(statsMenu, kStatsMenuFocusTileOffset);
	}

	static Tile* GetStatsMenuDetailTile(void* statsMenu)
	{
		return GetMenuTileAtOffset(statsMenu, kStatsMenuDetailTileOffset);
	}

	static Tile* GetNativeStatsRow(void* statsMenu, UInt32 nativeSkillOffset)
	{
		if (!statsMenu || nativeSkillOffset >= kNativeSkillCount)
			return nullptr;

		return *reinterpret_cast<Tile**>(
			reinterpret_cast<UInt8*>(statsMenu) + kStatsMenuSkillRowsOffset + nativeSkillOffset * sizeof(Tile*));
	}

	static bool HasNativeStatsRows(void* statsMenu)
	{
		return GetNativeStatsRow(statsMenu, 0) != nullptr;
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

	static const char* __cdecl HookActorValueGetName(UInt32 actorValue)
	{
		if (ActorValueGetNameFn original = ActorValueGetName())
			return original(actorValue);

		return "";
	}

	static const char* GetSafeActorValueIcon(UInt32 actorValue)
	{
		if (ActorValueGetIconFn getIcon = ActorValueGetIcon())
		{
			if (const char* icon = getIcon(actorValue))
			{
				if (icon[0])
					return icon;
			}
		}

		return "";
	}

	static bool LooseFileExists(const char* path)
	{
		if (!path || !path[0])
			return false;

		const DWORD attributes = GetFileAttributesA(path);
		return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	static bool LooseTextureAssetExists(const char* texturePath)
	{
		if (!texturePath || !texturePath[0])
			return false;

		char path[MAX_PATH] = {};
		_snprintf_s(path, sizeof(path), _TRUNCATE, "Data\\Textures\\%s", texturePath);
		if (LooseFileExists(path))
			return true;

		_snprintf_s(path, sizeof(path), _TRUNCATE, "Textures\\%s", texturePath);
		if (LooseFileExists(path))
			return true;

		_snprintf_s(path, sizeof(path), _TRUNCATE, "Data\\%s", texturePath);
		if (LooseFileExists(path))
			return true;

		return LooseFileExists(texturePath);
	}

	static const char* GetSidecarSkillFallbackIcon(UInt32 index)
	{
		if (index < kSkillCount && kSkills[index].fallbackIconPath && kSkills[index].fallbackIconPath[0])
			return kSkills[index].fallbackIconPath;

		return "";
	}

	static const char* GetSidecarSkillIcon(UInt32 index)
	{
		if (index < kSkillCount && kSkills[index].iconPath && LooseTextureAssetExists(kSkills[index].iconPath))
			return kSkills[index].iconPath;

		return GetSidecarSkillFallbackIcon(index);
	}

	static const char* GetSidecarSkillRowIcon(UInt32 index)
	{
		if (index < kSkillCount && kSkills[index].rowIconPath && LooseTextureAssetExists(kSkills[index].rowIconPath))
			return kSkills[index].rowIconPath;

		if (index < kSkillCount && kSkills[index].fallbackRowIconPath && kSkills[index].fallbackRowIconPath[0])
			return kSkills[index].fallbackRowIconPath;

		return GetSidecarSkillIcon(index);
	}

	static const char* GetSidecarMasteryPerkText(UInt32 index, UInt32 mastery)
	{
		if (index >= kSkillCount || mastery == 0 || mastery >= kStatsMenuMasteryRankCount)
			return kSkillProgressionPlaceholderDescription;

		const MasteryPerkText& text = kMasteryPerkTexts[index];
		switch (mastery)
		{
			case 1:
				return text.apprentice;
			case 2:
				return text.journeyman;
			case 3:
				return text.expert;
			case 4:
				return text.master;
			default:
				return kSkillProgressionPlaceholderDescription;
		}
	}

	static const char* GetSpearUpgradeDescription(UInt32 level)
	{
		switch (level)
		{
			case 25:
				return kSpearApprenticeUpgradeDescription;
			case 50:
				return kSpearJourneymanUpgradeDescription;
			case 75:
				return kSpearExpertUpgradeDescription;
			case 100:
				return kSpearMasterUpgradeDescription;
			default:
				return kSkillProgressionPlaceholderDescription;
		}
	}

	static const char* GetSkillUpgradeDescription(UInt32 index, const SkillLevelProgression& progression)
	{
		if (index == kSpearSkillIndex)
			return GetSpearUpgradeDescription(progression.level);

		return progression.upgradeDescription ? progression.upgradeDescription : kSkillProgressionPlaceholderDescription;
	}

	static bool ShowSidecarMasteryPerkPopup(UInt32 index, UInt32 mastery)
	{
		if (index >= kSkillCount || mastery == 0 || mastery >= kStatsMenuMasteryRankCount)
			return false;

		const char* icon = GetSidecarSkillIcon(index);
		const char* description = GetSidecarMasteryPerkText(index, mastery);
		if (!icon || !icon[0] || !description || !description[0])
			return false;

		return OpenSkillPerkMenu()(kSkillPerkMenuXml,
			0,
			1,
			0,
			kGenericMenuArgString,
			icon,
			kGenericMenuArgString,
			description,
			kGenericMenuArgString,
			kSkillPerkOkText,
			kGenericMenuArgEnd) != 0;
	}

	static const char* GetSafeMasteryName(UInt32 level)
	{
		if (const char* name = ActorValueGetMasteryName()(CalcMasteryFromSkill()(static_cast<SInt32>(level))))
			return name;

		return "";
	}

	static const char* GetSpearRankSummaryForMastery(UInt32 mastery)
	{
		switch (mastery)
		{
			case 0:
				return kSpearNoviceRankSummary;
			case 1:
				return kSpearApprenticeRankSummary;
			case 2:
				return kSpearJourneymanRankSummary;
			case 3:
				return kSpearExpertRankSummary;
			case 4:
				return kSpearMasterRankSummary;
			default:
				return "";
		}
	}

	static const char* GetSpearRankDescriptionForMastery(UInt32 mastery)
	{
		switch (mastery)
		{
			case 0:
				return kSpearNoviceRankDescription;
			case 1:
				return kSpearApprenticeRankDescription;
			case 2:
				return kSpearJourneymanRankDescription;
			case 3:
				return kSpearExpertRankDescription;
			case 4:
				return kSpearMasterRankDescription;
			default:
				return "";
		}
	}

	static const char* GetSidecarRankSummaryForMastery(UInt32 index, UInt32 mastery)
	{
		if (index == kSpearSkillIndex)
			return GetSpearRankSummaryForMastery(mastery);

		return "";
	}

	static const char* GetSidecarRankDescriptionForMastery(UInt32 index, UInt32 mastery)
	{
		if (index == kSpearSkillIndex)
			return GetSpearRankDescriptionForMastery(mastery);

		return "";
	}

	static float GetProgressFraction(UInt32 index)
	{
		const SkillState& state = g_state.states[index];
		if (state.level >= kMaxSkillLevel || state.requiredProgress <= 0.0f)
			return 0.0f;

		const float fraction = state.progress / state.requiredProgress;
		if (!std::isfinite(fraction) || fraction < 0.0f)
			return 0.0f;
		if (fraction > 1.0f)
			return 1.0f;
		return fraction;
	}

	static void RefreshSidecarSkillDisplay(UInt32 index);
	static void RefreshPlayerWeaponSidecarPowerAttackGroups();
	static void SetSpearProgress(float progress);
	static void ModSpearSkill(SInt32 delta);

	static void MirrorLevelUpSideEffects(UInt32 index, UInt32 levelUps)
	{
		if (!levelUps)
			return;

		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;

		for (UInt32 i = 0; i < levelUps; ++i)
		{
			if (kSkills[index].governingAttributeAV <= kActorVal_Luck)
				PlayerIncrementAttributeBonusBucket()(player, kSkills[index].governingAttributeAV);

			if (IsEffectiveMajor(index))
			{
				++player->majorSkillAdvances;
				if (GetLevelUpSkillCount())
					PlayerMaybeStartNextAttributeBonusBucket()(player);

				if (GetLevelUpSkillCount() && player->majorSkillAdvances >= GetLevelUpSkillCount())
					player->bCanLevelUp = 1;
			}
		}
	}

	static void NotifyLevelIncrease(UInt32 index, UInt32 previousLevel, UInt32 levelUps)
	{
		if (!levelUps)
			return;

		char message[256] = {};
		_snprintf_s(message, sizeof(message), _TRUNCATE, "Your %s skill increased to %u.",
			kSkills[index].name,
			g_state.states[index].level);
		QueueUIMessage(message, 0, 1, 2.0f);

		const UInt32 previousMastery = CalcMasteryFromSkill()(static_cast<SInt32>(previousLevel));
		const UInt32 newMastery = CalcMasteryFromSkill()(static_cast<SInt32>(g_state.states[index].level));
		if (newMastery > previousMastery && newMastery < kStatsMenuMasteryRankCount)
		{
			_snprintf_s(message, sizeof(message), _TRUNCATE, "You are now a %s in %s.",
				GetSafeMasteryName(g_state.states[index].level),
				kSkills[index].name);
			QueueUIMessage(message, 0, 1, 4.0f);
			ShowSidecarMasteryPerkPopup(index, newMastery);
			RefreshPlayerWeaponSidecarPowerAttackGroups();
		}
	}

	static bool AddSkillProgress(UInt32 index, float progressDelta)
	{
		if (index >= kSkillCount || !std::isfinite(progressDelta))
			return false;

		NormalizeState(index);
		SkillState& state = g_state.states[index];
		if (state.level >= kMaxSkillLevel)
			return true;

		const UInt32 previousLevel = state.level;
		UInt32 levelUps = 0;
		state.progress += progressDelta;
		if (!std::isfinite(state.progress) || state.progress < 0.0f)
			state.progress = 0.0f;

		while (state.level < kMaxSkillLevel && state.progress + kProgressEpsilon >= state.requiredProgress)
		{
			state.progress -= state.requiredProgress;
			++state.level;
			++state.levelUps;
			++state.governingAttributeIncreaseCount;
			++levelUps;
			state.requiredProgress = RequiredProgressForLevel(index, state.level);
		}

		if (state.level >= kMaxSkillLevel)
			state.progress = 0.0f;

		if (levelUps)
		{
			MirrorLevelUpSideEffects(index, levelUps);
			NotifyLevelIncrease(index, previousLevel, levelUps);
		}
		RefreshSidecarSkillDisplay(index);
		return true;
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

		// NPC Spear state is keyed by base TESNPC form. That is safe for
		// authored/base item-rating lookups, but not for live progression.
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

	static void LoadEditorNpcSpearTrainingSidecars(bool preserveExistingIfNoCarriers)
	{
		if (!g_dataHandler || !*g_dataHandler || !(*g_dataHandler)->boundObjects)
		{
			if (!preserveExistingIfNoCarriers)
				g_npcTrainingStore.Clear();
			return;
		}

		SpearSkillShared::NpcSpearTrainingStore importedStore;
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
				if (!SpearSkillShared::NpcSpearTrainingPayloadCodec::LooksLikePayload(payload))
					continue;

				++totals.carrierRecords;
				std::vector<SpearSkillShared::NpcSpearTrainingPayloadRow> rows;
				SpearSkillShared::NpcSpearPayloadStats stats = {};
				if (!SpearSkillShared::NpcSpearTrainingPayloadCodec::Parse(payload, rows, &stats, NpcSkillStoreLog, nullptr))
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

					if (importedStore.SetLoaded(resolvedFormId))
					{
						++totals.resolvedEntries;
					}
					else
					{
						++totals.unresolvedEntries;
						if (!g_loggedNpcTrainingCarrierStoreFull)
						{
							_WARNING("SpearSkill: embedded NPC Spear training carrier row could not be stored form=%08X", resolvedFormId);
							g_loggedNpcTrainingCarrierStoreFull = true;
						}
					}
				}
			}
		}

		if (totals.carrierRecords || !preserveExistingIfNoCarriers)
		{
			g_npcTrainingStore.Clear();
			for (UInt32 i = 0; i < importedStore.Count(); ++i)
				g_npcTrainingStore.SetLoaded(importedStore.EntryAt(i).formId);
		}

		g_npcTrainingStore.ClearDirty();
		_MESSAGE("SpearSkill: embedded NPC Spear training carriers=%u parsed=%u resolved=%u unresolved=%u skipped=%u",
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

		NormalizeState(index);
		if (outIndex)
			*outIndex = index;
		if (outLevel)
			*outLevel = g_state.states[index].level;
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

		NormalizeState(index);
		if (outLevel)
			*outLevel = g_state.states[index].level;
		return true;
	}

	static UInt32 GetCurrentActorValue(Actor* actor, UInt32 actorValue)
	{
		if (!actor)
			return 0;

		return actor->GetActorValue(actorValue);
	}

	static UInt32 __cdecl GetCombatScoringWeaponSkillLevel(Actor* actor, TESObjectWEAP* weapon)
	{
		const UInt32 nativeActorValue = NativeWeaponSkillAV(weapon);
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevelForWeapon(actor, weapon, nativeActorValue, &sidecarLevel))
			return sidecarLevel;

		return GetCurrentActorValue(actor, nativeActorValue);
	}

	static UInt32 __cdecl GetCombatSelectionActorValueSkill(Actor* actor, UInt32 actorValue)
	{
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevel(actor, actorValue, nullptr, &sidecarLevel))
			return sidecarLevel;

		return GetCurrentActorValue(actor, actorValue);
	}

	static __declspec(naked) void HookCombatControllerWeaponSkillLevel()
	{
		__asm
		{
			push ecx
			push ebx
			call GetCombatScoringWeaponSkillLevel
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
			call GetCombatSelectionActorValueSkill
			add esp, 8
			mov ebx, eax
			mov edx, [esi]
			mov eax, [edx+284h]
			push 11h
			mov ecx, esi
			call eax
			cmp eax, ebx
			jle keepCandidate
			mov edx, kCombatSelectionHandToHandPreferred
			jmp edx
keepCandidate:
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
			push dword ptr [esp + 0x10]
			push dword ptr [esp + 0x10]
			push ecx
			call TrySetMagicPopupWeaponTypeLabel
			add esp, 0x10
			pop ecx
			test al, al
			jz chainOriginal
			ret 0x08
		chainOriginal:
			jmp dword ptr [g_magicPopupEdiLabelOriginalTarget]
		}
	}

	static __declspec(naked) void HookMagicPopupWeaponTypeLabelSetStringFromEbp()
	{
		__asm
		{
			push ecx
			push ebp
			push dword ptr [esp + 0x10]
			push dword ptr [esp + 0x10]
			push ecx
			call TrySetMagicPopupWeaponTypeLabel
			add esp, 0x10
			pop ecx
			test al, al
			jz chainOriginal
			ret 0x08
		chainOriginal:
			jmp dword ptr [g_magicPopupEbpLabelOriginalTarget]
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
			push dword ptr [esp + 36]
			call PushBaseWeaponRatingContext
			add esp, 4
			popad

			push dword ptr [esp + 16]
			push dword ptr [esp + 16]
			push dword ptr [esp + 16]
			push dword ptr [esp + 16]
			call dword ptr [g_equippableWeaponRatingSelectorOriginal]

			push eax
			pushad
			call PopWeaponRatingContext
			popad
			pop eax
			ret 16
		}
	}

	static void ClearTrainingPolicyState()
	{
		std::memset(&g_trainingPolicyState, 0, sizeof(g_trainingPolicyState));
	}

	static UInt32 GetTrainingSessionLimit()
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>("iTrainingSkills"), &setting) && setting && setting->i > 0)
			return static_cast<UInt32>(setting->i);

		return 5;
	}

	static float GetTrainingCostMultiplier()
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>("fTrainingCostMult"), &setting) &&
			setting &&
			std::isfinite(setting->f) &&
			setting->f > 0.0f)
		{
			return setting->f;
		}

		return 10.0f;
	}

	static UInt32 GetPlayerGoldCount()
	{
		PlayerCharacter* player = GetPlayer();
		return player ? static_cast<UInt32>(ActorGetGold()(player)) : 0;
	}

	static UInt32 GetSpearTrainingCost()
	{
		NormalizeState(kSpearSkillIndex);
		const float cost = static_cast<float>(g_state.states[kSpearSkillIndex].level) * GetTrainingCostMultiplier();
		if (!std::isfinite(cost) || cost <= 0.0f)
			return 0;

		return static_cast<UInt32>(cost + 0.5f);
	}

	static void* GetOpenTrainingMenu()
	{
		Tile* openTile = MenuGetOpenMenuTile()(0x404);
		return openTile ? TileGetParentMenu()(openTile) : nullptr;
	}

	static Actor* GetTrainingMenuTrainer(void* trainingMenu)
	{
		return trainingMenu ?
			*reinterpret_cast<Actor**>(reinterpret_cast<UInt8*>(trainingMenu) + kTrainingMenuTrainerOffset) :
			nullptr;
	}

	static UInt32 GetTrainingMenuTrainerLevel(void* trainingMenu)
	{
		return trainingMenu ?
			*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(trainingMenu) + kTrainingMenuTrainerLevelOffset) :
			0;
	}

	static UInt32 GetEffectiveSpearTrainerLevel(TESNPC* trainerBase, UInt32 nativeTrainerLevel, bool* outUsedNpcSidecar)
	{
		if (outUsedNpcSidecar)
			*outUsedNpcSidecar = false;

		UInt32 npcSpearLevel = 0;
		if (TryGetNpcSpearSkill(trainerBase, &npcSpearLevel) && npcSpearLevel > 0)
		{
			if (outUsedNpcSidecar)
				*outUsedNpcSidecar = true;
			return npcSpearLevel > kMaxSkillLevel ? kMaxSkillLevel : npcSpearLevel;
		}

		if (nativeTrainerLevel > 0)
			return nativeTrainerLevel > kMaxSkillLevel ? kMaxSkillLevel : nativeTrainerLevel;

		// A sidecar-authored Spear trainer can have no usable native skill cap
		// because Spear is not a TESAIForm/native actor value. Treat that as a
		// max-level sidecar trainer instead of inheriting vanilla's hard denial.
		return kMaxSkillLevel;
	}

	static TESNPC* GetSpearTrainingNpc(Actor* trainer)
	{
		if (!trainer)
			return nullptr;

		TESNPC* npc = AsNpcForm(trainer->baseForm);
		return npc && g_npcTrainingStore.Has(npc->refID) ? npc : nullptr;
	}

	static TESNPC* GetSpearTrainingNpcFromAiForm(void* aiForm)
	{
		if (!aiForm)
			return nullptr;

		TESNPC* npc = AsNpcForm(reinterpret_cast<TESForm*>(reinterpret_cast<UInt8*>(aiForm) - kRuntimeActorBaseAiFormOffset));
		return npc && g_npcTrainingStore.Has(npc->refID) ? npc : nullptr;
	}

	static bool __fastcall HookDialogueTrainingOffersService(void* aiForm, void*, UInt32 serviceMask)
	{
		TESAIFormOffersServiceFn original = DialogueTrainingOffersServiceOriginal();
		const bool nativeOffersService = original ? original(aiForm, serviceMask) : false;
		if (nativeOffersService || serviceMask != kTrainingServiceMask)
			return nativeOffersService;

		return GetSpearTrainingNpcFromAiForm(aiForm) != nullptr;
	}

	static bool TrainingPolicyCanPurchase(UInt32 trainerLevel, UInt32 cost)
	{
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return false;

		NormalizeState(kSpearSkillIndex);
		const SkillState& state = g_state.states[kSpearSkillIndex];
		if (state.level >= trainerLevel || state.level >= kMaxSkillLevel)
			return false;
		if (player->trainingSessionsUsed >= GetTrainingSessionLimit())
			return false;

		return cost <= GetPlayerGoldCount();
	}

	static void ApplySpearTrainingMenuDisplay(void* trainingMenu)
	{
		ClearTrainingPolicyState();
		if (!trainingMenu)
			return;

		TESNPC* trainerBase = GetSpearTrainingNpc(GetTrainingMenuTrainer(trainingMenu));
		if (!trainerBase)
			return;

		const UInt32 nativeTrainerLevel = GetTrainingMenuTrainerLevel(trainingMenu);
		bool usedNpcSidecarLevel = false;
		const UInt32 trainerLevel = GetEffectiveSpearTrainerLevel(trainerBase, nativeTrainerLevel, &usedNpcSidecarLevel);
		const UInt32 cost = GetSpearTrainingCost();
		*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(trainingMenu) + kTrainingMenuTrainerLevelOffset) = trainerLevel;
		*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(trainingMenu) + kTrainingMenuCostOffset) = cost;

		g_trainingPolicyState.menu = trainingMenu;
		g_trainingPolicyState.trainerBase = trainerBase;
		g_trainingPolicyState.trainerLevel = trainerLevel;
		g_trainingPolicyState.cost = cost;
		g_trainingPolicyState.used = true;

		if (Tile* skillName = GetMenuTileAtOffset(trainingMenu, kTrainingMenuSkillNameTileOffset))
			SetTileString(skillName, kTileValue_string, kSpearSkillDisplayName);
		if (Tile* icon = GetMenuTileAtOffset(trainingMenu, kTrainingMenuIconTileOffset))
			SetTileString(icon, kTileValue_filename, GetSidecarSkillIcon(kSpearSkillIndex));
		if (Tile* costTile = GetMenuTileAtOffset(trainingMenu, kTrainingMenuCostTileOffset))
		{
			char costText[64] = {};
			_snprintf_s(costText, sizeof(costText), _TRUNCATE, "Cost: %u", cost);
			SetTileString(costTile, kTileValue_string, costText);
		}

		const bool canPurchase = TrainingPolicyCanPurchase(trainerLevel, cost);
		if (Tile* acceptTile = GetMenuTileAtOffset(trainingMenu, kTrainingMenuAcceptTileOffset))
			SetTileFloat(acceptTile, kTileValue_visible, canPurchase ? 2.0f : 1.0f);
		if (Tile* disabledReason = GetMenuTileAtOffset(trainingMenu, kTrainingMenuDisabledReasonTileOffset))
		{
			SetTileFloat(disabledReason, kTileValue_visible, canPurchase ? 1.0f : 2.0f);
			if (!canPurchase)
			{
				const char* reason = cost > GetPlayerGoldCount() ?
					"You do not have enough gold." :
					"You cannot train this skill further right now.";
				SetTileString(disabledReason, kTileValue_string, reason);
			}
		}

		_MESSAGE("SpearSkill: mapped trainer %08X to Spear training level=%u trainerLevel=%u nativeTrainerLevel=%u trainerLevelSource=%s cost=%u",
			trainerBase ? trainerBase->refID : 0,
			g_state.states[kSpearSkillIndex].level,
			trainerLevel,
			nativeTrainerLevel,
			usedNpcSidecarLevel ? "npc-sidecar" : (nativeTrainerLevel ? "native-ai" : "sidecar-default"),
			cost);
	}

	static void __cdecl ApplySpearTrainingMenuAfterOpen(Tile* menuTile)
	{
		void* trainingMenu = menuTile ? TileGetParentMenu()(menuTile) : GetOpenTrainingMenu();
		ApplySpearTrainingMenuDisplay(trainingMenu);
	}

	static void RemovePlayerGold(UInt32 cost)
	{
		if (!cost)
			return;

		PlayerCharacter* player = GetPlayer();
		TESForm* gold = LookupFormByID(kGoldFormId);
		if (player && gold)
			player->RemoveItem(gold, nullptr, cost, 0, 0, nullptr, 0, 0, 1, 0);
	}

	static bool __cdecl HandleSpearTrainingMenuButton(UInt32 buttonId)
	{
		void* trainingMenu = GetOpenTrainingMenu();
		if (!g_trainingPolicyState.used || g_trainingPolicyState.menu != trainingMenu)
			ApplySpearTrainingMenuDisplay(trainingMenu);

		const bool mapped = g_trainingPolicyState.used && g_trainingPolicyState.menu == trainingMenu;
		if (!mapped)
		{
			_MESSAGE("SpearSkill: TrainingMenu button %u falling through to native handler; no Spear mapping is active",
				buttonId);
			return false;
		}

		_MESSAGE("SpearSkill: mapped TrainingMenu button %u selected openMenu=%p trainerLevel=%u cost=%u",
			buttonId,
			trainingMenu,
			g_trainingPolicyState.trainerLevel,
			g_trainingPolicyState.cost);

		if (buttonId == kTrainingMenuCloseButton)
		{
			_MESSAGE("SpearSkill: closing mapped Spear TrainingMenu via button %u", buttonId);
			ClearTrainingPolicyState();
			TrainingMenuClose()();
			return true;
		}

		if (buttonId != kTrainingMenuTrainButton)
		{
			_MESSAGE("SpearSkill: swallowed mapped Spear TrainingMenu button %u to avoid native/chained trampoline fallthrough",
				buttonId);
			return true;
		}

		if (!TrainingPolicyCanPurchase(g_trainingPolicyState.trainerLevel, g_trainingPolicyState.cost))
		{
			_MESSAGE("SpearSkill: Spear training denied level=%u trainerLevel=%u cost=%u sessions=%u",
				g_state.states[kSpearSkillIndex].level,
				g_trainingPolicyState.trainerLevel,
				g_trainingPolicyState.cost,
				GetPlayer() ? GetPlayer()->trainingSessionsUsed : 0);
			QueueUIMessage("You cannot train this skill right now.", 0, 1, 2.0f);
			return true;
		}

		NormalizeState(kSpearSkillIndex);
		const UInt32 previousLevel = g_state.states[kSpearSkillIndex].level;
		SetSpearProgress(0.0f);
		ModSpearSkill(1);
		if (g_state.states[kSpearSkillIndex].level <= previousLevel)
		{
			QueueUIMessage("Training failed.", 0, 1, 2.0f);
			return true;
		}

		PlayerCharacter* player = GetPlayer();
		if (player)
		{
			++player->trainingSessionsUsed;
			++player->miscStats[3];
		}
		RemovePlayerGold(g_trainingPolicyState.cost);

		_MESSAGE("SpearSkill: Spear training purchased newLevel=%u cost=%u sessions=%u",
			g_state.states[kSpearSkillIndex].level,
			g_trainingPolicyState.cost,
			player ? player->trainingSessionsUsed : 0);

		ClearTrainingPolicyState();
		TrainingMenuClose()();
		return true;
	}

	static __declspec(naked) void HookTrainingMenuOpen()
	{
		__asm
		{
			pop ecx
			pop edx
			push ecx
			push edx
			mov eax, [g_trainingMenuOpenOriginal]
			test eax, eax
			jnz callOriginal
			mov eax, 005DD4B0h
callOriginal:
			push offset afterOriginal
			jmp eax
afterOriginal:
			pushad
			push eax
			call ApplySpearTrainingMenuAfterOpen
			add esp, 4
			popad
			pop edx
			xchg edx, dword ptr [esp]
			push edx
			ret
		}
	}

	static __declspec(naked) void HookTrainingMenuButton()
	{
		__asm
		{
			pushad
			mov eax, [esp+36]
			push eax
			call HandleSpearTrainingMenuButton
			add esp, 4
			test al, al
			popad
			jnz handled
			jmp dword ptr [g_trainingMenuButtonOriginal]
handled:
			ret 8
		}
	}

	static void __fastcall HookPlayerModExperience(PlayerCharacter* player, void*, UInt32 actorValue, UInt32 useType, float baseDelta)
	{
		if (player == GetPlayer() && (actorValue == kActorVal_Blade || actorValue == kActorVal_Blunt))
		{
			const SpearSkillShared::WeaponSkillKind kind = ClassifySidecarWeapon(GetPlayerEquippedWeapon());
			if (SkillConditionAllowsProgress(kind, actorValue, useType) &&
				AddWeaponProgress(kind, useType, baseDelta))
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
			NormalizeState(index);
			if (outLevel)
				*outLevel = g_state.states[index].level;
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

	static bool RowIsStatsSkill(Tile* tile, UInt32 index)
	{
		if (!tile || GetTileFloat(tile, kStatsRowSyntheticMarkerTrait) != 2.0f)
			return false;

		const float skillId = GetTileFloat(tile, kStatsRowSyntheticSkillIdTrait);
		return std::isfinite(skillId) && static_cast<UInt32>(skillId + 0.5f) == kSkills[index].skillId;
	}

	static bool TryGetForeignSyntheticStatsSkillId(Tile* tile, UInt32& skillId)
	{
		skillId = 0xFFFFFFFF;
		if (!tile || GetTileFloat(tile, kStatsRowSyntheticMarkerTrait) != 2.0f)
			return false;

		const float rawSkillId = GetTileFloat(tile, kStatsRowSyntheticSkillIdTrait);
		if (!std::isfinite(rawSkillId))
			return false;

		skillId = static_cast<UInt32>(rawSkillId + 0.5f);
		return GetSkillIndexById(skillId) >= kSkillCount;
	}

	static void UpdateStatsRowTile(UInt32 index, Tile* tile, float order)
	{
		if (!tile)
			return;

		SetTileFloat(tile, kTileValue_user1, 1.0f);
		SetTileFloat(tile, kTileValue_user2, GetProgressFraction(index));
		SetTileFloat(tile, kTileValue_user3, static_cast<float>(g_state.states[index].level));
		SetTileString(tile, kTileValue_user4, kSkills[index].name);
		SetTileString(tile, kTileValue_user5, GetSidecarSkillRowIcon(index));
		SetTileFloat(tile, kTileValue_user6, static_cast<float>(kSkills[index].fallbackActorValue));
		SetTileFloat(tile, kTileValue_user7, static_cast<float>(g_state.states[index].level));
		SetTileFloat(tile, kTileValue_listindex, order);
		SetTileFloat(tile, kStatsRowSyntheticSkillIdTrait, static_cast<float>(kSkills[index].skillId));
		SetTileFloat(tile, kStatsRowSyntheticMarkerTrait, 2.0f);
	}

	static Tile* CreateStatsRow(void* statsMenu, UInt32 index)
	{
		Tile* parent = GetStatsMenuSkillParent(statsMenu);
		if (!statsMenu || !parent)
			return nullptr;

		Tile* tile = MenuCreateTileFromTemplate()(statsMenu, parent, kStatsSkillTemplate, 0);
		if (!tile && !g_loggedStatsRowCreateFailure)
		{
			_WARNING("SpearSkill: failed to create StatsMenu row");
			g_loggedStatsRowCreateFailure = true;
		}
		return tile;
	}

	static Tile* FindStatsRow(void* statsMenu, UInt32 index)
	{
		Tile* parent = GetStatsMenuSkillParent(statsMenu);
		if (!parent)
			return nullptr;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(parent) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (RowIsStatsSkill(tile, index))
				return tile;

			node = *reinterpret_cast<UInt32*>(node);
		}

		return nullptr;
	}

	static void RefreshSidecarSkillDisplay(UInt32 index)
	{
		if (index >= kSkillCount || !g_statsMenu)
			return;

		if (!g_statsRows[index].tile)
			g_statsRows[index].tile = FindStatsRow(g_statsMenu, index);
		if (!g_statsRows[index].tile)
			return;

		const float order = GetTileFloat(g_statsRows[index].tile, kTileValue_listindex);
		UpdateStatsRowTile(index, g_statsRows[index].tile, order);
	}

	static UInt32 GetNativeMajorCount(void* statsMenu)
	{
		return static_cast<UInt32>(GetTileFloat(GetStatsMenuSummaryTile(statsMenu), kTileValue_user3));
	}

	static UInt32 CountNativeMajorRows(void* statsMenu, UInt32 summaryMajorCount)
	{
		if (!statsMenu || !summaryMajorCount)
			return 0;

		UInt32 count = 0;
		for (UInt32 i = 0; i < kNativeSkillCount; ++i)
		{
			Tile* row = GetNativeStatsRow(statsMenu, i);
			if (!row)
				continue;

			const float order = GetTileFloat(row, kTileValue_listindex);
			if (std::isfinite(order) && order >= 0.0f && order < static_cast<float>(summaryMajorCount))
				++count;
		}
		return count;
	}

	static UInt32 GetRawNativeMajorCount(void* statsMenu)
	{
		const UInt32 summaryMajorCount = GetNativeMajorCount(statsMenu);
		const UInt32 nativeMajorCount = CountNativeMajorRows(statsMenu, summaryMajorCount);
		return nativeMajorCount || !summaryMajorCount ? nativeMajorCount : summaryMajorCount;
	}

	static void CountForeignSyntheticStatsRows(void* statsMenu, UInt32 currentMajorCount, UInt32& majorCount, UInt32& minorCount)
	{
		majorCount = 0;
		minorCount = 0;
		Tile* parent = GetStatsMenuSkillParent(statsMenu);
		if (!parent)
			return;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(parent) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 skillId = 0;
			if (TryGetForeignSyntheticStatsSkillId(tile, skillId))
			{
				const float order = GetTileFloat(tile, kTileValue_listindex);
				if (std::isfinite(order) && order >= 0.0f && order < static_cast<float>(currentMajorCount))
					++majorCount;
				else
					++minorCount;
			}

			node = *reinterpret_cast<UInt32*>(node);
		}
	}

	static void RestoreStatsMenuNativeMajorSummary(void* statsMenu)
	{
		Tile* summary = GetStatsMenuSummaryTile(statsMenu);
		if (!summary || !HasNativeStatsRows(statsMenu))
			return;

		const UInt32 summaryMajorCount = GetNativeMajorCount(statsMenu);
		const UInt32 nativeMajorCount = CountNativeMajorRows(statsMenu, summaryMajorCount);
		if (nativeMajorCount && nativeMajorCount != summaryMajorCount)
			SetTileFloat(summary, kTileValue_user3, static_cast<float>(nativeMajorCount));
	}

	static bool NativeStatsRowIsHiddenBySidecar(Tile* row)
	{
		if (!row)
			return false;

		return GetTileFloat(row, kTileValue_listclip) == 2.0f &&
			GetTileFloat(row, kTileValue_visible) != 2.0f &&
			GetTileFloat(row, kTileValue_heightRaw) <= 1.0f;
	}

	static bool NativeStatsOrderIsMajor(float order, UInt32 rawNativeMajorCount)
	{
		return std::isfinite(order) &&
			order >= 0.0f &&
			order < static_cast<float>(rawNativeMajorCount);
	}

	static float GetNativeMiscStatsOrderStart(UInt32 rawNativeMajorCount)
	{
		return rawNativeMajorCount ? static_cast<float>(rawNativeMajorCount + 1) : 0.0f;
	}

	static UInt32 CountVisibleNativeMajorRows(void* statsMenu, UInt32 rawNativeMajorCount)
	{
		if (!statsMenu || !rawNativeMajorCount)
			return 0;

		UInt32 count = 0;
		for (UInt32 i = 0; i < kNativeSkillCount; ++i)
		{
			Tile* row = GetNativeStatsRow(statsMenu, i);
			if (!row || NativeStatsRowIsHiddenBySidecar(row))
				continue;

			if (NativeStatsOrderIsMajor(GetTileFloat(row, kTileValue_listindex), rawNativeMajorCount))
				++count;
		}
		return count;
	}

	static UInt32 GetSyntheticMiscStatsOrderBase(UInt32 nativeMajorCount, UInt32 syntheticMajorCount);

	static void RepairVisibleNativeStatsOrdering(void* statsMenu, UInt32 rawNativeMajorCount, UInt32 visibleNativeMajorCount, UInt32 foreignSyntheticMajorCount, UInt32 syntheticMajorCount, UInt32 foreignSyntheticMinorCount, UInt32 syntheticMinorCount)
	{
		if (!statsMenu)
			return;

		UInt32 nativeMajorIndex = 0;
		UInt32 nativeMiscIndex = 0;
		const float rawNativeMiscStart = GetNativeMiscStatsOrderStart(rawNativeMajorCount);
		const UInt32 nativeMiscBase =
			GetSyntheticMiscStatsOrderBase(visibleNativeMajorCount, foreignSyntheticMajorCount + syntheticMajorCount) +
			foreignSyntheticMinorCount +
			syntheticMinorCount;

		for (UInt32 i = 0; i < kNativeSkillCount; ++i)
		{
			Tile* row = GetNativeStatsRow(statsMenu, i);
			if (!row || NativeStatsRowIsHiddenBySidecar(row))
				continue;

			const float order = GetTileFloat(row, kTileValue_listindex);
			if (!std::isfinite(order))
				continue;

			float repairedOrder = order;
			if (NativeStatsOrderIsMajor(order, rawNativeMajorCount))
				repairedOrder = static_cast<float>(nativeMajorIndex++);
			else if (order >= rawNativeMiscStart)
				repairedOrder = static_cast<float>(nativeMiscBase + nativeMiscIndex++);

			if (repairedOrder != order)
				SetTileFloat(row, kTileValue_listindex, repairedOrder);
		}
	}

	static void RepairForeignSyntheticStatsOrdering(void* statsMenu, UInt32 currentMajorCount, UInt32 visibleNativeMajorCount, UInt32 foreignSyntheticMajorCount, UInt32 syntheticMajorCount, UInt32 foreignSyntheticMinorCount)
	{
		Tile* parent = GetStatsMenuSkillParent(statsMenu);
		if (!parent)
			return;

		UInt32 foreignMajorIndex = 0;
		UInt32 foreignMinorIndex = 0;
		const UInt32 miscBase = GetSyntheticMiscStatsOrderBase(visibleNativeMajorCount, foreignSyntheticMajorCount + syntheticMajorCount);
		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(parent) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 skillId = 0;
			if (TryGetForeignSyntheticStatsSkillId(tile, skillId))
			{
				const float order = GetTileFloat(tile, kTileValue_listindex);
				if (std::isfinite(order))
				{
					float repairedOrder = order;
					if (foreignMajorIndex < foreignSyntheticMajorCount &&
						order >= 0.0f &&
						order < static_cast<float>(currentMajorCount))
					{
						repairedOrder = static_cast<float>(visibleNativeMajorCount + foreignMajorIndex++);
					}
					else if (foreignMinorIndex < foreignSyntheticMinorCount)
					{
						repairedOrder = static_cast<float>(miscBase + foreignMinorIndex++);
					}

					if (repairedOrder != order)
						SetTileFloat(tile, kTileValue_listindex, repairedOrder);
				}
			}

			node = *reinterpret_cast<UInt32*>(node);
		}
	}

	static UInt32 CountMajorSidecars()
	{
		UInt32 count = 0;
		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (IsEffectiveMajor(i))
				++count;
		}
		return count;
	}

	static UInt32 GetSyntheticMiscStatsOrderBase(UInt32 nativeMajorCount, UInt32 syntheticMajorCount)
	{
		const bool hasMajorSection = (nativeMajorCount + syntheticMajorCount) > 0;
		return nativeMajorCount + syntheticMajorCount + (hasMajorSection ? 1 : 0);
	}

	static void SyncStatsMenuRows(void* statsMenu, bool repairOrdering)
	{
		if (!statsMenu || !HasNativeStatsRows(statsMenu))
			return;

		if (g_statsMenu != statsMenu)
		{
			g_statsMenu = statsMenu;
			std::memset(g_statsRows, 0, sizeof(g_statsRows));
			g_loggedStatsRowCreateFailure = false;
		}

		const UInt32 rawNativeMajorCount = repairOrdering ? GetRawNativeMajorCount(statsMenu) : 0;
		const UInt32 currentMajorCount = repairOrdering ? GetNativeMajorCount(statsMenu) : 0;
		UInt32 foreignSyntheticMajorCount = 0;
		UInt32 foreignSyntheticMinorCount = 0;
		if (repairOrdering)
			CountForeignSyntheticStatsRows(statsMenu, currentMajorCount, foreignSyntheticMajorCount, foreignSyntheticMinorCount);
		const UInt32 visibleNativeMajorCount = repairOrdering ? CountVisibleNativeMajorRows(statsMenu, rawNativeMajorCount) : 0;
		const UInt32 syntheticMajorCount = repairOrdering ? CountMajorSidecars() : 0;
		const UInt32 syntheticMinorCount = repairOrdering ? kSkillCount - syntheticMajorCount : 0;
		UInt32 syntheticMajorIndex = 0;
		UInt32 syntheticMiscIndex = 0;

		if (repairOrdering)
		{
			RepairVisibleNativeStatsOrdering(
				statsMenu,
				rawNativeMajorCount,
				visibleNativeMajorCount,
				foreignSyntheticMajorCount,
				syntheticMajorCount,
				foreignSyntheticMinorCount,
				syntheticMinorCount);
			RepairForeignSyntheticStatsOrdering(
				statsMenu,
				currentMajorCount,
				visibleNativeMajorCount,
				foreignSyntheticMajorCount,
				syntheticMajorCount,
				foreignSyntheticMinorCount);
		}
		if (repairOrdering)
		{
			if (Tile* summary = GetStatsMenuSummaryTile(statsMenu))
				SetTileFloat(summary, kTileValue_user3, static_cast<float>(visibleNativeMajorCount + syntheticMajorCount + foreignSyntheticMajorCount));
		}

		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (!g_statsRows[i].tile)
				g_statsRows[i].tile = FindStatsRow(statsMenu, i);
			if (!g_statsRows[i].tile)
				g_statsRows[i].tile = CreateStatsRow(statsMenu, i);

			float order = g_statsRows[i].tile ? GetTileFloat(g_statsRows[i].tile, kTileValue_listindex) : 0.0f;
			if (repairOrdering)
			{
				if (IsEffectiveMajor(i))
				{
					order = static_cast<float>(visibleNativeMajorCount + foreignSyntheticMajorCount + syntheticMajorIndex++);
				}
				else
					order = static_cast<float>(GetSyntheticMiscStatsOrderBase(visibleNativeMajorCount, foreignSyntheticMajorCount + syntheticMajorCount) + foreignSyntheticMinorCount + syntheticMiscIndex++);
			}

			UpdateStatsRowTile(i, g_statsRows[i].tile, order);
		}

		if (repairOrdering)
			g_statsOrderingDirty = false;
	}

	static void __fastcall HookStatsMenuCreateRows(void* statsMenu, void*)
	{
		std::memset(g_statsRows, 0, sizeof(g_statsRows));
		g_statsMenu = statsMenu;
		g_insideStatsMenuCreateRows = true;
		StatsMenuCreateRowsOriginal()(statsMenu);
		g_insideStatsMenuCreateRows = false;
		SyncStatsMenuRows(statsMenu, true);
	}

	static void __fastcall HookStatsMenuRefresh(void* statsMenu, void*, UInt32 actorValue)
	{
		if (actorValue == 0xFFFFFFFF)
			RestoreStatsMenuNativeMajorSummary(statsMenu);
		StatsMenuRefreshOriginal()(statsMenu, actorValue);
		if (g_insideStatsMenuCreateRows)
			return;

		bool repairOrdering = actorValue == 0xFFFFFFFF;
		if (g_statsOrderingDirty && !repairOrdering)
		{
			RestoreStatsMenuNativeMajorSummary(statsMenu);
			StatsMenuRefreshOriginal()(statsMenu, 0xFFFFFFFF);
			repairOrdering = true;
		}
		SyncStatsMenuRows(statsMenu, repairOrdering);
	}

	static void __stdcall AddStatsMenuMasteryCounts(void*, UInt32* masteryCounts)
	{
		if (!masteryCounts)
			return;

		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			const UInt32 mastery = CalcMasteryFromSkill()(static_cast<SInt32>(g_state.states[i].level));
			if (mastery < kStatsMenuMasteryRankCount)
				++masteryCounts[mastery];
		}
	}

	static __declspec(naked) void HookStatsMenuMasteryCounts()
	{
		__asm
		{
			lea eax, [esp+30h]
			push eax
			push esi
			call AddStatsMenuMasteryCounts
			mov dword ptr [esp+14h], 5
			xor ebx, ebx
			mov eax, 005DAAB2h
			jmp eax
		}
	}

	static void ComposeSkillDescriptionText(UInt32 index, char* buffer, UInt32 bufferSize)
	{
		if (!buffer || !bufferSize)
			return;

		if (index >= kSkillCount)
		{
			buffer[0] = '\0';
			return;
		}

		NormalizeState(index);
		const SkillState& state = g_state.states[index];
		_snprintf_s(buffer, bufferSize, _TRUNCATE,
			"%s\n\nGoverning Attribute: %s\n\nLevel: %s",
			kSkills[index].description,
			GetSafeActorValueName(kSkills[index].governingAttributeAV),
			GetSafeMasteryName(state.level));
	}

	static void ComposeSkillDetailText(UInt32 index, char* buffer, UInt32 bufferSize)
	{
		ComposeSkillDescriptionText(index, buffer, bufferSize);
	}

	static void ComposeClassPickerSkillDetailText(UInt32 index, char* buffer, UInt32 bufferSize)
	{
		ComposeSkillDescriptionText(index, buffer, bufferSize);
	}

	static UInt32 FindStatsSkillIndexByTile(Tile* tile)
	{
		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (RowIsStatsSkill(tile, i))
				return i;
		}
		return 0xFFFFFFFF;
	}

	static void UpdateStatsFocusTile(void* statsMenu, Tile* selectedTile)
	{
		Tile* focus = GetStatsMenuFocusTile(statsMenu);
		if (!focus || !selectedTile)
			return;

		if (GetTileFloat(selectedTile, kTileValue_listclip) == 2.0f)
		{
			SetTileFloat(focus, kTileValue_visible, 1.0f);
			return;
		}

		const float width = GetTileFloat(selectedTile, kTileValue_width) - kStatsFocusSizeInset;
		const float height = GetTileFloat(selectedTile, kTileValue_height) - kStatsFocusSizeInset;
		SetTileFloat(focus, kTileValue_depth, static_cast<float>(TileGetGlobalDepth()(selectedTile)) - kStatsFocusDepthInset);
		SetTileFloat(focus, kTileValue_width, width > 0.0f ? width : 0.0f);
		SetTileFloat(focus, kTileValue_height, height > 0.0f ? height : 0.0f);
		SetTileFloat(focus, kTileValue_x, static_cast<float>(TileGetGlobalX()(selectedTile)));
		SetTileFloat(focus, kTileValue_y, static_cast<float>(TileGetGlobalY()(selectedTile)) + kStatsFocusYOffset);
		SetTileFloat(focus, kTileValue_visible, 2.0f);
	}

	static bool __stdcall HandleStatsMenuDetails(void* statsMenu, UInt32 buttonId, Tile* selectedTile)
	{
		if (buttonId != 0x22)
			return false;

		const UInt32 index = FindStatsSkillIndexByTile(selectedTile);
		if (index >= kSkillCount)
			return false;

		Tile* detailTile = GetStatsMenuDetailTile(statsMenu);
		if (!detailTile)
			return false;

		UpdateStatsFocusTile(statsMenu, selectedTile);
		char detailText[768] = {};
		ComposeSkillDetailText(index, detailText, sizeof(detailText));
		SetTileString(detailTile, kTileValue_user2, GetSidecarSkillIcon(index));
		SetTileString(detailTile, kTileValue_user3, detailText);
		SetTileFloat(detailTile, kTileValue_user5, -1.0f);
		SetTileFloat(detailTile, kTileValue_user4, 2.0f);
		TileAnimateTrait()(detailTile, kTileValue_user0, GetTileFloat(detailTile, kTileValue_user0), 1.0f, GetTileFloat(detailTile, kTileValue_user1));
		return true;
	}

	static __declspec(naked) void HookStatsMenuDetails()
	{
		__asm
		{
			pushad
			mov eax, [esp+40]
			push eax
			mov eax, [esp+40]
			push eax
			push ecx
			call HandleStatsMenuDetails
			test al, al
			popad
			jnz handled
			jmp dword ptr [g_statsMenuDetailsOriginal]
handled:
			ret 8
		}
	}

	static Tile* GetSkillsMenuListTile(void* skillsMenu)
	{
		return GetMenuTileAtOffset(skillsMenu, kSkillsMenuListTileOffset);
	}

	static Tile* GetSkillsMenuAcceptButton(void* skillsMenu)
	{
		return GetMenuTileAtOffset(skillsMenu, kSkillsMenuAcceptButtonOffset);
	}

	static Tile* GetSkillsMenuTile(void* skillsMenu)
	{
		return GetMenuTileAtOffset(skillsMenu, kSkillsMenuTileOffset);
	}

	static void* GetSkillsMenuClassMenu(void* skillsMenu)
	{
		return skillsMenu ? *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuClassMenuOffset) : nullptr;
	}

	static UInt32 GetSkillsMenuMode(void* skillsMenu)
	{
		return skillsMenu ? *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuModeOffset) : 0xFFFFFFFF;
	}

	static UInt32 GetSkillsMenuCurrentValue(void* skillsMenu)
	{
		return skillsMenu ? *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuCurrentValueOffset) : 0;
	}

	static void SetSkillsMenuSelectionCap(void* skillsMenu, UInt32 cap)
	{
		if (skillsMenu)
			*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuSelectionCapOffset) = cap;
	}

	static UInt32* GetClassMenuNativeSkillArray(void* classMenu)
	{
		return classMenu ? reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuSelectedSkillsOffset) : nullptr;
	}

	static bool ClassMenuHasNativeMajor(void* classMenu, UInt32 actorValue)
	{
		UInt32* nativeSkills = GetClassMenuNativeSkillArray(classMenu);
		if (!nativeSkills)
			return false;

		for (UInt32 i = 0; i < kNativeClassMajorCount; ++i)
		{
			if (nativeSkills[i] == actorValue)
				return true;
		}
		return false;
	}

	static bool SidecarDisplayedByLegacyNativeSlot(void* classMenu, UInt32 index)
	{
		return false;
	}

	static Tile* GetClassMenuTile(void* classMenu)
	{
		return GetMenuTileAtOffset(classMenu, kClassMenuTileOffset);
	}

	static void* GetClassMenuSelectedClass(void* classMenu)
	{
		return classMenu ? *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuSelectedClassOffset) : nullptr;
	}

	static void* GetClassMenuCustomClass(void* classMenu)
	{
		return classMenu ? *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuCustomClassOffset) : nullptr;
	}

	static UInt32 GetFormId(void* form)
	{
		return form ? *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(form) + kTESFormFormIdOffset) : 0;
	}

	static void SetClassMenuCurrentPickerValue(void* classMenu, UInt32 mode, UInt32 value)
	{
		if (!classMenu || mode > 2)
			return;

		*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuCurrentPickerValueOffset + mode * sizeof(UInt32)) = value;
	}

	static bool IsClassSkillPicker(void* skillsMenu)
	{
		return GetSkillsMenuClassMenu(skillsMenu) != nullptr && GetSkillsMenuMode(skillsMenu) == 0;
	}

	static bool RowIsSelected(Tile* tile)
	{
		return GetTileFloat(tile, kPickerRowSelectedTrait) == 2.0f;
	}

	static UInt32 GetPickerRowSyntheticSkillId(Tile* tile)
	{
		if (!tile || GetTileFloat(tile, kPickerSyntheticMarkerTrait) != 2.0f)
			return 0xFFFFFFFF;

		const float skillId = GetTileFloat(tile, kPickerSyntheticSkillIdTrait);
		return std::isfinite(skillId) ? static_cast<UInt32>(skillId + 0.5f) : 0xFFFFFFFF;
	}

	static UInt32 FindPickerSkillIndexByTile(Tile* tile)
	{
		return GetSkillIndexById(GetPickerRowSyntheticSkillId(tile));
	}

	static bool TryGetForeignSyntheticPickerSkillId(Tile* tile, UInt32& skillId)
	{
		skillId = 0xFFFFFFFF;
		if (!tile || GetTileFloat(tile, kPickerSyntheticMarkerTrait) != 2.0f)
			return false;

		const float rawSkillId = GetTileFloat(tile, kPickerSyntheticSkillIdTrait);
		if (!std::isfinite(rawSkillId))
			return false;

		skillId = static_cast<UInt32>(rawSkillId + 0.5f);
		return GetSkillIndexById(skillId) >= kSkillCount;
	}

	static UInt32 GetPickerRowActorValue(Tile* tile)
	{
		if (!tile)
			return 0xFFFFFFFF;

		const float actorValue = GetTileFloat(tile, kPickerRowValueTrait);
		return std::isfinite(actorValue) ? static_cast<UInt32>(actorValue + 0.5f) : 0xFFFFFFFF;
	}

	static bool IsNativePickerSkillRow(Tile* tile, UInt32& actorValue)
	{
		if (!tile || GetTileFloat(tile, kPickerSyntheticMarkerTrait) == 2.0f)
			return false;

		actorValue = GetPickerRowActorValue(tile);
		return actorValue >= kFirstNativeSkillAV && actorValue <= kLastNativeSkillAV;
	}

	static bool NativePickerRowIsHiddenBySidecar(Tile* tile)
	{
		if (!tile)
			return false;

		return GetTileFloat(tile, kTileValue_listclip) == 2.0f &&
			GetTileFloat(tile, kTileValue_visible) != 2.0f &&
			GetTileFloat(tile, kTileValue_heightRaw) <= 1.0f;
	}

	static float GetSidecarPickerOrder(UInt32 visibleNativeCount, UInt32 foreignSyntheticCount, UInt32 index)
	{
		return static_cast<float>(visibleNativeCount + foreignSyntheticCount + index);
	}

	static bool HasPickerRow(void* skillsMenu, UInt32 index)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return false;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (FindPickerSkillIndexByTile(tile) == index)
				return true;

			node = *reinterpret_cast<UInt32*>(node);
		}
		return false;
	}

	static void HideNativeBladeClassPickerRows(void* skillsMenu)
	{
	}

	static UInt32 CountForeignSyntheticPickerRows(void* skillsMenu)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return 0;

		UInt32 count = 0;
		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 skillId = 0;
			if (TryGetForeignSyntheticPickerSkillId(tile, skillId))
				++count;

			node = *reinterpret_cast<UInt32*>(node);
		}
		return count;
	}

	static void RepairClassPickerRowOrdering(void* skillsMenu)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return;

		// Vanilla name-sorts this picker before row creation; compact the live ordered list instead of rebuilding order from actor-value IDs.
		Tile* visibleNativeRows[kNativeSkillCount] = {};
		UInt32 visibleNativeCount = 0;
		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x38);
		while (node && visibleNativeCount < kNativeSkillCount)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 actorValue = 0xFFFFFFFF;
			if (IsNativePickerSkillRow(tile, actorValue) && !NativePickerRowIsHiddenBySidecar(tile))
				visibleNativeRows[visibleNativeCount++] = tile;

			node = *reinterpret_cast<UInt32*>(node + 4);
		}

		for (UInt32 i = 0; i < visibleNativeCount; ++i)
			SetTileFloat(visibleNativeRows[i], kTileValue_listindex, static_cast<float>(i));
	}

	static void RepairForeignSyntheticPickerOrdering(void* skillsMenu, UInt32 visibleNativeCount)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return;

		UInt32 foreignIndex = 0;
		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x38);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 skillId = 0;
			if (TryGetForeignSyntheticPickerSkillId(tile, skillId))
				SetTileFloat(tile, kTileValue_listindex, static_cast<float>(visibleNativeCount + foreignIndex++));

			node = *reinterpret_cast<UInt32*>(node + 4);
		}
	}

	static UInt32 CountVisibleNativePickerRows(void* skillsMenu)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return kNativeSkillCount - 1;

		UInt32 count = 0;
		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 actorValue = 0xFFFFFFFF;
			if (IsNativePickerSkillRow(tile, actorValue) && !NativePickerRowIsHiddenBySidecar(tile))
				++count;

			node = *reinterpret_cast<UInt32*>(node);
		}
		return count ? count : kNativeSkillCount;
	}

	static bool GetStagedOrSavedSelection(void* classMenu, UInt32 index)
	{
		if (g_stagedUsed && g_stagedClassMenu == classMenu)
			return g_stagedSelections[index];

		return g_state.states[index].major != 0;
	}

	static void UpdatePickerRow(void* skillsMenu, UInt32 index, UInt32 visibleNativeCount, UInt32 foreignSyntheticCount)
	{
		Tile* row = g_pickerRows[index].tile;
		if (!row)
			return;

		const bool selected = GetStagedOrSavedSelection(GetSkillsMenuClassMenu(skillsMenu), index);
		SetTileFloat(row, kPickerRowSelectedTrait, selected ? 2.0f : 1.0f);
		SetTileFloat(row, kPickerRowValueTrait, kSyntheticPickerNativeValueSentinel);
		SetTileFloat(row, kPickerSyntheticSkillIdTrait, static_cast<float>(kSkills[index].skillId));
		SetTileFloat(row, kPickerSyntheticMarkerTrait, 2.0f);
		SetTileFloat(row, kTileValue_listindex, GetSidecarPickerOrder(visibleNativeCount, foreignSyntheticCount, index));
	}

	static UInt32 CountSelectedPickerRows(void* skillsMenu)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return 0;

		UInt32 count = 0;
		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (tile && !NativePickerRowIsHiddenBySidecar(tile) && RowIsSelected(tile))
				++count;

			node = *reinterpret_cast<UInt32*>(node);
		}
		return count;
	}

	static void UpdateClassSkillPickerAcceptButton(void* skillsMenu)
	{
		Tile* acceptButton = GetSkillsMenuAcceptButton(skillsMenu);
		if (!acceptButton)
			return;

		const float enabled = CountSelectedPickerRows(skillsMenu) == kNativeClassMajorCount ? 2.0f : 1.0f;
		SetTileFloat(acceptButton, kTileValue_user1, enabled);
		SetTileFloat(acceptButton, kTileValue_heightRaw, enabled);
	}

	static void SyncClassSkillPickerRows(void* skillsMenu)
	{
		if (!IsClassSkillPicker(skillsMenu))
			return;

		if (g_classPickerSkillsMenu != skillsMenu)
		{
			g_classPickerSkillsMenu = skillsMenu;
			std::memset(g_pickerRows, 0, sizeof(g_pickerRows));
		}

		HideNativeBladeClassPickerRows(skillsMenu);
		RepairClassPickerRowOrdering(skillsMenu);
		const UInt32 visibleNativeCount = CountVisibleNativePickerRows(skillsMenu);
		const UInt32 foreignSyntheticCount = CountForeignSyntheticPickerRows(skillsMenu);
		RepairForeignSyntheticPickerOrdering(skillsMenu, visibleNativeCount);
		SetSkillsMenuSelectionCap(skillsMenu, kNativeClassMajorCount);

		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (!g_pickerRows[i].tile && !HasPickerRow(skillsMenu, i))
				g_pickerRows[i].tile = SkillsMenuCreateSkillRow()(skillsMenu, kSkills[i].name, kSkills[i].fallbackActorValue);
			UpdatePickerRow(skillsMenu, i, visibleNativeCount, foreignSyntheticCount);
		}

		UpdateClassSkillPickerAcceptButton(skillsMenu);
	}

	static void __fastcall HookSkillsMenuPreselect(void* skillsMenu, void*)
	{
		SkillsMenuPreselectOriginal()(skillsMenu);
		SyncClassSkillPickerRows(skillsMenu);
	}

	static bool __stdcall HandleSkillsMenuDetails(void* skillsMenu, UInt32)
	{
		if (!IsClassSkillPicker(skillsMenu))
			return false;

		Tile* selectedTile = *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuSelectedTileOffset);
		const UInt32 index = FindPickerSkillIndexByTile(selectedTile);
		if (index >= kSkillCount)
			return false;

		Tile* detailTile = GetSkillsMenuTile(skillsMenu);
		if (!detailTile)
			return false;

		char detailText[768] = {};
		ComposeClassPickerSkillDetailText(index, detailText, sizeof(detailText));
		SetTileString(detailTile, kTileValue_user1, detailText);
		SetTileString(detailTile, kTileValue_user2, GetSidecarSkillIcon(index));
		return true;
	}

	static __declspec(naked) void HookSkillsMenuDetails()
	{
		__asm
		{
			pushad
			mov eax, [esp+36]
			push eax
			push ecx
			call HandleSkillsMenuDetails
			test al, al
			popad
			jnz handled
			jmp dword ptr [g_skillsMenuDetailsOriginal]
handled:
			ret 4
		}
	}

	static void __fastcall HookSkillsMenuUpdateAccept(void* skillsMenu, void*)
	{
		if (IsClassSkillPicker(skillsMenu))
		{
			UpdateClassSkillPickerAcceptButton(skillsMenu);
			return;
		}

		SkillsMenuUpdateAcceptOriginal()(skillsMenu);
	}

	static void CollectSelectedClassPickerRows(
		void* skillsMenu,
		UInt32* nativeActorValues,
		UInt32& nativeCount,
		bool* sidecarSelected,
		UInt32& foreignSyntheticSelectedCount,
		UInt32* selectedSyntheticSkillIds,
		UInt32& selectedSyntheticSkillCount)
	{
		nativeCount = 0;
		foreignSyntheticSelectedCount = 0;
		selectedSyntheticSkillCount = 0;
		std::memset(sidecarSelected, 0, sizeof(bool) * kSkillCount);
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x38);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (tile && !NativePickerRowIsHiddenBySidecar(tile) && RowIsSelected(tile))
			{
				const UInt32 sidecarIndex = FindPickerSkillIndexByTile(tile);
				if (sidecarIndex < kSkillCount)
				{
					sidecarSelected[sidecarIndex] = true;
					AddUniqueUInt32(selectedSyntheticSkillIds, selectedSyntheticSkillCount,
						kNativeClassMajorCount, kSkills[sidecarIndex].skillId);
				}
				else
				{
					UInt32 foreignSkillId = 0;
					if (TryGetForeignSyntheticPickerSkillId(tile, foreignSkillId))
					{
						++foreignSyntheticSelectedCount;
						AddUniqueUInt32(selectedSyntheticSkillIds, selectedSyntheticSkillCount,
							kNativeClassMajorCount, foreignSkillId);
					}
					else
					{
						const UInt32 actorValue = static_cast<UInt32>(GetTileFloat(tile, kPickerRowValueTrait) + 0.5f);
						if (IsVisibleNativeSkillActorValue(actorValue))
							AddUniqueUInt32(nativeActorValues, nativeCount, kNativeClassMajorCount, actorValue);
					}
				}
			}

			node = *reinterpret_cast<UInt32*>(node + 4);
		}
	}

	static void FillNativeClassMajorArray(void* classMenu, const UInt32* selectedActorValues, UInt32 selectedCount)
	{
		UInt32* nativeSkills = GetClassMenuNativeSkillArray(classMenu);
		if (!nativeSkills)
			return;

		UInt32 repaired[kNativeClassMajorCount] = {};
		UInt32 repairedCount = 0;
		for (UInt32 i = 0; i < selectedCount; ++i)
		{
			if (IsVisibleNativeSkillActorValue(selectedActorValues[i]))
				AddUniqueUInt32(repaired, repairedCount, kNativeClassMajorCount, selectedActorValues[i]);
		}
		for (UInt32 i = 0; i < kNativeClassMajorCount; ++i)
		{
			if (IsVisibleNativeSkillActorValue(nativeSkills[i]))
				AddUniqueUInt32(repaired, repairedCount, kNativeClassMajorCount, nativeSkills[i]);
		}
		for (UInt32 actorValue = kFirstNativeSkillAV; actorValue <= kLastNativeSkillAV && repairedCount < kNativeClassMajorCount; ++actorValue)
		{
			if (IsVisibleNativeSkillActorValue(actorValue))
				AddUniqueUInt32(repaired, repairedCount, kNativeClassMajorCount, actorValue);
		}
		for (UInt32 i = 0; i < kNativeClassMajorCount; ++i)
			nativeSkills[i] = repaired[i];
	}

	static bool IsClassSkillPickerAcceptEvent(void* skillsMenu, UInt32 buttonId)
	{
		if (buttonId == 4 || buttonId == 5 || buttonId == 6)
			return true;

		return buttonId == 7 && GetTileFloat(GetSkillsMenuTile(skillsMenu), kSkillsMenuForwardEnabledTrait) == 2.0f;
	}

	static void AdvanceClassMenuAfterSkillPicker(void* classMenu, void* skillsMenu, UInt32 buttonId)
	{
		UInt32* step = reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuStepOffset);
		const bool increment = buttonId == 4 || (buttonId == 7 && GetTileFloat(GetSkillsMenuTile(skillsMenu), kSkillsMenuForwardEnabledTrait) == 2.0f);
		if (increment)
			++*step;
		else
		{
			--*step;
			if (*step == 0)
				ClassMenuStepRefresh()(classMenu);
		}
		SetClassMenuCurrentPickerValue(classMenu, 0, GetSkillsMenuCurrentValue(skillsMenu));
		SkillsMenuClose()();
	}

	static bool __stdcall HandleClassSkillPickerAccept(void* skillsMenu, UInt32 buttonId, Tile* tile)
	{
		if (!IsClassSkillPicker(skillsMenu) || !IsClassSkillPickerAcceptEvent(skillsMenu, buttonId))
			return false;

		void* classMenu = GetSkillsMenuClassMenu(skillsMenu);
		UInt32 nativeActorValues[kNativeClassMajorCount] = {};
		UInt32 nativeCount = 0;
		bool sidecarSelected[kSkillCount] = {};
		UInt32 foreignSyntheticSelectedCount = 0;
		UInt32 selectedSyntheticSkillIds[kNativeClassMajorCount] = {};
		UInt32 selectedSyntheticSkillCount = 0;
		CollectSelectedClassPickerRows(skillsMenu, nativeActorValues, nativeCount, sidecarSelected,
			foreignSyntheticSelectedCount, selectedSyntheticSkillIds, selectedSyntheticSkillCount);
		g_stagedClassMenu = classMenu;
		std::memcpy(g_stagedSelections, sidecarSelected, sizeof(g_stagedSelections));
		g_stagedForeignSyntheticSelectionCount = foreignSyntheticSelectedCount;
		std::memcpy(g_stagedSelectedSyntheticSkillIds, selectedSyntheticSkillIds,
			sizeof(g_stagedSelectedSyntheticSkillIds));
		g_stagedSelectedSyntheticSkillCount = selectedSyntheticSkillCount;
		g_stagedUsed = classMenu != nullptr;

		if (g_skillsMenuAcceptChainedExisting && g_skillsMenuAcceptOriginal)
		{
			SkillsMenuAcceptOriginal()(skillsMenu, buttonId, tile);
			FillNativeClassMajorArray(classMenu, nativeActorValues, nativeCount);
			return true;
		}

		FillNativeClassMajorArray(classMenu, nativeActorValues, nativeCount);
		AdvanceClassMenuAfterSkillPicker(classMenu, skillsMenu, buttonId);
		return true;
	}

	static __declspec(naked) void HookSkillsMenuAccept()
	{
		__asm
		{
			pushad
			mov eax, [esp+40]
			push eax
			mov eax, [esp+40]
			push eax
			push ecx
			call HandleClassSkillPickerAccept
			test al, al
			popad
			jnz handled
			jmp dword ptr [g_skillsMenuAcceptOriginal]
handled:
			ret 8
		}
	}

	static void ApplyClassMenuHiddenBladeDisplay(Tile* tile)
	{
	}

	static bool TryGetStagedSyntheticDisplaySlot(void* classMenu, UInt32 skillId, UInt32& slot)
	{
		slot = 0;
		if (g_stagedUsed && g_stagedClassMenu == classMenu && g_stagedSelectedSyntheticSkillCount != 0)
		{
			const UInt32 displayCount =
				g_stagedSelectedSyntheticSkillCount > kNativeClassMajorCount ?
				kNativeClassMajorCount :
				g_stagedSelectedSyntheticSkillCount;
			const UInt32 firstDisplayedIndex = g_stagedSelectedSyntheticSkillCount - displayCount;
			const UInt32 firstSlot = kNativeClassMajorCount - displayCount;
			for (UInt32 i = firstDisplayedIndex; i < g_stagedSelectedSyntheticSkillCount; ++i)
			{
				if (g_stagedSelectedSyntheticSkillIds[i] == skillId)
				{
					slot = firstSlot + (i - firstDisplayedIndex);
					return true;
				}
			}
		}

		if (skillId == kSkills[kSpearSkillIndex].skillId)
		{
			slot = 6;
			return true;
		}

		return false;
	}

	static void ApplyClassMenuSidecarDisplay(void* classMenu, void* displayedClass)
	{
		Tile* tile = GetClassMenuTile(classMenu);
		if (!tile)
			return;

		ApplyClassMenuHiddenBladeDisplay(tile);

		if (!displayedClass)
			displayedClass = GetClassMenuSelectedClass(classMenu);
		if (GetClassMenuCustomClass(classMenu) && displayedClass != GetClassMenuCustomClass(classMenu))
			return;

		UInt32 selected[kSkillCount] = {};
		UInt32 selectedCount = 0;
		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (GetStagedOrSavedSelection(classMenu, i) && !SidecarDisplayedByLegacyNativeSlot(classMenu, i))
				selected[selectedCount++] = i;
		}
		const UInt32 displayCount = selectedCount > kNativeClassMajorCount ? kNativeClassMajorCount : selectedCount;
		const UInt32 stagedForeignCount =
			g_stagedUsed && g_stagedClassMenu == classMenu ?
			g_stagedForeignSyntheticSelectionCount :
			0;
		const UInt32 foreignDisplayCount =
			stagedForeignCount > kNativeClassMajorCount - displayCount ?
			kNativeClassMajorCount - displayCount :
			stagedForeignCount;
		const UInt32 firstSlot = kNativeClassMajorCount - foreignDisplayCount - displayCount;
		for (UInt32 i = 0; i < displayCount; ++i)
		{
			const UInt32 index = selected[selectedCount - displayCount + i];
			UInt32 slot = firstSlot + i;
			TryGetStagedSyntheticDisplaySlot(classMenu, kSkills[index].skillId, slot);
			SetTileString(tile, kTileValue_user1 + slot, kSkills[index].name);
			SetTileFloat(tile, kTileValue_user11 + slot, static_cast<float>(kSkills[index].fallbackActorValue));
		}
	}

	static void CommitStagedMajors(void* classMenu)
	{
		if (!g_stagedUsed || g_stagedClassMenu != classMenu)
			return;

		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (g_state.states[i].major != (g_stagedSelections[i] ? 1 : 0))
				g_statsOrderingDirty = true;
			g_state.states[i].major = g_stagedSelections[i] ? 1 : 0;
			NormalizeState(i);
		}
	}

	static void __fastcall HookClassMenuCommit(void* classMenu, void*)
	{
		ClassMenuCommitOriginal()(classMenu);
		CommitStagedMajors(classMenu);
		ApplyClassMenuSidecarDisplay(classMenu, GetClassMenuCustomClass(classMenu));
	}

	static void __fastcall HookClassMenuRefreshDetails(void* classMenu, void*, void* displayedClass)
	{
		ClassMenuRefreshDetailsOriginal()(classMenu, displayedClass);
		ApplyClassMenuSidecarDisplay(classMenu, displayedClass);
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
	static bool WriteRelJumpRaw(const char* name, UInt32 address, UInt32 target, UInt32 patchLength);

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

	static bool WriteRelJumpChecked(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength = 5)
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

	static bool WriteRelJumpRaw(const char* name, UInt32 address, UInt32 target, UInt32 patchLength = 5)
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

	static bool InstallStatsMenuMasteryCountsHook()
	{
		const UInt32 hookTarget = reinterpret_cast<UInt32>(&HookStatsMenuMasteryCounts);
		const UInt8* actual = reinterpret_cast<const UInt8*>(kStatsMenuMasteryCountsPatch);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(kStatsMenuMasteryCountsPatch);
			if (currentTarget == hookTarget)
				return true;

			_MESSAGE("SpearSkill: preserving existing StatsMenu mastery count hook target=%08X for sidecar compatibility", currentTarget);
			return true;
		}

		return WriteRelJumpChecked("StatsMenu mastery count hook",
			kStatsMenuMasteryCountsPatch,
			kStatsMenuMasteryCountsExpected,
			sizeof(kStatsMenuMasteryCountsExpected),
			hookTarget,
			kStatsMenuMasteryCountsPatchLength);
	}

	static bool InstallActorValueGetNameHook()
	{
		const UInt32 hookTarget = reinterpret_cast<UInt32>(&HookActorValueGetName);
		const UInt8* actual = reinterpret_cast<const UInt8*>(kActorValueGetName);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(kActorValueGetName);
			if (currentTarget == hookTarget)
				return true;

			g_actorValueGetNameOriginal = reinterpret_cast<void*>(currentTarget);
			_MESSAGE("SpearSkill: chaining existing ActorValue_GetName hook target=%08X", currentTarget);
			return WriteRelJumpRaw("ActorValue_GetName sidecar display mask chained", kActorValueGetName, hookTarget);
		}

		if (!g_actorValueGetNameOriginal)
			g_actorValueGetNameOriginal = CreateTrampoline(kActorValueGetName, kActorValueGetNamePatchLength);
		if (!g_actorValueGetNameOriginal)
		{
			++g_failedPatches;
			return false;
		}

		return WriteRelJumpChecked("ActorValue_GetName sidecar display mask", kActorValueGetName, kActorValueGetNameExpected, sizeof(kActorValueGetNameExpected), hookTarget, kActorValueGetNamePatchLength);
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

		ok &= WriteRelCallChained("Dialogue Training service Spear sidecar visibility hook",
			kDialogueTrainingServiceOffersCall,
			kTESAIFormOffersService,
			reinterpret_cast<UInt32>(&HookDialogueTrainingOffersService),
			g_dialogueTrainingOffersServiceOriginalTarget);

		ok &= InstallFunctionJumpHook("TrainingMenu Spear sidecar display hook",
			kTrainingMenuOpenJump,
			kTrainingMenuOpenJumpExpected,
			sizeof(kTrainingMenuOpenJumpExpected),
			reinterpret_cast<UInt32>(&HookTrainingMenuOpen),
			sizeof(kTrainingMenuOpenJumpExpected),
			g_trainingMenuOpenOriginal);

		ok &= InstallFunctionJumpHook("TrainingMenu Spear sidecar purchase hook",
			kTrainingMenuButton,
			kTrainingMenuButtonExpected,
			sizeof(kTrainingMenuButtonExpected),
			reinterpret_cast<UInt32>(&HookTrainingMenuButton),
			kTrainingMenuButtonPatchLength,
			g_trainingMenuButtonOriginal);

		ok &= WriteRelJumpChecked("CombatController weapon skill sidecar scoring",
			kCombatControllerWeaponSkillCall,
			kCombatControllerWeaponSkillExpected,
			sizeof(kCombatControllerWeaponSkillExpected),
			reinterpret_cast<UInt32>(&HookCombatControllerWeaponSkillLevel),
			sizeof(kCombatControllerWeaponSkillExpected));

		ok &= WriteRelJumpChecked("Combat selection weapon skill sidecar scoring",
			kCombatSelectionHandToHandComparePatch,
			kCombatSelectionHandToHandCompareExpected,
			sizeof(kCombatSelectionHandToHandCompareExpected),
			reinterpret_cast<UInt32>(&HookCombatSelectionHandToHandSkillCompare),
			sizeof(kCombatSelectionHandToHandCompareExpected));

		ok &= InstallCalcWeaponDamageHook();
		ok &= InstallCalcPowerAttackBonusHook();

		ok &= WriteRelCallChained("PowerAttack KF list weapon sidecar mastery hook", kPowerAttackBuilderMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk), g_weaponPerkMasteryOriginalTarget);
		ok &= WriteRelCallChained("Weapon sidecar sidestep disarm perk level hook", kWeaponDisarmBaseCalcCall, kActorGetBaseCalcAVi, reinterpret_cast<UInt32>(&HookActorGetBaseCalcAViForWeaponSidecarPerk), g_weaponPerkBaseCalcOriginalTarget);
		ok &= WriteRelCallChained("Weapon sidecar sweeping perk mastery hook", kPostHitExpertMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk), g_weaponPerkMasteryOriginalTarget);
		ok &= WriteRelCallChained("Weapon sidecar rushing paralyze mastery hook", kPostHitMasterMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk), g_weaponPerkMasteryOriginalTarget);

		ok &= WriteRelCallChained("MagicPopupMenu enchanted weapon Type sidecar label hook", kMagicPopupEnchantedWeaponLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEbp), g_magicPopupEbpLabelOriginalTarget);
		ok &= WriteRelCallChained("MagicPopupMenu enchanted weapon Type sidecar null-label hook", kMagicPopupEnchantedWeaponNullLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEbp), g_magicPopupEbpLabelOriginalTarget);
		ok &= WriteRelCallChained("MagicPopupMenu weapon Type sidecar label hook", kMagicPopupSimpleWeaponLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEdi), g_magicPopupEdiLabelOriginalTarget);

		ok &= InstallFunctionJumpHook("StatsMenu detail pane hook",
			kStatsMenuDetails,
			kStatsMenuDetailsExpected,
			sizeof(kStatsMenuDetailsExpected),
			reinterpret_cast<UInt32>(&HookStatsMenuDetails),
			kStatsMenuDetailsPatchLength,
			g_statsMenuDetailsOriginal);
		ok &= WriteRelCallChained("StatsMenu skill row creation hook", kStatsMenuCreateRowsCall, kStatsMenuCreateRows, reinterpret_cast<UInt32>(&HookStatsMenuCreateRows), g_statsMenuCreateRowsOriginalTarget);
		for (UInt32 i = 0; i < sizeof(kStatsMenuRefreshCalls) / sizeof(kStatsMenuRefreshCalls[0]); ++i)
			ok &= WriteRelCallChained("StatsMenu refresh hook", kStatsMenuRefreshCalls[i], kStatsMenuRefresh, reinterpret_cast<UInt32>(&HookStatsMenuRefresh), g_statsMenuRefreshOriginalTarget);
		ok &= InstallStatsMenuMasteryCountsHook();

		ok &= InstallFunctionJumpHook("SkillsMenu accept-button hook",
			kSkillsMenuUpdateAccept,
			kSkillsMenuUpdateAcceptExpected,
			sizeof(kSkillsMenuUpdateAcceptExpected),
			reinterpret_cast<UInt32>(&HookSkillsMenuUpdateAccept),
			kSkillsMenuUpdateAcceptPatchLength,
			g_skillsMenuUpdateAcceptOriginal);
		ok &= InstallFunctionJumpHook("SkillsMenu detail hook",
			kSkillsMenuDetails,
			kSkillsMenuDetailsExpected,
			sizeof(kSkillsMenuDetailsExpected),
			reinterpret_cast<UInt32>(&HookSkillsMenuDetails),
			kSkillsMenuDetailsPatchLength,
			g_skillsMenuDetailsOriginal);
		g_skillsMenuAcceptChainedExisting =
			*reinterpret_cast<const UInt8*>(kSkillsMenuAccept) == 0xE9 &&
			ReadRelJumpTarget(kSkillsMenuAccept) != reinterpret_cast<UInt32>(&HookSkillsMenuAccept);
		ok &= InstallFunctionJumpHook("SkillsMenu class-skill writeback hook",
			kSkillsMenuAccept,
			kSkillsMenuAcceptExpected,
			sizeof(kSkillsMenuAcceptExpected),
			reinterpret_cast<UInt32>(&HookSkillsMenuAccept),
			kSkillsMenuAcceptPatchLength,
			g_skillsMenuAcceptOriginal);
		ok &= WriteRelCallChained("SkillsMenu class-skill row injection hook", kSkillsMenuPreselectCall, kSkillsMenuPreselect, reinterpret_cast<UInt32>(&HookSkillsMenuPreselect), g_skillsMenuPreselectOriginalTarget);
		ok &= WriteRelCallChained("ClassMenu custom-class sidecar commit hook", kClassMenuCommitCall, kClassMenuCommit, reinterpret_cast<UInt32>(&HookClassMenuCommit), g_classMenuCommitOriginalTarget);
		for (UInt32 i = 0; i < sizeof(kClassMenuRefreshDetailsCalls) / sizeof(kClassMenuRefreshDetailsCalls[0]); ++i)
			ok &= WriteRelCallChained("ClassMenu class-detail display hook", kClassMenuRefreshDetailsCalls[i], kClassMenuRefreshDetails, reinterpret_cast<UInt32>(&HookClassMenuRefreshDetails), g_classMenuRefreshDetailsOriginalTarget);

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

	static void SeedSkillFromNative(UInt32 index, UInt32 actorValue)
	{
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;

		SkillState& state = g_state.states[index];
		if (state.level != 5 || state.progress != 0.0f || state.levelUps != 0)
			return;

		state.level = player->GetBaseActorValue(actorValue);
		NormalizeState(index);
	}

	static void SeedDefaultsFromPlayer()
	{
		SeedSkillFromNative(kSpearSkillIndex, kActorVal_Blade);
	}

	static UInt32 GetSpearSkill()
	{
		NormalizeState(kSpearSkillIndex);
		return g_state.states[kSpearSkillIndex].level;
	}

	static void SetSpearSkillClamped(SInt64 level)
	{
		NormalizeState(kSpearSkillIndex);
		SkillState& state = g_state.states[kSpearSkillIndex];
		const UInt32 previousLevel = state.level;
		if (level < 0)
			state.level = 0;
		else if (level > static_cast<SInt64>(kMaxSkillLevel))
			state.level = kMaxSkillLevel;
		else
			state.level = static_cast<UInt32>(level);
		NormalizeState(kSpearSkillIndex);

		if (state.level != previousLevel)
		{
			RefreshSidecarSkillDisplay(kSpearSkillIndex);
			RefreshPlayerWeaponSidecarPowerAttackGroups();
		}
	}

	static void ModSpearSkill(SInt32 delta)
	{
		NormalizeState(kSpearSkillIndex);
		SkillState& state = g_state.states[kSpearSkillIndex];
		if (delta <= 0)
		{
			SetSpearSkillClamped(static_cast<SInt64>(state.level) + static_cast<SInt64>(delta));
			return;
		}

		if (state.level >= kMaxSkillLevel)
			return;

		const UInt32 previousLevel = state.level;
		UInt32 increase = static_cast<UInt32>(delta);
		const UInt32 room = kMaxSkillLevel - state.level;
		if (increase > room)
			increase = room;

		float progressDebit = 0.0f;
		for (UInt32 level = previousLevel; level < previousLevel + increase; ++level)
			progressDebit += RequiredProgressForLevel(kSpearSkillIndex, level);

		if (std::isfinite(progressDebit) && progressDebit > 0.0f)
			state.progress = state.progress > progressDebit ? state.progress - progressDebit : 0.0f;

		state.level += increase;
		state.levelUps += increase;
		state.governingAttributeIncreaseCount += increase;
		NormalizeState(kSpearSkillIndex);
		MirrorLevelUpSideEffects(kSpearSkillIndex, increase);
		NotifyLevelIncrease(kSpearSkillIndex, previousLevel, increase);
		RefreshSidecarSkillDisplay(kSpearSkillIndex);
	}

	static float GetSpearProgress()
	{
		NormalizeState(kSpearSkillIndex);
		return g_state.states[kSpearSkillIndex].progress;
	}

	static void SetSpearProgress(float progress)
	{
		NormalizeState(kSpearSkillIndex);
		g_state.states[kSpearSkillIndex].progress = progress;
		NormalizeState(kSpearSkillIndex);
		RefreshSidecarSkillDisplay(kSpearSkillIndex);
	}

	static float GetSpearRequiredProgress()
	{
		NormalizeState(kSpearSkillIndex);
		return g_state.states[kSpearSkillIndex].requiredProgress;
	}

	static UInt32 GetSpearLevelUps()
	{
		NormalizeState(kSpearSkillIndex);
		return g_state.states[kSpearSkillIndex].levelUps;
	}

	static UInt32 GetSpearGoverningAttributeIncreases()
	{
		NormalizeState(kSpearSkillIndex);
		return g_state.states[kSpearSkillIndex].governingAttributeIncreaseCount;
	}

	static UInt32 GetSpearMastery()
	{
		NormalizeState(kSpearSkillIndex);
		return CalcMasteryFromSkill()(static_cast<SInt32>(g_state.states[kSpearSkillIndex].level));
	}

	static UInt32 GetSpearPerkMask()
	{
		NormalizeState(kSpearSkillIndex);
		const UInt32 level = g_state.states[kSpearSkillIndex].level;
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
		NormalizeState(kSpearSkillIndex);
		return IsEffectiveMajor(kSpearSkillIndex);
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
		if (!g_serialization || !g_serialization->WriteRecord(kRecordState, kSaveVersion, &g_state, sizeof(g_state)))
			_WARNING("SpearSkill: failed to write sidecar save record");
		if (!SaveWeaponTypeSidecars())
			_WARNING("SpearSkill: failed to write weapon Type sidecar save record");
	}

	static void LoadCallback(void*)
	{
		ResetState();
		ClearPendingWeaponSkillConsumer();
		ClearWeaponRatingNpcContext();
		ClearTrainingPolicyState();
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
			if (type == kRecordState)
			{
				if (version != kSaveVersion || length < sizeof(g_state))
				{
					_WARNING("SpearSkill: ignored incompatible save record version=%u length=%u", version, length);
					continue;
				}
				SaveState loaded = {};
				if (g_serialization->ReadRecordData(&loaded, sizeof(loaded)) == sizeof(loaded))
					g_state = loaded;
			}
			else if (type == kRecordWeaponTypes)
			{
				LoadWeaponTypeSidecars(version, length);
			}
			else
			{
				continue;
			}
		}

		for (UInt32 i = 0; i < kSkillCount; ++i)
			NormalizeState(i);
		LoadEditorWeaponTypeSidecars(true);
		LoadEditorNpcSpearSidecars(true);
		LoadEditorNpcSpearTrainingSidecars(true);
		SeedDefaultsFromPlayer();
		PatchVisibleGameSettingLabels();
	}

	static void NewGameCallback(void*)
	{
		ResetState();
		ClearPendingWeaponSkillConsumer();
		ClearWeaponRatingNpcContext();
		ClearTrainingPolicyState();
		g_weaponTypeStore.Clear();
		EnsureNpcSpearStoreConfigured();
		g_npcSkillStore.Clear();
		g_npcTrainingStore.Clear();
		LoadEditorWeaponTypeSidecars(false);
		LoadEditorNpcSpearSidecars(false);
		LoadEditorNpcSpearTrainingSidecars(false);
		SeedDefaultsFromPlayer();
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
			if (!InstallHooksOnce())
				_ERROR("SpearSkill: failed to install native hooks after OBSE plugin load");
			break;
		case OBSEMessagingInterface::kMessage_PostLoadGame:
			ClearPendingWeaponSkillConsumer();
			ClearWeaponRatingNpcContext();
			ClearTrainingPolicyState();
			LoadEditorWeaponTypeSidecars(true);
			LoadEditorNpcSpearSidecars(true);
			LoadEditorNpcSpearTrainingSidecars(true);
			SeedDefaultsFromPlayer();
			PatchVisibleGameSettingLabels();
			break;
		}
	}

	static bool SidecarIsSpearSkill(UInt32 skillId)
	{
		return skillId == SpearSkillShared::kSpearSkillId;
	}

	static bool SidecarCommandValueToLevel(double value, UInt32* outLevel)
	{
		if (outLevel)
			*outLevel = 0;
		if (!std::isfinite(value))
			return false;

		if (value <= 0.0)
		{
			if (outLevel)
				*outLevel = 0;
			return true;
		}
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

	static bool SidecarCommandValueToDelta(double value, SInt32* outDelta)
	{
		if (outDelta)
			*outDelta = 0;
		if (!std::isfinite(value))
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

	static TESNPC* SidecarCommandNpc(void* npc)
	{
		return AsNpcForm(static_cast<TESForm*>(npc));
	}

	static bool SidecarGetSpearAV(UInt32 skillId, double* outValue)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;
		if (outValue)
			*outValue = static_cast<double>(GetSpearSkill());
		return true;
	}

	static bool SidecarSetSpearAV(UInt32 skillId, double value)
	{
		UInt32 level = 0;
		if (!SidecarIsSpearSkill(skillId) || !SidecarCommandValueToLevel(value, &level))
			return false;

		SetSpearSkillClamped(level);
		return true;
	}

	static bool SidecarModSpearAV(UInt32 skillId, double value)
	{
		SInt32 delta = 0;
		if (!SidecarIsSpearSkill(skillId) || !SidecarCommandValueToDelta(value, &delta))
			return false;

		ModSpearSkill(delta);
		return true;
	}

	static bool SidecarAdvanceSpearSkill(UInt32 skillId, double value)
	{
		if (!SidecarIsSpearSkill(skillId) || !std::isfinite(value))
			return false;

		return AddSkillProgress(kSpearSkillIndex, static_cast<float>(value));
	}

	static bool SidecarGetSpearProgress(UInt32 skillId, double* outValue)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;
		if (outValue)
			*outValue = static_cast<double>(GetSpearProgress());
		return true;
	}

	static bool SidecarSetSpearProgress(UInt32 skillId, double value)
	{
		if (!SidecarIsSpearSkill(skillId) || !std::isfinite(value))
			return false;

		SetSpearProgress(static_cast<float>(value));
		return true;
	}

	static bool SidecarGetSpearRequiredProgress(UInt32 skillId, double* outValue)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;
		if (outValue)
			*outValue = static_cast<double>(GetSpearRequiredProgress());
		return true;
	}

	static bool SidecarGetSpearLevelUps(UInt32 skillId, double* outValue)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;
		if (outValue)
			*outValue = static_cast<double>(GetSpearLevelUps());
		return true;
	}

	static bool SidecarIsSpearWeapon(UInt32 skillId, void* form, bool* outResult)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;
		if (outResult)
			*outResult = IsSpearWeapon(static_cast<TESForm*>(form));
		return true;
	}

	static bool SidecarGetNpcSpearEntry(TESNPC* npc, SpearSkillShared::NpcSpearEntry* outEntry)
	{
		if (outEntry)
			std::memset(outEntry, 0, sizeof(*outEntry));
		if (!npc || !npc->refID)
			return false;

		EnsureNpcSpearStoreConfigured();
		return g_npcSkillStore.TryGet(npc->refID, outEntry);
	}

	static bool SidecarSetNpcSpearEntry(TESNPC* npc, UInt32 level, float progress, UInt32 levelUps)
	{
		if (!npc || !npc->refID)
			return false;

		EnsureNpcSpearStoreConfigured();
		return g_npcSkillStore.Set(npc->refID, level, progress, levelUps);
	}

	static bool SidecarGetNpcSpearAV(void* npcForm, UInt32 skillId, double* outValue)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;

		UInt32 level = 0;
		const bool found = TryGetNpcSpearSkill(SidecarCommandNpc(npcForm), &level);
		if (outValue)
			*outValue = static_cast<double>(level);
		return found;
	}

	static bool SidecarSetNpcSpearAV(void* npcForm, UInt32 skillId, double value)
	{
		UInt32 level = 0;
		TESNPC* npc = SidecarCommandNpc(npcForm);
		if (!SidecarIsSpearSkill(skillId) || !SidecarCommandValueToLevel(value, &level) || !npc)
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!SidecarGetNpcSpearEntry(npc, &entry))
		{
			entry.formId = npc->refID;
			entry.progress = 0.0f;
			entry.levelUps = 0;
		}

		return SidecarSetNpcSpearEntry(npc, level, entry.progress, entry.levelUps);
	}

	static bool SidecarModNpcSpearAV(void* npcForm, UInt32 skillId, double value)
	{
		SInt32 delta = 0;
		TESNPC* npc = SidecarCommandNpc(npcForm);
		if (!SidecarIsSpearSkill(skillId) || !SidecarCommandValueToDelta(value, &delta) || !npc)
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!SidecarGetNpcSpearEntry(npc, &entry))
		{
			entry.formId = npc->refID;
			entry.level = 0;
			entry.progress = 0.0f;
			entry.levelUps = 0;
		}

		if (delta <= 0)
		{
			const SInt64 adjustedLevel = static_cast<SInt64>(entry.level) + static_cast<SInt64>(delta);
			const UInt32 level = adjustedLevel <= 0 ? 0 : adjustedLevel >= 100 ? 100 : static_cast<UInt32>(adjustedLevel);
			return SidecarSetNpcSpearEntry(npc, level, entry.progress, entry.levelUps);
		}

		if (entry.level >= 100)
			return SidecarSetNpcSpearEntry(npc, entry.level, entry.progress, entry.levelUps);

		UInt32 increase = static_cast<UInt32>(delta);
		const UInt32 room = 100 - entry.level;
		if (increase > room)
			increase = room;

		float progressDebit = 0.0f;
		for (UInt32 advancingLevel = entry.level; advancingLevel < entry.level + increase; ++advancingLevel)
			progressDebit += RequiredProgressForLevel(kSpearSkillIndex, advancingLevel);
		if (std::isfinite(progressDebit) && progressDebit > 0.0f)
			entry.progress = entry.progress > progressDebit ? entry.progress - progressDebit : 0.0f;

		return SidecarSetNpcSpearEntry(npc, entry.level + increase, entry.progress, entry.levelUps + increase);
	}

	static bool SidecarGetNpcSpearProgress(void* npcForm, UInt32 skillId, double* outValue)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		const bool found = SidecarGetNpcSpearEntry(SidecarCommandNpc(npcForm), &entry);
		if (outValue)
			*outValue = found ? static_cast<double>(entry.progress) : 0.0;
		return found;
	}

	static bool SidecarSetNpcSpearProgress(void* npcForm, UInt32 skillId, double value)
	{
		TESNPC* npc = SidecarCommandNpc(npcForm);
		if (!SidecarIsSpearSkill(skillId) || !npc || !std::isfinite(value))
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		if (!SidecarGetNpcSpearEntry(npc, &entry))
		{
			entry.formId = npc->refID;
			entry.level = 0;
			entry.levelUps = 0;
		}

		return SidecarSetNpcSpearEntry(npc, entry.level, static_cast<float>(value), entry.levelUps);
	}

	static bool SidecarGetNpcSpearRequiredProgress(void* npcForm, UInt32 skillId, double* outValue)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		const bool found = SidecarGetNpcSpearEntry(SidecarCommandNpc(npcForm), &entry);
		if (outValue)
			*outValue = found ? static_cast<double>(RequiredProgressForLevel(kSpearSkillIndex, entry.level)) : 0.0;
		return found;
	}

	static bool SidecarGetNpcSpearLevelUps(void* npcForm, UInt32 skillId, double* outValue)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;

		SpearSkillShared::NpcSpearEntry entry = {};
		const bool found = SidecarGetNpcSpearEntry(SidecarCommandNpc(npcForm), &entry);
		if (outValue)
			*outValue = found ? static_cast<double>(entry.levelUps) : 0.0;
		return found;
	}

	static bool SidecarHasNpcSpearSkill(void* npcForm, UInt32 skillId, bool* outResult)
	{
		if (!SidecarIsSpearSkill(skillId))
			return false;

		TESNPC* npc = SidecarCommandNpc(npcForm);
		EnsureNpcSpearStoreConfigured();
		if (outResult)
			*outResult = npc && g_npcSkillStore.Has(npc->refID);
		return npc != nullptr;
	}

	static bool SidecarClearNpcSpearSkill(void* npcForm, UInt32 skillId)
	{
		TESNPC* npc = SidecarCommandNpc(npcForm);
		if (!SidecarIsSpearSkill(skillId) || !npc)
			return false;

		EnsureNpcSpearStoreConfigured();
		return g_npcSkillStore.Remove(npc->refID);
	}

	static const SidecarSkillCommandsShared::SidecarSkillProvider kSidecarSkillProviders[] =
	{
		{
			SidecarSkillCommandsShared::kSidecarSkillProviderApiVersion,
			sizeof(SidecarSkillCommandsShared::SidecarSkillProvider),
			SpearSkillShared::kSpearSkillId,
			"Spear",
			"SpearSkill|SPER",
			&SidecarGetSpearAV,
			&SidecarGetSpearAV,
			&SidecarSetSpearAV,
			&SidecarModSpearAV,
			&SidecarSetSpearAV,
			&SidecarAdvanceSpearSkill,
			&SidecarModSpearAV,
			&SidecarGetSpearProgress,
			&SidecarSetSpearProgress,
			&SidecarGetSpearRequiredProgress,
			&SidecarGetSpearLevelUps,
			&SidecarIsSpearWeapon,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			&SidecarGetNpcSpearAV,
			&SidecarGetNpcSpearAV,
			&SidecarSetNpcSpearAV,
			&SidecarModNpcSpearAV,
			&SidecarSetNpcSpearAV,
			&SidecarGetNpcSpearProgress,
			&SidecarSetNpcSpearProgress,
			&SidecarGetNpcSpearRequiredProgress,
			&SidecarGetNpcSpearLevelUps,
			&SidecarHasNpcSpearSkill,
			&SidecarClearNpcSpearSkill,
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

	static void RegisterMessaging(const OBSEInterface* obse)
	{
		if (!obse || !obse->QueryInterface || g_pluginHandle == kPluginHandle_Invalid)
			return;

		OBSEMessagingInterface* messaging =
			static_cast<OBSEMessagingInterface*>(obse->QueryInterface(kInterface_Messaging));
		if (messaging && messaging->RegisterListener)
			messaging->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
	}
}

extern "C"
{
	const SidecarSkillCommandsShared::SidecarSkillProviderTable* GetSidecarSkillProviderTable()
	{
		return SpearSkill::GetSidecarSkillProviderTableInternal();
	}

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
		SpearSkill::ResetState();
		SpearSkill::RegisterSerializationCallbacks();
		SpearSkill::RegisterMessaging(obse);
		return true;
	}
}

