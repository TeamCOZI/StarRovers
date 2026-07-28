#include "Editor/SRGenerateResourceV2ContentCommandlet.h"

#include "Utility/SRLog.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRResourceV2AuthoredContent.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	const TArray<FString> ProcessorTemplatePaths = {
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Starter/DA_Structure_SP1.DA_Structure_SP1"),
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Starter/DA_Structure_SP2.DA_Structure_SP2"),
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Basic/Basic_Processor/DA_Structure_BP1.DA_Structure_BP1"),
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Basic/Basic_Processor/DA_Structure_BP2.DA_Structure_BP2"),
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Advanced/Advanced_Processor/DA_Structure_AP1.DA_Structure_AP1"),
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Expert/Expert_Processor/DA_Structure_EP1.DA_Structure_EP1"),
	};

	const TArray<FString> SynthesizerTemplatePaths = {
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Starter/DA_Structure_SS1.DA_Structure_SS1"),
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Basic/Basic_Synthesizer/DA_Structure_BS1.DA_Structure_BS1"),
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Advanced/Advanced_Synthesizer/DA_Structure_AS1.DA_Structure_AS1"),
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Expert/Expert_Synthesizer/DA_Structure_ES1.DA_Structure_ES1"),
	};

	FString MakeResourceV2ObjectPath(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
	}

	template <typename TObjectType>
	TObjectType* LoadOrCreateResourceV2Asset(const FString& PackageName, bool& bOutCreated)
	{
		bOutCreated = false;
		if (PackageName.IsEmpty())
		{
			return nullptr;
		}

		if (FPackageName::DoesPackageExist(PackageName))
		{
			if (TObjectType* Existing = LoadObject<TObjectType>(nullptr, *MakeResourceV2ObjectPath(PackageName)))
			{
				return Existing;
			}
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!IsValid(Package))
		{
			return nullptr;
		}
		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		TObjectType* Asset = NewObject<TObjectType>(
			Package,
			AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		if (IsValid(Asset))
		{
			FAssetRegistryModule::AssetCreated(Asset);
			bOutCreated = true;
		}
		return Asset;
	}

	bool SaveResourceV2Asset(UObject* Asset)
	{
		if (!IsValid(Asset))
		{
			return false;
		}
		Asset->MarkPackageDirty();
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
	}

	void CopyStructureVisuals(
		const USRStructureDataAsset& Source,
		USRStructureDataAsset& Target)
	{
		Target.StructureActorClass = Source.StructureActorClass;
		Target.StaticMesh = Source.StaticMesh;
		Target.Material = Source.Material;
		Target.GhostMaterial = Source.GhostMaterial;
		Target.DeleteMaterial = Source.DeleteMaterial;
		Target.CopyPlaceableMaterial = Source.CopyPlaceableMaterial;
		Target.ReplaceableMaterial = Source.ReplaceableMaterial;
		Target.CopyBlockedMaterial = Source.CopyBlockedMaterial;
		Target.MeshRelativeLocation = Source.MeshRelativeLocation;
		Target.MeshRelativeRotation = Source.MeshRelativeRotation;
		Target.MeshRelativeScale = Source.MeshRelativeScale;
		Target.ConstructionHeightOffset = Source.ConstructionHeightOffset;
		Target.PlacementYawDegrees = Source.PlacementYawDegrees;
		Target.bAlignToSurfaceNormal = Source.bAlignToSurfaceNormal;
	}

	FSRStructurePortSpec MakePort(
		const TCHAR* Prefix,
		int32 Index,
		int32 CellX,
		int32 CellY,
		ESRStructurePortDirection Direction)
	{
		FSRStructurePortSpec Port;
		Port.PortId = FName(*FString::Printf(TEXT("%s_%d"), Prefix, Index));
		Port.CellOffsetX = CellX;
		Port.CellOffsetY = CellY;
		Port.Direction = Direction;
		return Port;
	}

	void ConfigureFacilityFootprintAndPorts(
		const USRFacilityDataAsset& Facility,
		USRStructureDataAsset& Structure)
	{
		const int32 InputCount = FMath::Max(0, Facility.InputInventory.SlotCount);
		const int32 OutputCount = FMath::Max(0, Facility.OutputInventory.SlotCount);
		Structure.InputPorts.Reset();
		Structure.OutputPorts.Reset();

		if (InputCount >= 5)
		{
			Structure.FootprintCellsX = 3;
			Structure.FootprintCellsY = 3;
			Structure.InputPorts.Add(MakePort(TEXT("Input"), 0, 0, 0, ESRStructurePortDirection::Left));
			Structure.InputPorts.Add(MakePort(TEXT("Input"), 1, 0, 1, ESRStructurePortDirection::Left));
			Structure.InputPorts.Add(MakePort(TEXT("Input"), 2, 0, 2, ESRStructurePortDirection::Left));
			Structure.InputPorts.Add(MakePort(TEXT("Input"), 3, 1, 0, ESRStructurePortDirection::Top));
			Structure.InputPorts.Add(MakePort(TEXT("Input"), 4, 1, 2, ESRStructurePortDirection::Bottom));
		}
		else if (InputCount >= 2)
		{
			Structure.FootprintCellsX = 2;
			Structure.FootprintCellsY = 2;
			for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
			{
				Structure.InputPorts.Add(MakePort(
					TEXT("Input"),
					InputIndex,
					0,
					FMath::Min(InputIndex, 1),
					ESRStructurePortDirection::Left));
			}
		}
		else
		{
			Structure.FootprintCellsX = 2;
			Structure.FootprintCellsY = 1;
			if (InputCount == 1)
			{
				Structure.InputPorts.Add(MakePort(TEXT("Input"), 0, 0, 0, ESRStructurePortDirection::Left));
			}
		}

		for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
		{
			Structure.OutputPorts.Add(MakePort(
				TEXT("Output"),
				OutputIndex,
				Structure.FootprintCellsX - 1,
				Structure.FootprintCellsY / 2,
				ESRStructurePortDirection::Right));
		}
	}

	FText BuildFacilityDescription(const FSRFacilityContentDefinitionV2& Definition)
	{
		if (Definition.OperationKind == ESRFacilityOperationKind::Synthesize)
		{
			return FText::Format(
				NSLOCTEXT("StarRoversResourceV2Authoring", "SynthesisDescription", "Resource V2 synthesis infrastructure. Base cycle: {0} seconds; Operational Load: {1}."),
				FText::AsNumber(Definition.CycleSeconds),
				FText::AsNumber(Definition.OperationalLoad));
		}

		const UEnum* FamilyEnum = StaticEnum<ESRResourceFamily>();
		const UEnum* LineRoleEnum = StaticEnum<ESRFacilityLineRoleV2>();
		const FString FamilyName = Definition.AcceptedFamily == ESRResourceFamily::None
			? TEXT("Any Card")
			: (FamilyEnum
				? FamilyEnum->GetDisplayNameTextByValue(static_cast<int64>(Definition.AcceptedFamily)).ToString()
				: TEXT("Card"));
		if (Definition.ProcessRole == ESRFacilityProcessRoleV2::FamilyProcess)
		{
			const FText LineRole = LineRoleEnum
				? LineRoleEnum->GetDisplayNameTextByValue(static_cast<int64>(Definition.LineRole))
				: FText::FromString(TEXT("Process"));
			if (Definition.LineRole == ESRFacilityLineRoleV2::UniversalBridge)
			{
				return FText::Format(
					NSLOCTEXT("StarRoversResourceV2Authoring", "BridgeDescription", "Universal Bridge. Adds {0} Energy in {1} seconds at Load {2}. It advances Family risk but cannot activate or consume positive Family merit."),
					FText::AsNumber(Definition.FacilityEnergyDelta),
					FText::AsNumber(Definition.CycleSeconds),
					FText::AsNumber(Definition.OperationalLoad));
			}
			return FText::Format(
				NSLOCTEXT("StarRoversResourceV2Authoring", "SpecialistDescription", "{0} specialist ({1}). Facility Energy: {2}; base cycle: {3} seconds; Operational Load: {4}. Positive merit follows the Family State rule."),
				FText::FromString(FamilyName),
				LineRole,
				FText::AsNumber(Definition.FacilityEnergyDelta),
				FText::AsNumber(Definition.CycleSeconds),
				FText::AsNumber(Definition.OperationalLoad));
		}

		return FText::Format(
			NSLOCTEXT("StarRoversResourceV2Authoring", "MutationDescription", "Transforms {0} without running a Family Energy process. Base cycle: {1} seconds; Operational Load: {2}."),
			FText::FromString(FamilyName),
			FText::AsNumber(Definition.CycleSeconds),
			FText::AsNumber(Definition.OperationalLoad));
	}

	const FString& GetDepositTemplatePath(ESRResourceContentPresetV2 Preset)
	{
		static const FString TerritePath(TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_TerriteLode.DA_Structure_TerriteLode"));
		static const FString AquidPath(TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_AquidLode.DA_Structure_AquidLode"));
		static const FString NitainPath(TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_NitainLode.DA_Structure_NitainLode"));
		static const FString Rock1Path(TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_Rock1.DA_Structure_Rock1"));
		static const FString Rock2Path(TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_Rock2.DA_Structure_Rock2"));
		static const FString Tree1Path(TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_Tree1.DA_Structure_Tree1"));
		static const FString Tree2Path(TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_Tree2.DA_Structure_Tree2"));
		switch (Preset)
		{
		case ESRResourceContentPresetV2::HeliosIron: return TerritePath;
		case ESRResourceContentPresetV2::EchoQuartz: return NitainPath;
		case ESRResourceContentPresetV2::VerdantSpore: return Tree1Path;
		case ESRResourceContentPresetV2::AuroraPlasma: return AquidPath;
		case ESRResourceContentPresetV2::NullPearl: return Rock2Path;
		case ESRResourceContentPresetV2::CommonOre: return Rock1Path;
		case ESRResourceContentPresetV2::BiomassFeedstock: return Tree2Path;
		default: return Rock1Path;
		}
	}
}

USRGenerateResourceV2ContentCommandlet::USRGenerateResourceV2ContentCommandlet()
{
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 USRGenerateResourceV2ContentCommandlet::Main(const FString& Params)
{
	(void)Params;
	int32 CreatedCount = 0;
	int32 UpdatedCount = 0;

	TArray<ESRResourceContentPresetV2> ResourcePresets;
	FSRResourceV2AuthoredContent::GetResourcePresets(ResourcePresets);
	for (const ESRResourceContentPresetV2 Preset : ResourcePresets)
	{
		bool bCreated = false;
		USRResourceDataAsset* Resource = LoadOrCreateResourceV2Asset<USRResourceDataAsset>(
			FSRResourceV2AuthoredContent::GetResourcePackageName(Preset),
			bCreated);
		if (!IsValid(Resource)
			|| !FSRResourceSystemContent::ApplyResourcePreset(*Resource, Preset)
			|| !SaveResourceV2Asset(Resource))
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to author Resource V2 preset %d."), static_cast<int32>(Preset));
			return 1;
		}
		bCreated ? ++CreatedCount : ++UpdatedCount;
	}

	TArray<FSRFacilityContentDefinitionV2> FacilityDefinitions;
	FSRResourceSystemContent::GetAllFacilityDefinitions(FacilityDefinitions);
	TMap<ESRFacilityContentPresetV2, TObjectPtr<USRFacilityDataAsset>> FacilitiesByPreset;
	for (const FSRFacilityContentDefinitionV2& Definition : FacilityDefinitions)
	{
		bool bCreated = false;
		USRFacilityDataAsset* Facility = LoadOrCreateResourceV2Asset<USRFacilityDataAsset>(
			FSRResourceV2AuthoredContent::GetFacilityPackageName(Definition.Preset),
			bCreated);
		if (!IsValid(Facility)
			|| !FSRResourceSystemContent::ApplyFacilityPreset(*Facility, Definition.Preset)
			|| !SaveResourceV2Asset(Facility))
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to author Facility V2 %s."), *Definition.ContentId.ToString());
			return 1;
		}
		FacilitiesByPreset.Add(Definition.Preset, Facility);
		bCreated ? ++CreatedCount : ++UpdatedCount;
	}

	for (int32 DefinitionIndex = 0; DefinitionIndex < FacilityDefinitions.Num(); ++DefinitionIndex)
	{
		const FSRFacilityContentDefinitionV2& Definition = FacilityDefinitions[DefinitionIndex];
		USRFacilityDataAsset* Facility = FacilitiesByPreset.FindRef(Definition.Preset);
		const TArray<FString>& TemplatePaths = Definition.OperationKind == ESRFacilityOperationKind::Synthesize
			? SynthesizerTemplatePaths
			: ProcessorTemplatePaths;
		USRStructureDataAsset* Template = LoadObject<USRStructureDataAsset>(
			nullptr,
			*TemplatePaths[DefinitionIndex % TemplatePaths.Num()]);
		bool bCreated = false;
		USRStructureDataAsset* Structure = LoadOrCreateResourceV2Asset<USRStructureDataAsset>(
			FSRResourceV2AuthoredContent::GetFacilityStructurePackageName(Definition.Preset),
			bCreated);
		if (!IsValid(Facility) || !IsValid(Template) || !IsValid(Structure))
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to resolve the authored Structure dependencies for %s."), *Definition.ContentId.ToString());
			return 1;
		}

		CopyStructureVisuals(*Template, *Structure);
		Structure->StructureId = FName(*FString::Printf(TEXT("V2_%s"), *Definition.ContentId.ToString()));
		Structure->DisplayName = Definition.DisplayName;
		Structure->Description = BuildFacilityDescription(Definition);
		Structure->bAvailableForConstruction = true;
		Structure->bDestroyableByConstruction = true;
		Structure->BuildKind = ESRStructureBuildKind::Structure;
		Structure->FacilityDataAsset = Facility;
		Structure->bProcessReady = true;
		Structure->bDeliveryReady = true;
		Structure->bIsResourceDeposit = false;
		Structure->DepositResourceDataAsset = nullptr;
		Structure->DepositTotalAmount = 0;
		ConfigureFacilityFootprintAndPorts(*Facility, *Structure);
		if (!SaveResourceV2Asset(Structure))
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to save the authored Structure for %s."), *Definition.ContentId.ToString());
			return 1;
		}
		bCreated ? ++CreatedCount : ++UpdatedCount;
	}

	TArray<ESRResourceContentPresetV2> DepositPresets;
	FSRResourceV2AuthoredContent::GetDepositResourcePresets(DepositPresets);
	TMap<ESRResourceContentPresetV2, TObjectPtr<USRStructureDataAsset>> DepositsByPreset;
	for (const ESRResourceContentPresetV2 Preset : DepositPresets)
	{
		int32 DepositTotalAmount = 0;
		if (!FSRResourceSystemContent::TryGetDepositTotalAmount(Preset, DepositTotalAmount))
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Missing finite deposit balance for preset %d."), static_cast<int32>(Preset));
			return 1;
		}
		USRResourceDataAsset* Resource = LoadObject<USRResourceDataAsset>(
			nullptr,
			*FSRResourceV2AuthoredContent::GetResourceObjectPath(Preset));
		USRStructureDataAsset* Template = LoadObject<USRStructureDataAsset>(nullptr, *GetDepositTemplatePath(Preset));
		bool bCreated = false;
		USRStructureDataAsset* Deposit = LoadOrCreateResourceV2Asset<USRStructureDataAsset>(
			FSRResourceV2AuthoredContent::GetDepositPackageName(Preset),
			bCreated);
		if (!IsValid(Resource) || !IsValid(Template) || !IsValid(Deposit))
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to resolve the authored Deposit dependencies for preset %d."), static_cast<int32>(Preset));
			return 1;
		}

		CopyStructureVisuals(*Template, *Deposit);
		Deposit->StructureId = FName(*FString::Printf(TEXT("DepositV2_%s"), *Resource->ResourceId.ToString()));
		Deposit->DisplayName = FText::Format(
			NSLOCTEXT("StarRoversResourceV2Authoring", "DepositName", "{0} Deposit"),
			Resource->DisplayName);
		Deposit->Description = FText::Format(
			NSLOCTEXT("StarRoversResourceV2Authoring", "DepositDescription", "A mineable Resource V2 deposit containing {0}."),
			Resource->DisplayName);
		Deposit->FootprintCellsX = FMath::Max(1, Template->FootprintCellsX);
		Deposit->FootprintCellsY = FMath::Max(1, Template->FootprintCellsY);
		Deposit->bAvailableForConstruction = false;
		Deposit->bDestroyableByConstruction = Template->bDestroyableByConstruction;
		Deposit->BuildKind = ESRStructureBuildKind::Structure;
		Deposit->FacilityDataAsset = nullptr;
		Deposit->bProcessReady = false;
		Deposit->bDeliveryReady = false;
		Deposit->bIsResourceDeposit = true;
		Deposit->DepositResourceDataAsset = Resource;
		Deposit->DepositTotalAmount = DepositTotalAmount;
		Deposit->InputPorts.Reset();
		Deposit->OutputPorts.Reset();
		if (!SaveResourceV2Asset(Deposit))
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to save the authored Deposit for %s."), *Resource->ResourceId.ToString());
			return 1;
		}
		DepositsByPreset.Add(Preset, Deposit);
		bCreated ? ++CreatedCount : ++UpdatedCount;
	}

	USRPlanetTerrainProfileDataAsset* TerrainProfile = LoadObject<USRPlanetTerrainProfileDataAsset>(
		nullptr,
		*FSRResourceV2AuthoredContent::GetTerrainProfileObjectPath());
	if (!IsValid(TerrainProfile))
	{
		SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to load the Earth terrain profile for Resource V2 deposit rules."));
		return 1;
	}
	for (const ESRResourceContentPresetV2 Preset : DepositPresets)
	{
		USRStructureDataAsset* Deposit = DepositsByPreset.FindRef(Preset);
		const USRResourceDataAsset* Resource = IsValid(Deposit)
			? Deposit->DepositResourceDataAsset.Get()
			: nullptr;
		if (!IsValid(Deposit) || !IsValid(Resource))
		{
			return 1;
		}
		const FName RuleId(*FString::Printf(TEXT("ResourceV2.%s"), *Resource->ResourceId.ToString()));
		FSRProfileNaturalStructureSpawnRule* Rule = TerrainProfile->ProfileNaturalStructureSpawnRules.FindByPredicate(
			[RuleId](const FSRProfileNaturalStructureSpawnRule& Candidate)
			{
				return Candidate.RuleId == RuleId;
			});
		if (!Rule)
		{
			Rule = &TerrainProfile->ProfileNaturalStructureSpawnRules.AddDefaulted_GetRef();
		}
		Rule->RuleId = RuleId;
		Rule->bEnabled = true;
		Rule->StructureDataAsset = Deposit;
		Rule->SpawnChancePerCell = 0.03f;
		Rule->MaxCount = 8;
		Rule->MinimumGuaranteedCount = 1;
		Rule->MinCellSpacing = 6;
	}
	if (!SaveResourceV2Asset(TerrainProfile))
	{
		SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to save Resource V2 deposit rules into the Earth terrain profile."));
		return 1;
	}
	++UpdatedCount;

	FSRResourceV2AuthoredContentValidation Validation;
	if (!FSRResourceV2AuthoredContent::ValidateAuthoredContent(Validation))
	{
		for (const FString& Error : Validation.Errors)
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Resource V2 authored content validation: %s"), *Error);
		}
		return 1;
	}

	SR_LOG(EditorCommandlet, LogTemp, Display,
		TEXT("Resource V2 authored content ready: Created=%d Updated=%d Resources=%d Facilities=%d Structures=%d Deposits=%d ProfileRules=%d"),
		CreatedCount,
		UpdatedCount,
		Validation.ResourceAssetCount,
		Validation.FacilityAssetCount,
		Validation.StructureAssetCount,
		Validation.DepositAssetCount,
		Validation.TerrainProfileRuleCount);
	return 0;
}

#else

USRGenerateResourceV2ContentCommandlet::USRGenerateResourceV2ContentCommandlet()
{
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
}

int32 USRGenerateResourceV2ContentCommandlet::Main(const FString& Params)
{
	(void)Params;
	return 1;
}

#endif
