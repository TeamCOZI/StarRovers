#include "SRFacilityProcessingTickRunner.h"

#include "SRFacilityProcessingRuleEvaluator.h"

int32 FSRFacilityProcessingTickRunner::ProcessFacilities(
	FSRFacilityNetworkRuntimeState& RuntimeState,
	float DeltaTime,
	int32 MaxFacilitiesProcessed,
	TFunctionRef<bool(FSRFacilityInstance&)> TryStartProcessing,
	TFunctionRef<bool(FSRFacilityInstance&)> TryCompleteProcessing)
{
	const int32 FacilityProcessLimit = FMath::Max(0, MaxFacilitiesProcessed);
	if (RuntimeState.FacilityInstancesByOccupantId.IsEmpty() || FacilityProcessLimit <= 0)
	{
		return 0;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	int32 ProcessedCount = 0;
	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		if (ProcessedCount >= FacilityProcessLimit)
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
			FacilityInstance.ProcessProgressSeconds += SafeDeltaTime;
			if (FacilityInstance.ProcessProgressSeconds >= FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance))
			{
				TryCompleteProcessing(FacilityInstance);
			}
			++ProcessedCount;
		}
	}

	return ProcessedCount;
}
