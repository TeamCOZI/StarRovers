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
	return FSRFacilityOperationStateController::SetProcessEnabled(
		this,
		RuntimeState,
		OccupantId,
		bEnabled,
		bAutoProcessFacilities);
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
			TEXT("[FacilityNetwork] Mining started: OccupantId=%s Facility=%s Deposit=%s ResourceId=%s Remaining=%d/%d Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*FacilityLogName,
			*StartResult.MiningResult.ResourceDeposit.OccupantId.ToString(),
			*StartResult.MiningResult.ResourceDeposit.ResourceId.ToString(),
			StartResult.MiningResult.ResourceDeposit.RemainingAmount,
			StartResult.MiningResult.ResourceDeposit.TotalAmount,
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

	if (!bLogFacilityNetworkEvents)
	{
		return true;
	}

	if (CompletionResult.StepKind == ESRFacilityProcessingStepKind::Mining)
	{
		const FString FacilityLogName = BuildFacilityLogName(FacilityInstance);
		SR_LOG(FacilityNetwork, LogTemp,
			Display,
			TEXT("[FacilityNetwork] Mining completed: OccupantId=%s Facility=%s Deposit=%s ResourceId=%s Remaining=%d/%d Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*FacilityLogName,
			*CompletionResult.MiningResult.DepositOccupantId.ToString(),
			*CompletionResult.MiningResult.MinedResource.ResourceId.ToString(),
			CompletionResult.MiningResult.RemainingAmount,
			CompletionResult.MiningResult.TotalAmount,
			*GetNameSafe(GetOwner()));
		return true;
	}

	if (CompletionResult.StepKind == ESRFacilityProcessingStepKind::Standard)
	{
		const FString FacilityLogName = BuildFacilityLogName(FacilityInstance);
		SR_LOG(FacilityNetwork, LogTemp,
			Display,
			TEXT("[FacilityNetwork] Processing completed: OccupantId=%s Facility=%s OutputResourceId=%s OutputCount=%d AdditionalOutputs=%d Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*FacilityLogName,
			CompletionResult.PrimaryOutputResource.ResourceId.IsNone() ? TEXT("None") : *CompletionResult.PrimaryOutputResource.ResourceId.ToString(),
			CompletionResult.OutputCount,
			CompletionResult.AdditionalOutputCount,
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::GetFacilityOutputPreview(
	FName OccupantId,
	FSRResourceInstance& OutPrimaryOutput,
	TArray<FSRResourceInstance>& OutAdditionalOutputs,
	int32& OutOutputCount) const
{
	return FSRFacilityOutputPreviewQuery::GetOutputPreview(
		this,
		RuntimeState,
		OccupantId,
		OutPrimaryOutput,
		OutAdditionalOutputs,
		OutOutputCount);
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
