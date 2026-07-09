#include "SRFacilityProcessingTickRunner.h"

#include "SRFacilityProcessingRuleEvaluator.h"

int32 FSRFacilityProcessingTickRunner::ProcessFacilities(
	FSRFacilityNetworkRuntimeState& RuntimeState,
	float DeltaTime,
	int32 MaxFacilitiesProcessed,
	TFunctionRef<bool(FSRFacilityInstance&)> TryStartProcessing,
	TFunctionRef<bool(FSRFacilityInstance&)> TryCompleteProcessing)
{
	if (RuntimeState.FacilityInstancesByOccupantId.IsEmpty())
	{
		return 0;
	}

	int32 ProcessedCount = 0;
	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		if (ProcessedCount >= MaxFacilitiesProcessed)
		{
			break;
		}

		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		if (!FacilityInstance.bProcessing)
		{
			TryStartProcessing(FacilityInstance);
		}

		if (FacilityInstance.bProcessing && FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(FacilityInstance))
		{
			FacilityInstance.ProcessProgressSeconds += FMath::Max(0.0f, DeltaTime);
			if (FacilityInstance.ProcessProgressSeconds >= FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance))
			{
				TryCompleteProcessing(FacilityInstance);
			}
			++ProcessedCount;
		}
	}

	return ProcessedCount;
}
