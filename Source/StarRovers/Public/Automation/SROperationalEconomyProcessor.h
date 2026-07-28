#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

enum class ESROperationalEconomyOutcomeV2 : uint8
{
	Success,
	InvalidFacility,
	UnsupportedDefinition,
	InvalidInput,
};

struct STARROVERS_API FSROperationalEconomyEvaluationV2
{
	ESROperationalEconomyOutcomeV2 Outcome = ESROperationalEconomyOutcomeV2::InvalidFacility;
	ESRFacilitySynthesisRoleV2 SynthesisRole = ESRFacilitySynthesisRoleV2::None;
	FString FailureReason;
	TArray<FSRResourceInstance> OutputResources;

	bool IsSuccess() const
	{
		return Outcome == ESROperationalEconomyOutcomeV2::Success;
	}
};

// Resource V2 infrastructure recipes. Utility resources intentionally bypass
// Family/Card processing and never participate in Stellar Fuel hands.
class STARROVERS_API FSROperationalEconomyProcessor final
{
public:
	static bool ShouldRouteThroughResourceV2(const USRFacilityDataAsset* FacilityDataAsset);
	static bool ValidateFacilityDefinition(
		const USRFacilityDataAsset* FacilityDataAsset,
		FString& OutFailureReason);
	static FSROperationalEconomyEvaluationV2 Evaluate(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources);
	static bool AllowsEmptyOutput(const USRFacilityDataAsset* FacilityDataAsset);
	static bool IsServiceCoreSupplied(const FSRFacilityInstance& FacilityInstance);
	static bool IsFleetBerthSupplied(const FSRFacilityInstance& FacilityInstance);
	static FString BuildPreviewSummary(const FSROperationalEconomyEvaluationV2& Evaluation);
};
