#include "Automation/SRResourceV2AuthoredContent.h"

#include "Automation/SRResourceSystemContent.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	constexpr TCHAR FacilityRoot[] = TEXT("/Game/StarRovers/Automation/V2/Facilities");
	constexpr TCHAR ResourceRoot[] = TEXT("/Game/StarRovers/Automation/V2/Resources");
	constexpr TCHAR FacilityStructureRoot[] = TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/ResourceV2");
	constexpr TCHAR DepositRoot[] = TEXT("/Game/StarRovers/Structure/DataAssets/Natural/ResourceV2");
	constexpr TCHAR TerrainProfilePath[] = TEXT("/Game/StarRovers/Surface/DataAssets/TerrainProfiles/DA_Profile_Earth.DA_Profile_Earth");

	FString MakeObjectPath(const FString& PackageName)
	{
		if (PackageName.IsEmpty())
		{
			return FString();
		}
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
	}

	FName GetResourceId(ESRResourceContentPresetV2 Preset)
	{
		FSRReferenceResourceDefinitionV2 CardDefinition;
		if (FSRResourceSystemContent::TryGetReferenceResourceDefinition(Preset, CardDefinition))
		{
			return CardDefinition.ResourceId;
		}

		FSRUtilityResourceDefinitionV2 UtilityDefinition;
		return FSRResourceSystemContent::TryGetUtilityResourceDefinition(Preset, UtilityDefinition)
			? UtilityDefinition.ResourceId
			: NAME_None;
	}

	FName GetFacilityContentId(ESRFacilityContentPresetV2 Preset)
	{
		FSRFacilityContentDefinitionV2 Definition;
		return FSRResourceSystemContent::TryGetFacilityDefinition(Preset, Definition)
			? Definition.ContentId
			: NAME_None;
	}

	template <typename TObjectType>
	TObjectType* LoadAsset(const FString& ObjectPath)
	{
		return ObjectPath.IsEmpty()
			? nullptr
			: LoadObject<TObjectType>(nullptr, *ObjectPath);
	}

	void AddValidationError(
		FSRResourceV2AuthoredContentValidation& Validation,
		const FString& Message)
	{
		Validation.Errors.Add(Message);
	}
}

void FSRResourceV2AuthoredContent::GetFacilityPresets(TArray<ESRFacilityContentPresetV2>& OutPresets)
{
	OutPresets.Reset();
	TArray<FSRFacilityContentDefinitionV2> Definitions;
	FSRResourceSystemContent::GetAllFacilityDefinitions(Definitions);
	OutPresets.Reserve(Definitions.Num());
	for (const FSRFacilityContentDefinitionV2& Definition : Definitions)
	{
		if (Definition.Preset != ESRFacilityContentPresetV2::Custom)
		{
			OutPresets.AddUnique(Definition.Preset);
		}
	}
}

void FSRResourceV2AuthoredContent::GetResourcePresets(TArray<ESRResourceContentPresetV2>& OutPresets)
{
	OutPresets.Reset();
	TArray<FSRReferenceResourceDefinitionV2> CardDefinitions;
	TArray<FSRUtilityResourceDefinitionV2> UtilityDefinitions;
	FSRResourceSystemContent::GetAllReferenceResourceDefinitions(CardDefinitions);
	FSRResourceSystemContent::GetAllUtilityResourceDefinitions(UtilityDefinitions);
	OutPresets.Reserve(CardDefinitions.Num() + UtilityDefinitions.Num());
	for (const FSRReferenceResourceDefinitionV2& Definition : CardDefinitions)
	{
		OutPresets.AddUnique(Definition.Preset);
	}
	for (const FSRUtilityResourceDefinitionV2& Definition : UtilityDefinitions)
	{
		OutPresets.AddUnique(Definition.Preset);
	}
}

void FSRResourceV2AuthoredContent::GetDepositResourcePresets(TArray<ESRResourceContentPresetV2>& OutPresets)
{
	OutPresets = {
		ESRResourceContentPresetV2::HeliosIron,
		ESRResourceContentPresetV2::EchoQuartz,
		ESRResourceContentPresetV2::VerdantSpore,
		ESRResourceContentPresetV2::AuroraPlasma,
		ESRResourceContentPresetV2::NullPearl,
		ESRResourceContentPresetV2::CommonOre,
		ESRResourceContentPresetV2::BiomassFeedstock,
	};
}

FString FSRResourceV2AuthoredContent::GetFacilityPackageName(ESRFacilityContentPresetV2 Preset)
{
	const FName ContentId = GetFacilityContentId(Preset);
	return ContentId.IsNone()
		? FString()
		: FString::Printf(TEXT("%s/DA_FacilityV2_%s"), FacilityRoot, *ContentId.ToString());
}

FString FSRResourceV2AuthoredContent::GetFacilityObjectPath(ESRFacilityContentPresetV2 Preset)
{
	return MakeObjectPath(GetFacilityPackageName(Preset));
}

FString FSRResourceV2AuthoredContent::GetFacilityStructurePackageName(ESRFacilityContentPresetV2 Preset)
{
	const FName ContentId = GetFacilityContentId(Preset);
	return ContentId.IsNone()
		? FString()
		: FString::Printf(TEXT("%s/DA_StructureV2_%s"), FacilityStructureRoot, *ContentId.ToString());
}

FString FSRResourceV2AuthoredContent::GetFacilityStructureObjectPath(ESRFacilityContentPresetV2 Preset)
{
	return MakeObjectPath(GetFacilityStructurePackageName(Preset));
}

FString FSRResourceV2AuthoredContent::GetResourcePackageName(ESRResourceContentPresetV2 Preset)
{
	const FName ResourceId = GetResourceId(Preset);
	return ResourceId.IsNone()
		? FString()
		: FString::Printf(TEXT("%s/DA_ResourceV2_%s"), ResourceRoot, *ResourceId.ToString());
}

FString FSRResourceV2AuthoredContent::GetResourceObjectPath(ESRResourceContentPresetV2 Preset)
{
	return MakeObjectPath(GetResourcePackageName(Preset));
}

FString FSRResourceV2AuthoredContent::GetDepositPackageName(ESRResourceContentPresetV2 Preset)
{
	const FName ResourceId = GetResourceId(Preset);
	return ResourceId.IsNone()
		? FString()
		: FString::Printf(TEXT("%s/DA_DepositV2_%s"), DepositRoot, *ResourceId.ToString());
}

FString FSRResourceV2AuthoredContent::GetDepositObjectPath(ESRResourceContentPresetV2 Preset)
{
	return MakeObjectPath(GetDepositPackageName(Preset));
}

FString FSRResourceV2AuthoredContent::GetTerrainProfileObjectPath()
{
	return TerrainProfilePath;
}

void FSRResourceV2AuthoredContent::GetFacilityStructureObjectPaths(TArray<FSoftObjectPath>& OutPaths)
{
	OutPaths.Reset();
	TArray<ESRFacilityContentPresetV2> Presets;
	GetFacilityPresets(Presets);
	OutPaths.Reserve(Presets.Num());
	for (const ESRFacilityContentPresetV2 Preset : Presets)
	{
		const FString ObjectPath = GetFacilityStructureObjectPath(Preset);
		if (!ObjectPath.IsEmpty())
		{
			OutPaths.Emplace(ObjectPath);
		}
	}
}

void FSRResourceV2AuthoredContent::LoadFacilityStructureDataAssets(TArray<USRStructureDataAsset*>& OutAssets)
{
	OutAssets.Reset();
	TArray<FSoftObjectPath> Paths;
	GetFacilityStructureObjectPaths(Paths);
	OutAssets.Reserve(Paths.Num());
	for (const FSoftObjectPath& Path : Paths)
	{
		if (USRStructureDataAsset* Asset = Cast<USRStructureDataAsset>(Path.TryLoad()))
		{
			OutAssets.AddUnique(Asset);
		}
	}
}

bool FSRResourceV2AuthoredContent::IsResourceV2FacilityStructure(
	const USRStructureDataAsset* StructureDataAsset)
{
	if (!IsValid(StructureDataAsset))
	{
		return false;
	}
	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	return StructureData.bAvailableForConstruction
		&& !StructureData.bIsResourceDeposit
		&& IsValid(FacilityDataAsset)
		&& FacilityDataAsset->FacilityDefinitionVersion >= StarRovers::Facilities::CurrentFacilityDefinitionVersion
		&& !FacilityDataAsset->ResourceV2ContentId.IsNone();
}

bool FSRResourceV2AuthoredContent::IsLegacyProcessingStructure(
	const USRStructureDataAsset* StructureDataAsset)
{
	if (!IsValid(StructureDataAsset))
	{
		return false;
	}
	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	return IsValid(FacilityDataAsset)
		&& FacilityDataAsset->FacilityKind == ESRFacilityKind::Standard
		&& FacilityDataAsset->FacilityDefinitionVersion < StarRovers::Facilities::CurrentFacilityDefinitionVersion
		&& (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process
			|| FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize);
}

bool FSRResourceV2AuthoredContent::ShouldGenerateNaturalStructure(
	const USRStructureDataAsset* StructureDataAsset,
	ESRResourceRulesetVersion RulesetVersion)
{
	if (!IsValid(StructureDataAsset))
	{
		return false;
	}
	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (!StructureData.bIsResourceDeposit)
	{
		return true;
	}

	const USRResourceDataAsset* ResourceDataAsset = StructureData.DepositResourceDataAsset.Get();
	const bool bResourceV2Deposit = IsValid(ResourceDataAsset)
		&& ResourceDataAsset->ResourceDefinitionVersion >= StarRovers::Resources::CurrentResourceDefinitionVersion;
	const bool bResourceV2Ruleset = RulesetVersion == ESRResourceRulesetVersion::ResourceV2;
	return bResourceV2Deposit == bResourceV2Ruleset;
}

bool FSRResourceV2AuthoredContent::ValidateAuthoredContent(
	FSRResourceV2AuthoredContentValidation& OutValidation)
{
	OutValidation = FSRResourceV2AuthoredContentValidation();

	TSet<FName> StructureIds;
	TArray<ESRFacilityContentPresetV2> FacilityPresets;
	GetFacilityPresets(FacilityPresets);
	for (const ESRFacilityContentPresetV2 Preset : FacilityPresets)
	{
		FSRFacilityContentDefinitionV2 Definition;
		const bool bHasDefinition = FSRResourceSystemContent::TryGetFacilityDefinition(Preset, Definition);
		USRFacilityDataAsset* Facility = LoadAsset<USRFacilityDataAsset>(GetFacilityObjectPath(Preset));
		USRStructureDataAsset* Structure = LoadAsset<USRStructureDataAsset>(GetFacilityStructureObjectPath(Preset));
		if (IsValid(Facility))
		{
			++OutValidation.FacilityAssetCount;
		}
		if (IsValid(Structure))
		{
			++OutValidation.StructureAssetCount;
		}
		if (!bHasDefinition || !IsValid(Facility))
		{
			AddValidationError(OutValidation, FString::Printf(TEXT("Missing authored V2 Facility for preset %d."), static_cast<int32>(Preset)));
			continue;
		}
		if (Facility->ResourceV2Preset != Preset
			|| Facility->ResourceV2ContentId != Definition.ContentId
			|| Facility->FacilityDefinitionVersion < StarRovers::Facilities::CurrentFacilityDefinitionVersion)
		{
			AddValidationError(OutValidation, FString::Printf(TEXT("Authored V2 Facility %s does not match its C++ preset."), *Definition.ContentId.ToString()));
		}
		if (!IsValid(Structure))
		{
			AddValidationError(OutValidation, FString::Printf(TEXT("Missing authored V2 Structure for %s."), *Definition.ContentId.ToString()));
			continue;
		}

		const FSRStructureData StructureData = Structure->BuildData();
		if (StructureData.FacilityDataAsset != Facility
			|| !IsResourceV2FacilityStructure(Structure)
			|| StructureData.InputPorts.Num() < Facility->InputInventory.SlotCount
			|| StructureData.OutputPorts.Num() < Facility->OutputInventory.SlotCount)
		{
			AddValidationError(OutValidation, FString::Printf(TEXT("Authored V2 Structure %s has an invalid Facility link or disconnected inventory ports."), *Definition.ContentId.ToString()));
		}
		if (StructureData.StructureId.IsNone() || StructureIds.Contains(StructureData.StructureId))
		{
			AddValidationError(OutValidation, FString::Printf(TEXT("Authored V2 Structure %s has a missing or duplicate StructureId."), *Definition.ContentId.ToString()));
		}
		StructureIds.Add(StructureData.StructureId);
	}

	TArray<ESRResourceContentPresetV2> ResourcePresets;
	GetResourcePresets(ResourcePresets);
	for (const ESRResourceContentPresetV2 Preset : ResourcePresets)
	{
		USRResourceDataAsset* Resource = LoadAsset<USRResourceDataAsset>(GetResourceObjectPath(Preset));
		if (IsValid(Resource))
		{
			++OutValidation.ResourceAssetCount;
		}
		if (!IsValid(Resource)
			|| Resource->ResourceV2Preset != Preset
			|| Resource->ResourceDefinitionVersion < StarRovers::Resources::CurrentResourceDefinitionVersion)
		{
			AddValidationError(OutValidation, FString::Printf(TEXT("Missing or invalid authored V2 Resource for preset %d."), static_cast<int32>(Preset)));
		}
	}

	TArray<ESRResourceContentPresetV2> DepositPresets;
	GetDepositResourcePresets(DepositPresets);
	TSet<FName> ExpectedDepositRuleIds;
	for (const ESRResourceContentPresetV2 Preset : DepositPresets)
	{
		int32 ExpectedDepositTotalAmount = 0;
		const bool bHasDepositBalance =
			FSRResourceSystemContent::TryGetDepositTotalAmount(
				Preset,
				ExpectedDepositTotalAmount);
		USRResourceDataAsset* Resource = LoadAsset<USRResourceDataAsset>(GetResourceObjectPath(Preset));
		USRStructureDataAsset* Deposit = LoadAsset<USRStructureDataAsset>(GetDepositObjectPath(Preset));
		const FName ResourceId = GetResourceId(Preset);
		ExpectedDepositRuleIds.Add(FName(*FString::Printf(TEXT("ResourceV2.%s"), *ResourceId.ToString())));
		if (IsValid(Deposit))
		{
			++OutValidation.DepositAssetCount;
		}
		if (!IsValid(Deposit))
		{
			AddValidationError(OutValidation, FString::Printf(TEXT("Missing authored V2 Deposit for %s."), *ResourceId.ToString()));
			continue;
		}
		const FSRStructureData DepositData = Deposit->BuildData();
		if (!DepositData.bIsResourceDeposit
			|| DepositData.DepositResourceDataAsset != Resource
			|| !bHasDepositBalance
			|| DepositData.DepositTotalAmount != ExpectedDepositTotalAmount
			|| !ShouldGenerateNaturalStructure(Deposit, ESRResourceRulesetVersion::ResourceV2)
			|| ShouldGenerateNaturalStructure(Deposit, ESRResourceRulesetVersion::Legacy))
		{
			AddValidationError(OutValidation, FString::Printf(TEXT("Authored V2 Deposit %s has an invalid resource or ruleset contract."), *ResourceId.ToString()));
		}
	}

	USRPlanetTerrainProfileDataAsset* TerrainProfile =
		LoadAsset<USRPlanetTerrainProfileDataAsset>(GetTerrainProfileObjectPath());
	if (!IsValid(TerrainProfile))
	{
		AddValidationError(OutValidation, TEXT("The Earth terrain profile used for V2 deposit rules is missing."));
	}
	else
	{
		for (const FSRProfileNaturalStructureSpawnRule& Rule : TerrainProfile->ProfileNaturalStructureSpawnRules)
		{
			if (ExpectedDepositRuleIds.Contains(Rule.RuleId)
				&& Rule.bEnabled
				&& Rule.MinimumGuaranteedCount > 0
				&& IsValid(Rule.StructureDataAsset))
			{
				++OutValidation.TerrainProfileRuleCount;
			}
		}
		if (OutValidation.TerrainProfileRuleCount != ExpectedDepositRuleIds.Num())
		{
			AddValidationError(OutValidation, TEXT("The Earth terrain profile does not contain every Resource V2 deposit rule."));
		}
	}

	return OutValidation.IsValid();
}
