#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceDataAsset.h"
#include "Simulation/SRSimulationSettings.h"

class USRStructureDataAsset;

struct STARROVERS_API FSRResourceV2AuthoredContentValidation
{
	int32 ResourceAssetCount = 0;
	int32 FacilityAssetCount = 0;
	int32 StructureAssetCount = 0;
	int32 DepositAssetCount = 0;
	int32 TerrainProfileRuleCount = 0;
	TArray<FString> Errors;

	bool IsValid() const
	{
		return Errors.IsEmpty();
	}
};

// Stable object paths shared by the editor authoring commandlet, runtime build
// catalog, natural-structure ruleset filter, and validation. The C++ reference
// catalog remains the mechanical source of truth; this class describes its
// persistent authored representation.
class STARROVERS_API FSRResourceV2AuthoredContent final
{
public:
	static void GetFacilityPresets(TArray<ESRFacilityContentPresetV2>& OutPresets);
	static void GetResourcePresets(TArray<ESRResourceContentPresetV2>& OutPresets);
	static void GetDepositResourcePresets(TArray<ESRResourceContentPresetV2>& OutPresets);

	static FString GetFacilityPackageName(ESRFacilityContentPresetV2 Preset);
	static FString GetFacilityObjectPath(ESRFacilityContentPresetV2 Preset);
	static FString GetFacilityStructurePackageName(ESRFacilityContentPresetV2 Preset);
	static FString GetFacilityStructureObjectPath(ESRFacilityContentPresetV2 Preset);
	static FString GetResourcePackageName(ESRResourceContentPresetV2 Preset);
	static FString GetResourceObjectPath(ESRResourceContentPresetV2 Preset);
	static FString GetDepositPackageName(ESRResourceContentPresetV2 Preset);
	static FString GetDepositObjectPath(ESRResourceContentPresetV2 Preset);
	static FString GetTerrainProfileObjectPath();

	static void GetFacilityStructureObjectPaths(TArray<FSoftObjectPath>& OutPaths);
	static void LoadFacilityStructureDataAssets(TArray<USRStructureDataAsset*>& OutAssets);

	static bool IsResourceV2FacilityStructure(const USRStructureDataAsset* StructureDataAsset);
	static bool IsLegacyProcessingStructure(const USRStructureDataAsset* StructureDataAsset);
	static bool ShouldGenerateNaturalStructure(
		const USRStructureDataAsset* StructureDataAsset,
		ESRResourceRulesetVersion RulesetVersion);

	static bool ValidateAuthoredContent(FSRResourceV2AuthoredContentValidation& OutValidation);
};
