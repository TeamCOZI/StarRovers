#include "SRFacilityProcessingRuleEvaluator.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "Automation/SRRefinementResistanceV2.h"
#include "Automation/SRResourceDataAsset.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRStellarFuelFabricator.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Simulation/SRSimulationSettings.h"
#include "SRFacilityMiningTargetResolver.h"
#include "SRFacilityEffectConditionEvaluator.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessContextResolver.h"
#include "SRFacilityProcessingInventoryRouter.h"

namespace
{
	bool DoesProcessingResourceRequireColdTemperature(const FSRResourceInstance& ResourceInstance)
	{
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == ESRResourceProcessTag::Supercooled && TagStack.StackCount > 0)
			{
				return true;
			}
		}

		return false;
	}

	bool CanResourcesAdvanceAtTemperature(
		const TArray<FSRResourceInstance>& ResourceInstances,
		ESRFacilityTemperatureState TemperatureState)
	{
		if (TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			return true;
		}

		for (const FSRResourceInstance& ResourceInstance : ResourceInstances)
		{
			if (DoesProcessingResourceRequireColdTemperature(ResourceInstance))
			{
				return false;
			}
		}

		return true;
	}

	bool CanAdvanceProcessingWithResources(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& ResourceInstances)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			return false;
		}

		if (FacilityDataAsset->FacilityKind == ESRFacilityKind::Hub)
		{
			return false;
		}

		if (!FacilityInstance.bProcessEnabled)
		{
			return false;
		}

		const bool bUsesResourceV2 =
			FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset)
			|| FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset)
			|| FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset);
		const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
			StarRovers::FacilityProcessing::ResolveProcessContext(FacilityInstance, ResourceInstances);
		const ESRFacilityTemperatureState EffectiveTemperatureState = bUsesResourceV2
			? FacilityInstance.TemperatureState
			: ProcessContext.EffectiveTemperatureState;
		if (EffectiveTemperatureState == ESRFacilityTemperatureState::Frozen
			|| EffectiveTemperatureState == ESRFacilityTemperatureState::Overheated)
		{
			return false;
		}

		return bUsesResourceV2
			|| CanResourcesAdvanceAtTemperature(ResourceInstances, EffectiveTemperatureState);
	}
}

bool FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(const FSRFacilityInstance& FacilityInstance)
{
	return CanAdvanceProcessingWithResources(FacilityInstance, FacilityInstance.ProcessingInventory);
}

bool FSRFacilityProcessingRuleEvaluator::CanRun(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return false;
	}
	if (IsValid(OwnerComponent))
	{
		if (const USRAugmentSubsystem* AugmentSubsystem = OwnerComponent->GetWorld()
			? OwnerComponent->GetWorld()->GetSubsystem<USRAugmentSubsystem>()
			: nullptr)
		{
			FString RecipeFailure;
			if (!AugmentSubsystem->IsFacilityRecipeUnlockedV2(FacilityInstance, RecipeFailure))
			{
				return false;
			}
		}
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return CanMiningRun(OwnerComponent, FacilityInstance);
	}

	TArray<FSRResourceInstance> InputResources;
	if (!FSRFacilityProcessingInventoryRouter::GatherPendingInputResources(FacilityInstance, InputResources))
	{
		return false;
	}

	if (!CanAdvanceProcessingWithResources(FacilityInstance, InputResources))
	{
		return false;
	}

	if (!FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(FacilityDataAsset, InputResources, FacilityInstance.TemperatureState))
	{
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	const FName ProcessingBodyId = StarRovers::Resources::ResolveCelestialBodyResourceId(
		IsValid(OwnerComponent) ? OwnerComponent->GetOwner() : nullptr);
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		FacilityInstance,
		InputResources,
		OutputResources,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		ProcessingBodyId);
	if (OutputResources.IsEmpty())
	{
		return FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(FacilityInstance);
	}
	return FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(FacilityInstance, OutputResources);
}

bool FSRFacilityProcessingRuleEvaluator::CanMiningRun(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset)
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine)
	{
		return false;
	}

	FSRResourceDepositInstance ResourceDeposit;
	if (!FSRFacilityMiningTargetResolver::FindTargetDeposit(OwnerComponent, FacilityInstance, ResourceDeposit))
	{
		return false;
	}

	if (!IsValid(ResourceDeposit.ResourceDataAsset.Get()))
	{
		return false;
	}

	TArray<FSRResourceInstance> MiningConditionResources;
	MiningConditionResources.Add(ResourceDeposit.ResourceDataAsset->BuildDefaultInstance());
	if (!CanAdvanceProcessingWithResources(FacilityInstance, MiningConditionResources))
	{
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	const FSRResourceInstance MinedResource = MiningConditionResources[0];
	FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
		FacilityInstance,
		TArray<FSRResourceInstance>(),
		MinedResource,
		OutputResources);
	if (OutputResources.IsEmpty())
	{
		return FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(FacilityInstance);
	}
	return FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(FacilityInstance, OutputResources);
}

namespace
{
	float ResolveUnsnapshottedProcessSeconds(const FSRFacilityInstance& FacilityInstance)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset))
		{
			return FSRFacilityProcessingRuleEvaluator::ResolveRefinementResistance(FacilityInstance).EffectiveProcessSeconds;
		}
		if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset)
			|| FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset))
		{
			return IsValid(FacilityDataAsset)
				? FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds)
				: 0.01f;
		}

		const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
			StarRovers::FacilityProcessing::ResolveProcessContext(FacilityInstance, FacilityInstance.ProcessingInventory);
		return StarRovers::FacilityProcessing::ResolveFacilityProcessSeconds(
			FacilityDataAsset,
			ProcessContext.EffectiveTemperatureState,
			FacilityInstance.ProcessingInventory);
	}
}

float FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance)
{
	if (FacilityInstance.bProcessing
		&& FacilityInstance.bHasResolvedProcessSeconds
		&& FMath::IsFinite(FacilityInstance.ResolvedProcessSeconds)
		&& FacilityInstance.ResolvedProcessSeconds > 0.0f)
	{
		return FMath::Max(0.01f, FacilityInstance.ResolvedProcessSeconds);
	}
	return ResolveUnsnapshottedProcessSeconds(FacilityInstance);
}

float FSRFacilityProcessingRuleEvaluator::CaptureProcessSecondsSnapshot(FSRFacilityInstance& FacilityInstance)
{
	ClearProcessSecondsSnapshot(FacilityInstance);
	const float ResolvedSeconds = FMath::Max(
		0.01f,
		ResolveUnsnapshottedProcessSeconds(FacilityInstance));
	FacilityInstance.ResolvedProcessSeconds = ResolvedSeconds;
	FacilityInstance.bHasResolvedProcessSeconds = true;
	return ResolvedSeconds;
}

void FSRFacilityProcessingRuleEvaluator::ClearProcessSecondsSnapshot(FSRFacilityInstance& FacilityInstance)
{
	FacilityInstance.ResolvedProcessSeconds = 0.0f;
	FacilityInstance.bHasResolvedProcessSeconds = false;
}

FSRRefinementResistanceResultV2 FSRFacilityProcessingRuleEvaluator::ResolveRefinementResistance(
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	const float BaseProcessSeconds = IsValid(FacilityDataAsset)
		? FacilityDataAsset->BaseProcessSeconds
		: 0.01f;
	FSRRefinementResistanceResultV2 Result =
		FSRRefinementResistanceV2::MakeInactive(BaseProcessSeconds);
	if (!FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset)
		|| FacilityDataAsset->ResourceV2Process.ProcessRole != ESRFacilityProcessRoleV2::FamilyProcess
		|| FMath::IsNearlyZero(FacilityDataAsset->ResourceV2Process.FacilityEnergyDelta))
	{
		return Result;
	}

	const FSRResourceInstance* InputResource = FacilityInstance.ProcessingInventory.IsEmpty()
		? nullptr
		: &FacilityInstance.ProcessingInventory[0];
	TArray<FSRResourceInstance> PendingInputs;
	if (!InputResource
		&& FSRFacilityProcessingInventoryRouter::GatherPendingInputResources(
			FacilityInstance,
			PendingInputs)
		&& !PendingInputs.IsEmpty())
	{
		InputResource = &PendingInputs[0];
	}
	if (!InputResource)
	{
		return Result;
	}

	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	FSRRefinementResistanceResultV2 EvaluatedResult = FSRRefinementResistanceV2::Evaluate(
		*InputResource,
		BaseProcessSeconds,
		IsValid(Settings) ? Settings->RefinementResistanceEnergyScaleV2 : 40.0);
	if (EvaluatedResult.bApplied
		&& FacilityInstance.bProcessing
		&& FacilityInstance.bHasResolvedProcessSeconds
		&& FMath::IsFinite(FacilityInstance.ResolvedProcessSeconds)
		&& FacilityInstance.ResolvedProcessSeconds > 0.0f)
	{
		EvaluatedResult.EffectiveProcessSeconds =
			FMath::Max(0.01f, FacilityInstance.ResolvedProcessSeconds);
		EvaluatedResult.CycleMultiplier =
			EvaluatedResult.EffectiveProcessSeconds / EvaluatedResult.BaseProcessSeconds;
	}
	return EvaluatedResult;
}
