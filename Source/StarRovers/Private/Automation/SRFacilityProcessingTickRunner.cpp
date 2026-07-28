#include "SRFacilityProcessingTickRunner.h"

#include "SRFacilityProcessingRuleEvaluator.h"

namespace
{
	void RebuildSchedulerOrder(FSRFacilityNetworkRuntimeState& RuntimeState)
	{
		const FName RequestedNextOccupantId = RuntimeState.NextFacilitySchedulerOccupantId;
		RuntimeState.FacilityInstancesByOccupantId.GenerateKeyArray(RuntimeState.FacilitySchedulerOrder);
		RuntimeState.FacilitySchedulerOrder.Sort([](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});

		RuntimeState.NextFacilitySchedulerOccupantId = NAME_None;
		if (!RuntimeState.FacilitySchedulerOrder.IsEmpty())
		{
			if (!RequestedNextOccupantId.IsNone()
				&& RuntimeState.FacilitySchedulerOrder.Contains(RequestedNextOccupantId))
			{
				RuntimeState.NextFacilitySchedulerOccupantId = RequestedNextOccupantId;
			}
			else
			{
				for (const FName CandidateOccupantId : RuntimeState.FacilitySchedulerOrder)
				{
					if (RequestedNextOccupantId.IsNone()
						|| !CandidateOccupantId.LexicalLess(RequestedNextOccupantId))
					{
						RuntimeState.NextFacilitySchedulerOccupantId = CandidateOccupantId;
						break;
					}
				}
				if (RuntimeState.NextFacilitySchedulerOccupantId.IsNone())
				{
					RuntimeState.NextFacilitySchedulerOccupantId = RuntimeState.FacilitySchedulerOrder[0];
				}
			}
		}
		RuntimeState.bFacilitySchedulerOrderDirty = false;
	}

	void EnsureSchedulerOrder(FSRFacilityNetworkRuntimeState& RuntimeState)
	{
		bool bNeedsRebuild = RuntimeState.bFacilitySchedulerOrderDirty
			|| RuntimeState.FacilitySchedulerOrder.Num()
				!= RuntimeState.FacilityInstancesByOccupantId.Num();
		if (!bNeedsRebuild)
		{
			for (const FName OccupantId : RuntimeState.FacilitySchedulerOrder)
			{
				if (!RuntimeState.FacilityInstancesByOccupantId.Contains(OccupantId))
				{
					bNeedsRebuild = true;
					break;
				}
			}
		}

		if (bNeedsRebuild)
		{
			RebuildSchedulerOrder(RuntimeState);
		}
	}

	void GatherTransitionCandidates(
		FSRFacilityNetworkRuntimeState& RuntimeState,
		int32 MaxFacilityTransitions,
		TArray<FSRFacilityInstance*>& OutCandidates)
	{
		OutCandidates.Reset();
		EnsureSchedulerOrder(RuntimeState);
		const int32 FacilityCount = RuntimeState.FacilitySchedulerOrder.Num();
		const int32 CandidateCount = FMath::Min(FMath::Max(0, MaxFacilityTransitions), FacilityCount);
		if (CandidateCount <= 0)
		{
			return;
		}

		int32 StartIndex = RuntimeState.FacilitySchedulerOrder.IndexOfByKey(
			RuntimeState.NextFacilitySchedulerOccupantId);
		if (StartIndex == INDEX_NONE)
		{
			StartIndex = 0;
		}
		OutCandidates.Reserve(CandidateCount);
		for (int32 CandidateOffset = 0; CandidateOffset < CandidateCount; ++CandidateOffset)
		{
			const int32 CandidateIndex = (StartIndex + CandidateOffset) % FacilityCount;
			const FName OccupantId = RuntimeState.FacilitySchedulerOrder[CandidateIndex];
			if (FSRFacilityInstance* FacilityInstance =
				RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId))
			{
				OutCandidates.Add(FacilityInstance);
			}
		}

		const int32 NextIndex = (StartIndex + CandidateCount) % FacilityCount;
		RuntimeState.NextFacilitySchedulerOccupantId =
			RuntimeState.FacilitySchedulerOrder[NextIndex];
	}
}

int32 FSRFacilityProcessingTickRunner::ProcessFacilities(
	FSRFacilityNetworkRuntimeState& RuntimeState,
	float DeltaTime,
	int32 MaxFacilityTransitions,
	TFunctionRef<bool(FSRFacilityInstance&)> TryStartProcessing,
	TFunctionRef<void()> RefreshOperationalCapacity,
	TFunctionRef<float(const FSRFacilityInstance&)> ResolveOperationalSpeedFactor,
	TFunctionRef<bool(FSRFacilityInstance&)> TryCompleteProcessing)
{
	const int32 FacilityTransitionLimit = FMath::Max(0, MaxFacilityTransitions);
	if (RuntimeState.FacilityInstancesByOccupantId.IsEmpty() || FacilityTransitionLimit <= 0)
	{
		return 0;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	TArray<FSRFacilityInstance*> TransitionCandidates;
	GatherTransitionCandidates(RuntimeState, FacilityTransitionLimit, TransitionCandidates);

	// Only expensive state transitions are time-sliced. Candidate order is
	// stable and rotates, so no facility can starve behind a fixed TMap prefix.
	for (FSRFacilityInstance* FacilityInstance : TransitionCandidates)
	{
		if (FacilityInstance && !FacilityInstance->bProcessing)
		{
			TryStartProcessing(*FacilityInstance);
		}
	}
	RefreshOperationalCapacity();

	// Every in-flight clock advances on every tick. MaxFacilityTransitions is a
	// CPU budget, not a production-speed penalty for large planets.
	int32 AdvancedFacilityCount = 0;
	for (const FName OccupantId : RuntimeState.FacilitySchedulerOrder)
	{
		FSRFacilityInstance* FacilityInstance =
			RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
		if (!FacilityInstance)
		{
			continue;
		}
		if (FacilityInstance->bProcessing
			&& FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(*FacilityInstance))
		{
			const float ProcessSeconds = FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(*FacilityInstance);
			const float SpeedFactor = FMath::Clamp(
				ResolveOperationalSpeedFactor(*FacilityInstance),
				0.0f,
				1.0f);
			FacilityInstance->ProcessProgressSeconds += SafeDeltaTime * SpeedFactor;
			++AdvancedFacilityCount;
		}
	}

	// Completion mutates inventories and may trigger other systems, so it uses
	// the same bounded, fair transition slice as start attempts.
	for (FSRFacilityInstance* FacilityInstance : TransitionCandidates)
	{
		if (!FacilityInstance
			|| !FacilityInstance->bProcessing
			|| !FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(*FacilityInstance))
		{
			continue;
		}
		const float ProcessSeconds =
			FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(*FacilityInstance);
		if (FacilityInstance->ProcessProgressSeconds >= ProcessSeconds)
		{
			TryCompleteProcessing(*FacilityInstance);
		}
	}

	return AdvancedFacilityCount;
}
