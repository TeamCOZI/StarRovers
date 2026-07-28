#include "Automation/SRFacilityNetworkComponent.h"

#include "Utility/SRLog.h"
#include "SRFacilityMiningTargetQuery.h"
#include "SRFacilityOperationStateController.h"
#include "SRFacilityOutputPreviewQuery.h"
#include "SRFacilityProcessingStepExecutor.h"
#include "SRFacilityProcessingTickRunner.h"
#include "Structure/SRStructureDataAsset.h"

namespace
{
	FString BuildFacilityLogName(const FSRFacilityInstance& FacilityInstance)
	{
		if (IsValid(FacilityInstance.StructureDataAsset.Get()))
		{
			const FSRStructureData StructureData = FacilityInstance.StructureDataAsset->BuildData();
			if (!StructureData.DisplayName.IsEmpty())
			{
				return StructureData.DisplayName.ToString();
			}
			if (!StructureData.StructureId.IsNone())
			{
				return StructureData.StructureId.ToString();
			}
		}

		return GetNameSafe(FacilityInstance.StructureDataAsset.Get());
	}
}

bool USRFacilityNetworkComponent::SetFacilityTemperatureState(FName OccupantId, ESRFacilityTemperatureState TemperatureState)
{
	if (!FSRFacilityOperationStateController::SetTemperatureState(RuntimeState, OccupantId, TemperatureState))
	{
		if (bLogFacilityNetworkEvents)
		{
			SR_LOG(FacilityNetwork, LogTemp,
				Warning,
				TEXT("[FacilityNetwork] SetTemperature failed: OccupantId=%s Owner=%s Reason=MissingFacility"),
				*OccupantId.ToString(),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork, LogTemp,
			Display,
			TEXT("[FacilityNetwork] Temperature set: OccupantId=%s TemperatureState=%d Owner=%s"),
			*OccupantId.ToString(),
			static_cast<int32>(TemperatureState),
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::SetFacilityProcessEnabled(FName OccupantId, bool bEnabled)
{
	const bool bChanged = FSRFacilityOperationStateController::SetProcessEnabled(
		this,
		RuntimeState,
		OccupantId,
		bEnabled,
		bAutoProcessFacilities);
	if (bChanged)
	{
		RefreshOperationalCapacity();
	}
	return bChanged;
}

bool USRFacilityNetworkComponent::SetFacilityDeliverEnabled(FName OccupantId, bool bEnabled)
{
	return FSRFacilityOperationStateController::SetDeliverEnabled(
		this,
		RuntimeState,
		OccupantId,
		bEnabled);
}

int32 USRFacilityNetworkComponent::ProcessFacilities(float DeltaTime)
{
	return FSRFacilityProcessingTickRunner::ProcessFacilities(
		RuntimeState,
		DeltaTime,
		MaxFacilitiesProcessedPerTick,
		[this](FSRFacilityInstance& FacilityInstance)
		{
			return TryStartProcessing(FacilityInstance);
		},
		[this]()
		{
			RefreshOperationalCapacity();
		},
		[](const FSRFacilityInstance& FacilityInstance)
		{
			return FacilityInstance.OperationalSpeedFactor;
		},
		[this](FSRFacilityInstance& FacilityInstance)
		{
			return TryCompleteProcessing(FacilityInstance);
		});
}

bool USRFacilityNetworkComponent::TryStartProcessing(FSRFacilityInstance& FacilityInstance)
{
	FSRFacilityProcessingStartResult StartResult;
	if (!FSRFacilityProcessingStepExecutor::TryStartProcessing(this, FacilityInstance, &StartResult))
	{
		return false;
	}

	if (!bLogFacilityNetworkEvents)
	{
		return true;
	}

	if (StartResult.StepKind == ESRFacilityProcessingStepKind::Mining)
	{
		const FString FacilityLogName = BuildFacilityLogName(FacilityInstance);
		SR_LOG(FacilityNetwork, LogTemp,
			Display,
			TEXT("[FacilityNetwork] Mining started: OccupantId=%s Facility=%s Deposit=%s ResourceId=%s Available=Infinite Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*FacilityLogName,
			*StartResult.MiningResult.ResourceDeposit.OccupantId.ToString(),
			*StartResult.MiningResult.ResourceDeposit.ResourceId.ToString(),
			*GetNameSafe(GetOwner()));
		return true;
	}

	if (StartResult.StepKind == ESRFacilityProcessingStepKind::Standard)
	{
		const FString FacilityLogName = BuildFacilityLogName(FacilityInstance);
		SR_LOG(FacilityNetwork, LogTemp,
			Display,
			TEXT("[FacilityNetwork] Processing started: OccupantId=%s Facility=%s ProcessingInputs=%d RemainingInputs=%d Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*FacilityLogName,
			StartResult.ProcessingInputCount,
			StartResult.RemainingInputCount,
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::TryCompleteProcessing(FSRFacilityInstance& FacilityInstance)
{
	FSRFacilityProcessingCompletionResult CompletionResult;
	if (!FSRFacilityProcessingStepExecutor::TryCompleteProcessing(this, FacilityInstance, &CompletionResult))
	{
		return false;
	}
	RefreshOperationalCapacity();
	if (CompletionResult.StepKind == ESRFacilityProcessingStepKind::Mining
		&& !CompletionResult.MiningResult.MinedResource.ResourceId.IsNone())
	{
		ResourceProducedEvent.Broadcast(
			this,
			FacilityInstance.OccupantId,
			CompletionResult.MiningResult.MinedResource);
	}
	else if (CompletionResult.StepKind == ESRFacilityProcessingStepKind::Standard
		&& CompletionResult.OutputCount > 0
		&& !CompletionResult.PrimaryOutputResource.ResourceId.IsNone())
	{
		ResourceProducedEvent.Broadcast(
			this,
			FacilityInstance.OccupantId,
			CompletionResult.PrimaryOutputResource);
	}

	if (!bLogFacilityNetworkEvents)
	{
		return true;
	}

	if (CompletionResult.StepKind == ESRFacilityProcessingStepKind::Mining)
	{
		const FString FacilityLogName = BuildFacilityLogName(FacilityInstance);
		SR_LOG(FacilityNetwork, LogTemp,
			Display,
			TEXT("[FacilityNetwork] Mining completed: OccupantId=%s Facility=%s Deposit=%s ResourceId=%s Available=Infinite Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*FacilityLogName,
			*CompletionResult.MiningResult.DepositOccupantId.ToString(),
			*CompletionResult.MiningResult.MinedResource.ResourceId.ToString(),
			*GetNameSafe(GetOwner()));
		return true;
	}

	if (CompletionResult.StepKind == ESRFacilityProcessingStepKind::Standard)
	{
		const FString FacilityLogName = BuildFacilityLogName(FacilityInstance);
		const FString ResourceV2Summary = CompletionResult.bUsedResourceV2Process
			? FString::Printf(
				TEXT(" V2Delta=(Facility=%+.3f Family=%+.3f Tag=%+.3f Clamp=%+.3f) StateChanges=(+0x%X -0x%X)"),
				CompletionResult.ResourceV2ProcessResult.FacilityEnergyDelta,
				CompletionResult.ResourceV2ProcessResult.FamilyEnergyDelta,
				CompletionResult.ResourceV2ProcessResult.ProcessTagEnergyDelta,
				CompletionResult.ResourceV2ProcessResult.ClampEnergyDelta,
				CompletionResult.ResourceV2ProcessResult.ActivatedFamilyStateFlags,
				CompletionResult.ResourceV2ProcessResult.ClearedFamilyStateFlags)
			: CompletionResult.bUsedStellarFuelFabricatorV2
				? FString::Printf(
					TEXT(" StellarFuel=(Hand=%s A=%.3f B=%.3f C=%.3f Energy=%.3f Topology=%s Twin=%d Catalyst=%d)"),
					*StaticEnum<ESRStellarFuelHandV2>()->GetNameStringByValue(
						static_cast<int64>(CompletionResult.StellarFuelFabricationResult.Hand)),
					CompletionResult.StellarFuelFabricationResult.FormulaA,
					CompletionResult.StellarFuelFabricationResult.FormulaB,
					CompletionResult.StellarFuelFabricationResult.FormulaC,
					CompletionResult.StellarFuelFabricationResult.FuelEnergy,
					CompletionResult.StellarFuelFabricationResult.AppliedTopologySealId.IsNone()
						? TEXT("None")
						: *CompletionResult.StellarFuelFabricationResult.AppliedTopologySealId.ToString(),
					CompletionResult.StellarFuelFabricationResult.EffectiveTwinSealCount,
					CompletionResult.StellarFuelFabricationResult.EffectivePrismaticCatalystCount)
				: FString();
		SR_LOG(FacilityNetwork, LogTemp,
			Display,
			TEXT("[FacilityNetwork] Processing completed: OccupantId=%s Facility=%s OutputResourceId=%s OutputCount=%d AdditionalOutputs=%d Owner=%s%s"),
			*FacilityInstance.OccupantId.ToString(),
			*FacilityLogName,
			CompletionResult.PrimaryOutputResource.ResourceId.IsNone() ? TEXT("None") : *CompletionResult.PrimaryOutputResource.ResourceId.ToString(),
			CompletionResult.OutputCount,
			CompletionResult.AdditionalOutputCount,
			*GetNameSafe(GetOwner()),
			*ResourceV2Summary);
	}
	return true;
}

bool USRFacilityNetworkComponent::GetFacilityOutputPreview(
	FName OccupantId,
	FSRResourceInstance& OutPrimaryOutput,
	TArray<FSRResourceInstance>& OutAdditionalOutputs,
	int32& OutOutputCount,
	TArray<FString>& OutEnergyFormulaTexts) const
{
	return FSRFacilityOutputPreviewQuery::GetOutputPreview(
		this,
		RuntimeState,
		OccupantId,
		OutPrimaryOutput,
		OutAdditionalOutputs,
		OutOutputCount,
		OutEnergyFormulaTexts);
}

bool USRFacilityNetworkComponent::GetFacilityMiningTarget(
	FName OccupantId,
	FSRResourceDepositInstance& OutResourceDeposit) const
{
	return FSRFacilityMiningTargetQuery::GetMiningTarget(
		this,
		RuntimeState,
		OccupantId,
		OutResourceDeposit);
}
