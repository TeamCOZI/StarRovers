#include "Automation/SROperationalEconomyProcessor.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceSystemContent.h"
#include "Simulation/SRSimulationSettings.h"

namespace
{
	FSROperationalEconomyEvaluationV2 MakeFailure(
		ESROperationalEconomyOutcomeV2 Outcome,
		ESRFacilitySynthesisRoleV2 Role,
		const FString& FailureReason)
	{
		FSROperationalEconomyEvaluationV2 Evaluation;
		Evaluation.Outcome = Outcome;
		Evaluation.SynthesisRole = Role;
		Evaluation.FailureReason = FailureReason;
		return Evaluation;
	}

	bool IsCurrentUtility(const FSRResourceInstance& Resource, FName ExpectedResourceId)
	{
		return !ExpectedResourceId.IsNone()
			&& Resource.ResourceSchemaVersion == StarRovers::Resources::CurrentResourceSchemaVersion
			&& Resource.ResourceClass == ESRResourceClass::Utility
			&& Resource.ResourceId == ExpectedResourceId
			&& Resource.StackCount > 0;
	}

	bool InventoryContainsIndustrialSupply(const TArray<FSRResourceInstance>& Inventory)
	{
		const FName SupplyId = FSRResourceSystemContent::GetUtilityResourceId(
			ESRResourceContentPresetV2::IndustrialSupply);
		for (const FSRResourceInstance& Resource : Inventory)
		{
			if (IsCurrentUtility(Resource, SupplyId))
			{
				return true;
			}
		}
		return false;
	}

	bool IsSuppliedInfrastructure(
		const FSRFacilityInstance& FacilityInstance,
		ESRFacilitySynthesisRoleV2 ExpectedRole)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		FString DefinitionFailure;
		if (!FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset)
			|| FacilityDataAsset->ResourceV2Synthesis.SynthesisRole != ExpectedRole
			|| !FacilityInstance.bProcessEnabled
			|| FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Frozen
			|| FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Overheated
			|| !FSROperationalEconomyProcessor::ValidateFacilityDefinition(FacilityDataAsset, DefinitionFailure))
		{
			return false;
		}
		if (InventoryContainsIndustrialSupply(FacilityInstance.ProcessingInventory))
		{
			return true;
		}
		for (const FSRFacilityPortInventory& Port : FacilityInstance.InputPortInventories)
		{
			if (InventoryContainsIndustrialSupply(Port.Inventory))
			{
				return true;
			}
		}
		return false;
	}
}

bool FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(
	const USRFacilityDataAsset* FacilityDataAsset)
{
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	if (!IsValid(Settings)
		|| Settings->ResourceRulesetVersion != ESRResourceRulesetVersion::ResourceV2
		|| !IsValid(FacilityDataAsset)
		|| FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Synthesize)
	{
		return false;
	}

	const ESRFacilitySynthesisRoleV2 Role = FacilityDataAsset->ResourceV2Synthesis.SynthesisRole;
	return Role == ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator
		|| Role == ESRFacilitySynthesisRoleV2::ServiceCore
		|| Role == ESRFacilitySynthesisRoleV2::FleetBerth;
}

bool FSROperationalEconomyProcessor::ValidateFacilityDefinition(
	const USRFacilityDataAsset* FacilityDataAsset,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!IsValid(FacilityDataAsset))
	{
		OutFailureReason = TEXT("Facility Data Asset is invalid.");
		return false;
	}
	if (FacilityDataAsset->FacilityDefinitionVersion
		!= StarRovers::Facilities::CurrentFacilityDefinitionVersion)
	{
		OutFailureReason = TEXT("Operational Economy facilities require the current Facility definition version.");
		return false;
	}
	if (FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Synthesize)
	{
		OutFailureReason = TEXT("Facility is not configured as a Resource V2 synthesis facility.");
		return false;
	}

	switch (FacilityDataAsset->ResourceV2Synthesis.SynthesisRole)
	{
	case ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator:
		if (FacilityDataAsset->InputInventory.SlotCount != 2
			|| FacilityDataAsset->OutputInventory.SlotCount < 1
			|| FacilityDataAsset->OperationalLoad != 4)
		{
			OutFailureReason = TEXT("Supply Fabricator requires two input slots, one output slot, and Operational Load 4.");
			return false;
		}
		return true;

	case ESRFacilitySynthesisRoleV2::ServiceCore:
	case ESRFacilitySynthesisRoleV2::FleetBerth:
		if (FacilityDataAsset->InputInventory.SlotCount != 1
			|| FacilityDataAsset->InputInventory.SlotCapacity != 4
			|| FacilityDataAsset->OutputInventory.SlotCount != 0
			|| FacilityDataAsset->OperationalLoad != 0)
		{
			OutFailureReason = TEXT("Capacity infrastructure requires one four-unit input buffer, no output slots, and Operational Load 0.");
			return false;
		}
		return true;

	default:
		OutFailureReason = TEXT("Facility has no Operational Economy synthesis role.");
		return false;
	}
}

FSROperationalEconomyEvaluationV2 FSROperationalEconomyProcessor::Evaluate(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	const ESRFacilitySynthesisRoleV2 Role = IsValid(FacilityDataAsset)
		? FacilityDataAsset->ResourceV2Synthesis.SynthesisRole
		: ESRFacilitySynthesisRoleV2::None;
	FString DefinitionFailure;
	if (!ValidateFacilityDefinition(FacilityDataAsset, DefinitionFailure))
	{
		return MakeFailure(
			IsValid(FacilityDataAsset)
				? ESROperationalEconomyOutcomeV2::UnsupportedDefinition
				: ESROperationalEconomyOutcomeV2::InvalidFacility,
			Role,
			DefinitionFailure);
	}

	const FName OreId = FSRResourceSystemContent::GetUtilityResourceId(ESRResourceContentPresetV2::CommonOre);
	const FName BiomassId = FSRResourceSystemContent::GetUtilityResourceId(ESRResourceContentPresetV2::BiomassFeedstock);
	const FName SupplyId = FSRResourceSystemContent::GetUtilityResourceId(ESRResourceContentPresetV2::IndustrialSupply);
	FSROperationalEconomyEvaluationV2 Evaluation;
	Evaluation.Outcome = ESROperationalEconomyOutcomeV2::Success;
	Evaluation.SynthesisRole = Role;

	if (Role == ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator)
	{
		if (InputResources.Num() != 2)
		{
			return MakeFailure(ESROperationalEconomyOutcomeV2::InvalidInput, Role, TEXT("Supply Fabricator requires exactly one Common Ore and one Biomass Feedstock."));
		}
		bool bHasOre = false;
		bool bHasBiomass = false;
		for (const FSRResourceInstance& Input : InputResources)
		{
			if (!bHasOre && IsCurrentUtility(Input, OreId))
			{
				bHasOre = true;
			}
			else if (!bHasBiomass && IsCurrentUtility(Input, BiomassId))
			{
				bHasBiomass = true;
			}
			else
			{
				return MakeFailure(ESROperationalEconomyOutcomeV2::InvalidInput, Role, TEXT("Supply Fabricator inputs must be one current-schema Common Ore and one Biomass Feedstock."));
			}
		}

		FSRResourceInstance Output;
		if (!bHasOre || !bHasBiomass
			|| !FSRResourceSystemContent::MakeReferenceResourceInstance(
				ESRResourceContentPresetV2::IndustrialSupply,
				NAME_None,
				Output))
		{
			return MakeFailure(ESROperationalEconomyOutcomeV2::InvalidInput, Role, TEXT("Supply Fabricator inputs or output definition are invalid."));
		}
		Output.StackCount = 2;
		Evaluation.OutputResources.Add(MoveTemp(Output));
		return Evaluation;
	}

	if (InputResources.Num() != 1 || !IsCurrentUtility(InputResources[0], SupplyId))
	{
		return MakeFailure(
			ESROperationalEconomyOutcomeV2::InvalidInput,
			Role,
			Role == ESRFacilitySynthesisRoleV2::FleetBerth
				? TEXT("Fleet Berth requires exactly one current-schema Industrial Supply.")
				: TEXT("Service Core requires exactly one current-schema Industrial Supply."));
	}
	return Evaluation;
}

bool FSROperationalEconomyProcessor::AllowsEmptyOutput(const USRFacilityDataAsset* FacilityDataAsset)
{
	return ShouldRouteThroughResourceV2(FacilityDataAsset)
		&& (FacilityDataAsset->ResourceV2Synthesis.SynthesisRole == ESRFacilitySynthesisRoleV2::ServiceCore
			|| FacilityDataAsset->ResourceV2Synthesis.SynthesisRole == ESRFacilitySynthesisRoleV2::FleetBerth);
}

bool FSROperationalEconomyProcessor::IsServiceCoreSupplied(
	const FSRFacilityInstance& FacilityInstance)
{
	return IsSuppliedInfrastructure(FacilityInstance, ESRFacilitySynthesisRoleV2::ServiceCore);
}

bool FSROperationalEconomyProcessor::IsFleetBerthSupplied(
	const FSRFacilityInstance& FacilityInstance)
{
	return IsSuppliedInfrastructure(FacilityInstance, ESRFacilitySynthesisRoleV2::FleetBerth);
}

FString FSROperationalEconomyProcessor::BuildPreviewSummary(
	const FSROperationalEconomyEvaluationV2& Evaluation)
{
	if (!Evaluation.IsSuccess())
	{
		return FString::Printf(
			TEXT("Operational Economy V2 unavailable\n%s"),
			Evaluation.FailureReason.IsEmpty() ? TEXT("Unknown failure") : *Evaluation.FailureReason);
	}
	if (Evaluation.SynthesisRole == ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator)
	{
		return TEXT("Supply Fabricator V2\n1 Common Ore + 1 Biomass Feedstock -> 2 Industrial Supply\nCycle 30s | Operational Load 4");
	}
	if (Evaluation.SynthesisRole == ESRFacilitySynthesisRoleV2::FleetBerth)
	{
		return TEXT("Fleet Berth V2\nConsumes 1 Industrial Supply every 60s\nSupplied bonus: +8 Fleet Capacity | Buffer: 4 (240s)");
	}
	return TEXT("Service Core V2\nConsumes 1 Industrial Supply every 30s\nSupplied bonus: +18 Operational Capacity | Buffer: 4 (120s)");
}
