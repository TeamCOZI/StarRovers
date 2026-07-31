#include "SRFacilityProcessingRuleEvaluator.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityMiningTargetResolver.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessingInventoryRouter.h"
#include "SRFacilityRunModifierResolver.h"

namespace
{
	bool CanAdvanceProcessingWithResources(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& ResourceInstances)
	{
		(void)ResourceInstances;
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		return IsValid(FacilityDataAsset)
			&& FacilityDataAsset->FacilityKind == ESRFacilityKind::Standard
			&& FacilityInstance.bProcessEnabled
			&& FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Frozen
			&& FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Overheated;
	}
}

bool FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(
	const FSRFacilityInstance& FacilityInstance)
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

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return CanMiningRun(OwnerComponent, FacilityInstance);
	}

	TArray<FSRResourceInstance> InputResources;
	if (!FSRFacilityProcessingInventoryRouter::GatherPendingInputResources(
		FacilityInstance,
		InputResources)
		|| !CanAdvanceProcessingWithResources(FacilityInstance, InputResources)
		|| !FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
			FacilityDataAsset,
			InputResources,
			FacilityInstance.TemperatureState))
	{
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		FacilityInstance,
		InputResources,
		OutputResources);
	return !OutputResources.IsEmpty()
		&& FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(
			FacilityInstance,
			OutputResources);
}

bool FSRFacilityProcessingRuleEvaluator::CanMiningRun(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset)
		|| FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine)
	{
		return false;
	}

	FSRResourceDepositInstance ResourceDeposit;
	if (!FSRFacilityMiningTargetResolver::FindTargetDeposit(
		OwnerComponent,
		FacilityInstance,
		ResourceDeposit)
		|| !ResourceDeposit.IsPatternSourceValid())
	{
		return false;
	}

	TArray<FSRResourceInstance> MiningConditionResources;
	MiningConditionResources.Add(ResourceDeposit.BuildResourceInstance());
	if (!CanAdvanceProcessingWithResources(FacilityInstance, MiningConditionResources))
	{
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
		FacilityInstance,
		TArray<FSRResourceInstance>(),
		MiningConditionResources[0],
		OutputResources);
	return !OutputResources.IsEmpty()
		&& FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(
			FacilityInstance,
			OutputResources);
}

float FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return 0.01f;
	}

	const FSRResolvedRunModifiers Modifiers = FSRFacilityRunModifierResolver::Resolve(
		FacilityInstance,
		FacilityInstance.ProcessingInventory);
	return FMath::Max(
		0.01f,
		static_cast<float>(
			static_cast<double>(FacilityDataAsset->BaseProcessSeconds)
			* Modifiers.FacilityProcessTimeMultiplier));
}
