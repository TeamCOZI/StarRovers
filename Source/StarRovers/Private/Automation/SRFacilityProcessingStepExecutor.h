#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Automation/SRResourceProcessingKernel.h"
#include "Automation/SRStellarFuelTypes.h"
#include "SRFacilityMiningProcessor.h"

class UActorComponent;

enum class ESRFacilityProcessingStepKind : uint8
{
	None,
	Standard,
	Mining
};

struct FSRFacilityProcessingStartResult
{
	ESRFacilityProcessingStepKind StepKind = ESRFacilityProcessingStepKind::None;
	FSRFacilityMiningStartResult MiningResult;
	int32 ProcessingInputCount = 0;
	int32 RemainingInputCount = 0;
};

struct FSRFacilityProcessingCompletionResult
{
	ESRFacilityProcessingStepKind StepKind = ESRFacilityProcessingStepKind::None;
	FSRFacilityMiningCompletionResult MiningResult;
	FSRResourceInstance PrimaryOutputResource;
	FSRResourceProcessResult ResourceV2ProcessResult;
	FSRStellarFuelFabricationResultV2 StellarFuelFabricationResult;
	int32 OutputCount = 0;
	int32 AdditionalOutputCount = 0;
	bool bUsedResourceV2 = false;
	bool bUsedResourceV2Process = false;
	bool bUsedStellarFuelFabricatorV2 = false;
	bool bUsedOperationalEconomyV2 = false;
};

class FSRFacilityProcessingStepExecutor
{
public:
	static bool TryStartProcessing(
		const UActorComponent* OwnerComponent,
		FSRFacilityInstance& FacilityInstance,
		FSRFacilityProcessingStartResult* OutStartResult = nullptr);

	static bool TryCompleteProcessing(
		const UActorComponent* OwnerComponent,
		FSRFacilityInstance& FacilityInstance,
		FSRFacilityProcessingCompletionResult* OutCompletionResult = nullptr);
};
