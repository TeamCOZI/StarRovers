#include "Assembly/SRStructureBuildCatalog.h"

#include "Simulation/SRAugmentPackageContent.h"
#include "Simulation/SRAugmentSubsystem.h"

namespace
{
	FText BuildUnlockHint(const USRFacilityDataAsset* FacilityDataAsset)
	{
		if (!IsValid(FacilityDataAsset) || FacilityDataAsset->ResourceV2ContentId.IsNone())
		{
			return NSLOCTEXT(
				"StarRoversBuildCatalog",
				"GenericAugmentUnlockHint",
				"Obtain a matching Augment to unlock this structure.");
		}

		TArray<FSRAugmentPackageDefinitionV2> PackageDefinitions;
		FSRAugmentPackageContentV2::GetAllDefinitions(PackageDefinitions);
		TArray<FText> GrantingPackageNames;
		for (const FSRAugmentPackageDefinitionV2& Definition : PackageDefinitions)
		{
			if (Definition.GrantedFacilityContentIds.Contains(FacilityDataAsset->ResourceV2ContentId))
			{
				GrantingPackageNames.Add(Definition.DisplayName.IsEmpty()
					? FText::FromName(Definition.PackageId)
					: Definition.DisplayName);
			}
		}

		if (GrantingPackageNames.IsEmpty())
		{
			return NSLOCTEXT(
				"StarRoversBuildCatalog",
				"UnmappedAugmentUnlockHint",
				"This structure is controlled by the Augment system.");
		}

		return FText::Format(
			NSLOCTEXT("StarRoversBuildCatalog", "AugmentPackageUnlockHint", "Unlock source: {0}"),
			FText::Join(
				NSLOCTEXT("StarRoversBuildCatalog", "AugmentPackageSeparator", " / "),
				GrantingPackageNames));
	}

	FText ResolveDisplayName(const FSRStructureBuildOption& BuildOption)
	{
		return BuildOption.DisplayName.IsEmpty()
			? FText::FromName(BuildOption.StructureId)
			: BuildOption.DisplayName;
	}
}

void FSRStructureBuildCatalogBuilder::BuildCatalog(
	const TArray<USRStructureDataAsset*>& ConfiguredStructureDataAssets,
	const USRAugmentSubsystem* AugmentSubsystem,
	FSRStructureBuildCatalog& OutCatalog)
{
	OutCatalog = FSRStructureBuildCatalog();
	OutCatalog.ConfiguredAssetCount = ConfiguredStructureDataAssets.Num();
	OutCatalog.BuildOptions.Reserve(ConfiguredStructureDataAssets.Num());

	TSet<FName> AddedStructureIds;
	AddedStructureIds.Reserve(ConfiguredStructureDataAssets.Num());
	for (USRStructureDataAsset* StructureDataAsset : ConfiguredStructureDataAssets)
	{
		if (!IsValid(StructureDataAsset))
		{
			++OutCatalog.ExcludedInvalidAssetCount;
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.StructureId.IsNone())
		{
			++OutCatalog.ExcludedInvalidAssetCount;
			continue;
		}
		if (StructureData.bIsResourceDeposit)
		{
			++OutCatalog.ExcludedNaturalDepositCount;
			continue;
		}
		if (AddedStructureIds.Contains(StructureData.StructureId))
		{
			++OutCatalog.ExcludedDuplicateIdCount;
			continue;
		}

		const bool bUnlocked = !AugmentSubsystem
			|| AugmentSubsystem->IsStructureUnlocked(StructureDataAsset);
		FSRStructureBuildOption BuildOption;
		if (!TryBuildOption(StructureDataAsset, bUnlocked, BuildOption))
		{
			++OutCatalog.ExcludedInvalidAssetCount;
			continue;
		}

		AddedStructureIds.Add(StructureData.StructureId);
		OutCatalog.BuildOptions.Add(MoveTemp(BuildOption));
	}
}

bool FSRStructureBuildCatalogBuilder::TryBuildOption(
	USRStructureDataAsset* StructureDataAsset,
	bool bUnlocked,
	FSRStructureBuildOption& OutBuildOption)
{
	OutBuildOption = FSRStructureBuildOption();
	if (!IsValid(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (StructureData.StructureId.IsNone() || StructureData.bIsResourceDeposit)
	{
		return false;
	}

	OutBuildOption.StructureId = StructureData.StructureId;
	OutBuildOption.DisplayName = StructureData.DisplayName;
	OutBuildOption.Description = StructureData.Description;
	OutBuildOption.StructureDataAsset = StructureDataAsset;
	OutBuildOption.bUnlocked = bUnlocked;
	OutBuildOption.Role = ResolveRole(StructureData);
	OutBuildOption.ResourceFamily = ResolveResourceFamily(StructureData);
	OutBuildOption.BuildKind = StructureData.BuildKind;
	OutBuildOption.FootprintCellsX = FMath::Max(1, StructureData.FootprintCellsX);
	OutBuildOption.FootprintCellsY = FMath::Max(1, StructureData.FootprintCellsY);
	OutBuildOption.InputPortCount = StructureData.InputPorts.Num();
	OutBuildOption.OutputPortCount = StructureData.OutputPorts.Num();

	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	if (IsValid(FacilityDataAsset))
	{
		OutBuildOption.Rarity = FacilityDataAsset->Rarity;
		OutBuildOption.FacilityContentId = FacilityDataAsset->ResourceV2ContentId;
		OutBuildOption.OperationKind = FacilityDataAsset->OperationKind;
		OutBuildOption.ProcessRole = FacilityDataAsset->ResourceV2Process.ProcessRole;
		OutBuildOption.LineRole = FacilityDataAsset->ResourceV2Process.LineRole;
		OutBuildOption.ProcessArchetype = FacilityDataAsset->ResourceV2Process.ProcessArchetype;
		OutBuildOption.FamilyAction = FacilityDataAsset->ResourceV2Process.FamilyAction;
		OutBuildOption.FacilityEnergyDelta = FacilityDataAsset->ResourceV2Process.FacilityEnergyDelta;
		OutBuildOption.ProcessTagId = FacilityDataAsset->ResourceV2Process.ProcessTagId;
		OutBuildOption.FuelImprintId = FacilityDataAsset->ResourceV2Process.FuelImprintId;
		OutBuildOption.SynthesisRole = FacilityDataAsset->ResourceV2Synthesis.SynthesisRole;
		OutBuildOption.OperationalLoad = FMath::Max(0, FacilityDataAsset->OperationalLoad);
		OutBuildOption.OperationalPriority = FacilityDataAsset->DefaultOperationalPriority;
		OutBuildOption.BaseProcessSeconds = FMath::Max(0.0f, FacilityDataAsset->BaseProcessSeconds);
	}

	if (!StructureData.bAvailableForConstruction)
	{
		OutBuildOption.bEnabled = false;
		OutBuildOption.Availability = ESRStructureBuildAvailability::ConstructionDisabled;
		OutBuildOption.BlockReason = ESRStructureBuildBlockReason::ConstructionDisabled;
		OutBuildOption.BlockReasonText = NSLOCTEXT(
			"StarRoversBuildCatalog",
			"ConstructionDisabledReason",
			"This structure is not available for construction.");
		return true;
	}

	if (!bUnlocked)
	{
		OutBuildOption.bEnabled = false;
		OutBuildOption.Availability = ESRStructureBuildAvailability::LockedByAugment;
		OutBuildOption.BlockReason = ESRStructureBuildBlockReason::RequiresAugment;
		OutBuildOption.BlockReasonText = NSLOCTEXT(
			"StarRoversBuildCatalog",
			"RequiresAugmentReason",
			"Requires an Augment unlock.");
		OutBuildOption.UnlockHintText = BuildUnlockHint(FacilityDataAsset);
		return true;
	}

	OutBuildOption.bEnabled = true;
	OutBuildOption.Availability = ESRStructureBuildAvailability::Available;
	OutBuildOption.BlockReason = ESRStructureBuildBlockReason::None;
	return true;
}

ESRStructureBuildRole FSRStructureBuildCatalogBuilder::ResolveRole(const FSRStructureData& StructureData)
{
	if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
	{
		return ESRStructureBuildRole::Logistics;
	}

	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return ESRStructureBuildRole::General;
	}
	if (FacilityDataAsset->FacilityKind == ESRFacilityKind::Hub)
	{
		return ESRStructureBuildRole::Hub;
	}

	switch (FacilityDataAsset->OperationKind)
	{
	case ESRFacilityOperationKind::Mine:
		return ESRStructureBuildRole::Extraction;
	case ESRFacilityOperationKind::Process:
		switch (FacilityDataAsset->ResourceV2Process.ProcessRole)
		{
		case ESRFacilityProcessRoleV2::FamilyProcess:
			return ESRStructureBuildRole::FamilyProcessing;
		case ESRFacilityProcessRoleV2::ApplyProcessTag:
		case ESRFacilityProcessRoleV2::ClearProcessTag:
			return ESRStructureBuildRole::TagProcessing;
		case ESRFacilityProcessRoleV2::ApplyFuelImprint:
			return ESRStructureBuildRole::FuelImprinting;
		default:
			return ESRStructureBuildRole::General;
		}
	case ESRFacilityOperationKind::Synthesize:
		switch (FacilityDataAsset->ResourceV2Synthesis.SynthesisRole)
		{
		case ESRFacilitySynthesisRoleV2::StellarFuelFabricator:
			return ESRStructureBuildRole::StellarFuelFabrication;
		case ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator:
		case ESRFacilitySynthesisRoleV2::ServiceCore:
		case ESRFacilitySynthesisRoleV2::FleetBerth:
			return ESRStructureBuildRole::Infrastructure;
		default:
			return ESRStructureBuildRole::Synthesis;
		}
	default:
		return ESRStructureBuildRole::General;
	}
}

ESRResourceFamily FSRStructureBuildCatalogBuilder::ResolveResourceFamily(const FSRStructureData& StructureData)
{
	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset)
		|| FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Process
		|| FacilityDataAsset->FacilityDefinitionVersion < StarRovers::Facilities::CurrentFacilityDefinitionVersion)
	{
		return ESRResourceFamily::None;
	}

	return FacilityDataAsset->ResourceV2Process.AcceptedFamily;
}

FText FSRStructureBuildCatalogBuilder::BuildStatusText(const FSRStructureBuildOption& BuildOption)
{
	if (BuildOption.IsSelectable())
	{
		return FText::GetEmpty();
	}
	if (!BuildOption.UnlockHintText.IsEmpty())
	{
		return BuildOption.UnlockHintText;
	}
	return BuildOption.BlockReasonText;
}

FText FSRStructureBuildCatalogBuilder::BuildToolTipText(const FSRStructureBuildOption& BuildOption)
{
	TArray<FText> Lines;
	Lines.Add(ResolveDisplayName(BuildOption));
	if (!BuildOption.Description.IsEmpty())
	{
		Lines.Add(BuildOption.Description);
	}
	const FText StatusText = BuildStatusText(BuildOption);
	if (!StatusText.IsEmpty())
	{
		Lines.Add(StatusText);
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}
