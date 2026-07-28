#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Automation/SRRefinementResistanceV2.h"
#include "Automation/SRResourceProcessingKernel.h"

enum class ESRFacilityResourceV2Outcome : uint8
{
	Success,
	InvalidFacility,
	UnsupportedFacilityDefinition,
	UnsupportedOperation,
	MissingProcessArchetype,
	InvalidEnergy,
	FamilyMismatch,
	InvalidResource,
	InvalidProcessTag,
	InvalidFuelImprint,
	ProcessTagSlotOccupied,
	FuelImprintSlotOccupied,
	KernelRejected,
};

struct STARROVERS_API FSRFacilityResourceV2Evaluation
{
	ESRFacilityResourceV2Outcome Outcome = ESRFacilityResourceV2Outcome::InvalidFacility;
	FString FailureReason;
	ESRFacilityProcessRoleV2 ProcessRole = ESRFacilityProcessRoleV2::FamilyProcess;
	FSRResourceProcessSpec ProcessSpec;
	FSRResourceProcessResult ResourceProcessResult;
	FSRRefinementResistanceResultV2 RefinementResistance;
	int32 InputGeneralProcessesSinceReset = 0;

	bool IsSuccess() const
	{
		return Outcome == ESRFacilityResourceV2Outcome::Success
			&& ResourceProcessResult.IsSuccess();
	}
};

class STARROVERS_API FSRFacilityResourceV2Processor
{
public:
	static bool IsResourceV2RulesetActive();

	// While Resource V2 is active every normal Process facility must use the V2 contract.
	// Mine and Synthesize retain their transitional paths until their dedicated migration phases.
	static bool ShouldRouteStandardProcessThroughResourceV2(const USRFacilityDataAsset* FacilityDataAsset);

	static bool ValidateProcessDefinition(
		const USRFacilityDataAsset* FacilityDataAsset,
		FString& OutFailureReason);

	static FName ResolveProcessTagRecipeId(const FSRFacilityInstance& FacilityInstance);
	static FName ResolveFuelImprintRecipeId(const FSRFacilityInstance& FacilityInstance);

	static ESRResourceProcessTemperatureState ConvertTemperature(
		ESRFacilityTemperatureState TemperatureState);

	static FSRFacilityResourceV2Evaluation Evaluate(
		const FSRFacilityInstance& FacilityInstance,
		const FSRResourceInstance& InputResource,
		FName ProcessingBodyId = NAME_None,
		const FSRResourceProcessingRules& Rules = FSRResourceProcessingRules());

	static FString BuildPreviewSummary(const FSRFacilityResourceV2Evaluation& Evaluation);
};
