#include "Conveyor/SRConveyorTransportProcessor.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Conveyor/SRConveyorConnectionQuery.h"
#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool HasAnyInputDirection(const FSRConveyorSegment& Segment)
	{
		return Segment.InputDirection != ESRConveyorGridDirection::None
			|| Segment.MergeInputDirection != ESRConveyorGridDirection::None
			|| Segment.SecondMergeInputDirection != ESRConveyorGridDirection::None;
	}

	int32 CountUniqueInputDirections(const FSRConveyorSegment& Segment)
	{
		int32 InputDirectionCount = 0;
		if (Segment.InputDirection != ESRConveyorGridDirection::None)
		{
			++InputDirectionCount;
		}
		if (Segment.MergeInputDirection != ESRConveyorGridDirection::None
			&& Segment.MergeInputDirection != Segment.InputDirection)
		{
			++InputDirectionCount;
		}
		if (Segment.SecondMergeInputDirection != ESRConveyorGridDirection::None
			&& Segment.SecondMergeInputDirection != Segment.InputDirection
			&& Segment.SecondMergeInputDirection != Segment.MergeInputDirection)
		{
			++InputDirectionCount;
		}
		return InputDirectionCount;
	}

	bool HasInputDirection(const FSRConveyorSegment& Segment, ESRConveyorGridDirection Direction)
	{
		return Direction != ESRConveyorGridDirection::None
			&& (Segment.InputDirection == Direction
				|| Segment.MergeInputDirection == Direction
				|| Segment.SecondMergeInputDirection == Direction);
	}

	bool TryResolveOutputLane(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorSegment& Segment,
		ESRConveyorGridDirection OutputDirection,
		FSRConveyorLaneKey& OutLaneKey)
	{
		return OutputDirection != ESRConveyorGridDirection::None
			&& StarRovers::Conveyor::FSRConveyorConnectionQuery::TryResolveLaneByDirection(
				SurfaceGrid,
				Segment,
				OutputDirection,
				OutLaneKey);
	}
}

void StarRovers::Conveyor::FSRConveyorTransportProcessor::Process(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRFacilityNetworkComponent* FacilityNetwork,
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	FSRConveyorTransportRuntimeState& TransportState,
	float DeltaTime,
	const FSRConveyorTransportSettings& Settings)
{
	const float ClampedDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (!IsValid(SurfaceGrid) || !IsValid(FacilityNetwork) || ClampedDeltaTime <= 0.0f)
	{
		return;
	}

	TArray<FSRConveyorLaneKey> ItemLaneKeys;
	TransportState.ItemsByLane.GetKeys(ItemLaneKeys);
	FSRConveyorNetworkGeometry::SortLaneKeys(ItemLaneKeys);

	int32 TransferCount = 0;
	const int32 MaxTransferCount = FMath::Max(1, Settings.MaxItemTransfersPerTick);
	TMap<FSRConveyorLaneKey, FSRConveyorItem> NextItemsByLane;
	NextItemsByLane.Reserve(TransportState.ItemsByLane.Num() + MaxTransferCount);
	const float ProgressDelta = ClampedDeltaTime * FMath::Max(0.01f, Settings.ItemSpeedCellsPerSecond);
	for (const FSRConveyorLaneKey& LaneKey : ItemLaneKeys)
	{
		const FSRConveyorItem* ExistingItem = TransportState.ItemsByLane.Find(LaneKey);
		FSRConveyorSegment* Segment = Segments.Find(LaneKey);
		if (!ExistingItem || !Segment)
		{
			continue;
		}

		FSRConveyorItem Item = *ExistingItem;
		Item.CurrentLane = LaneKey;
		Item.Progress = FMath::Min(1.0f, Item.Progress + ProgressDelta);
		if (Item.Progress >= 1.0f)
		{
			FSRConveyorLaneKey NextLaneKey;
			if (TryResolveNextTransferLane(SurfaceGrid, Segments, TransportState, *Segment, NextItemsByLane, NextLaneKey))
			{
				Item.CurrentLane = NextLaneKey;
				Item.Progress = 0.0f;
				NextItemsByLane.Add(NextLaneKey, Item);
				continue;
			}
			else if (!HasConnectedOutputLane(SurfaceGrid, Segments, *Segment)
				&& TransferCount < MaxTransferCount
				&& FacilityNetwork->TryAcceptInputResourceFromConveyorCell(
					SurfaceGrid,
					LaneKey.CellId,
					Item.ResourceInstance,
					Item.SourceFacilityOccupantId))
			{
				++TransferCount;
				continue;
			}
		}

		NextItemsByLane.Add(LaneKey, Item);
	}

	if (TransferCount < MaxTransferCount)
	{
		TArray<FSRConveyorLaneKey> SegmentLaneKeys;
		Segments.GetKeys(SegmentLaneKeys);
		FSRConveyorNetworkGeometry::SortLaneKeys(SegmentLaneKeys);
		for (const FSRConveyorLaneKey& LaneKey : SegmentLaneKeys)
		{
			if (TransferCount >= MaxTransferCount)
			{
				break;
			}

			const FSRConveyorSegment* Segment = Segments.Find(LaneKey);
			if (!Segment || NextItemsByLane.Contains(LaneKey))
			{
				continue;
			}

			if (HasAnyInputDirection(*Segment))
			{
				continue;
			}

			if (TryPullFacilityOutputToConveyor(SurfaceGrid, FacilityNetwork, LaneKey, NextItemsByLane))
			{
				++TransferCount;
			}
		}
	}

	TransportState.ItemsByLane = MoveTemp(NextItemsByLane);
}

bool StarRovers::Conveyor::FSRConveyorTransportProcessor::HasConnectedOutputLane(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRConveyorSegment& Segment)
{
	const ESRConveyorGridDirection OutputDirections[] =
	{
		Segment.OutputDirection,
		Segment.BranchOutputDirection,
		Segment.SecondBranchOutputDirection,
	};

	for (const ESRConveyorGridDirection OutputDirection : OutputDirections)
	{
		FSRConveyorLaneKey CandidateLaneKey;
		if (TryResolveOutputLane(SurfaceGrid, Segment, OutputDirection, CandidateLaneKey)
			&& Segments.Contains(CandidateLaneKey))
		{
			return true;
		}
	}

	return false;
}

bool StarRovers::Conveyor::FSRConveyorTransportProcessor::TryResolveNextTransferLane(
	USRPlanetSurfaceGrid* SurfaceGrid,
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRConveyorTransportRuntimeState& TransportState,
	FSRConveyorSegment& Segment,
	const TMap<FSRConveyorLaneKey, FSRConveyorItem>& NextItemsByLane,
	FSRConveyorLaneKey& OutNextLane)
{
	OutNextLane = FSRConveyorLaneKey();

	TArray<ESRConveyorGridDirection> OutputDirections;
	FSRConveyorNetworkGeometry::CollectOutputDirections(Segment, OutputDirections);
	if (OutputDirections.IsEmpty())
	{
		Segment.NextOutputDirectionIndex = 0;
		return false;
	}

	const int32 OutputCount = OutputDirections.Num();
	const int32 StartIndex = FMath::Clamp(Segment.NextOutputDirectionIndex, 0, OutputCount - 1);
	for (int32 AttemptIndex = 0; AttemptIndex < OutputCount; ++AttemptIndex)
	{
		const int32 OutputIndex = (StartIndex + AttemptIndex) % OutputCount;
		FSRConveyorLaneKey CandidateLaneKey;
		if (!FSRConveyorConnectionQuery::TryResolveLaneByDirection(SurfaceGrid, Segment, OutputDirections[OutputIndex], CandidateLaneKey)
			|| TransportState.ItemsByLane.Contains(CandidateLaneKey)
			|| NextItemsByLane.Contains(CandidateLaneKey))
		{
			continue;
		}

		FSRConveyorSegment* CandidateSegment = Segments.Find(CandidateLaneKey);
		if (!CandidateSegment)
		{
			continue;
		}

		ESRConveyorGridDirection CandidateInputDirection = ESRConveyorGridDirection::None;
		if (!FSRConveyorNetworkGeometry::FindDirectionBetweenCells(SurfaceGrid, CandidateLaneKey.CellId, Segment.Lane.CellId, CandidateInputDirection)
			|| !CanTransferIntoMergeConveyorSegment(SurfaceGrid, Segments, TransportState, *CandidateSegment, CandidateInputDirection, NextItemsByLane))
		{
			continue;
		}

		OutNextLane = CandidateLaneKey;
		Segment.NextOutputDirectionIndex = (OutputIndex + 1) % OutputCount;
		return true;
	}

	return false;
}

bool StarRovers::Conveyor::FSRConveyorTransportProcessor::CanTransferIntoMergeConveyorSegment(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRConveyorTransportRuntimeState& TransportState,
	FSRConveyorSegment& MergeSegment,
	ESRConveyorGridDirection IncomingInputDirection,
	const TMap<FSRConveyorLaneKey, FSRConveyorItem>& NextItemsByLane)
{
	const int32 InputCount = CountUniqueInputDirections(MergeSegment);
	if (InputCount == 0)
	{
		return true;
	}
	if (!HasInputDirection(MergeSegment, IncomingInputDirection))
	{
		return false;
	}
	if (InputCount <= 1)
	{
		return true;
	}

	TArray<ESRConveyorGridDirection> InputDirections;
	FSRConveyorNetworkGeometry::CollectInputDirections(MergeSegment, InputDirections);
	const int32 StartIndex = FMath::Clamp(MergeSegment.NextInputDirectionIndex, 0, InputCount - 1);
	for (int32 AttemptIndex = 0; AttemptIndex < InputCount; ++AttemptIndex)
	{
		const int32 InputIndex = (StartIndex + AttemptIndex) % InputCount;
		const ESRConveyorGridDirection InputDirection = InputDirections[InputIndex];

		FSRConveyorLaneKey SourceLaneKey;
		if (!FSRConveyorConnectionQuery::TryResolveLaneByDirection(SurfaceGrid, MergeSegment, InputDirection, SourceLaneKey)
			|| !Segments.Contains(SourceLaneKey))
		{
			continue;
		}

		bool bInputReady = InputDirection == IncomingInputDirection;
		if (!bInputReady)
		{
			if (const FSRConveyorItem* SourceItem = TransportState.ItemsByLane.Find(SourceLaneKey))
			{
				bInputReady = SourceItem->Progress >= 1.0f;
			}
		}
		if (!bInputReady)
		{
			if (const FSRConveyorItem* SourceItem = NextItemsByLane.Find(SourceLaneKey))
			{
				bInputReady = SourceItem->Progress >= 1.0f;
			}
		}
		if (!bInputReady)
		{
			continue;
		}

		if (InputDirection != IncomingInputDirection)
		{
			return false;
		}

		MergeSegment.NextInputDirectionIndex = (InputIndex + 1) % InputCount;
		return true;
	}

	return false;
}

bool StarRovers::Conveyor::FSRConveyorTransportProcessor::TryPullFacilityOutputToConveyor(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRFacilityNetworkComponent* FacilityNetwork,
	const FSRConveyorLaneKey& LaneKey,
	TMap<FSRConveyorLaneKey, FSRConveyorItem>& OutNextItems)
{
	if (!IsValid(FacilityNetwork) || OutNextItems.Contains(LaneKey))
	{
		return false;
	}

	FSRResourceInstance ResourceInstance;
	FName SourceFacilityOccupantId = NAME_None;
	if (!FacilityNetwork->TryPullOutputResourceToConveyorCell(SurfaceGrid, LaneKey.CellId, ResourceInstance, SourceFacilityOccupantId))
	{
		return false;
	}

	FSRConveyorItem Item;
	Item.ResourceInstance = ResourceInstance;
	Item.CurrentLane = LaneKey;
	Item.Progress = 0.0f;
	Item.SourceFacilityOccupantId = SourceFacilityOccupantId;
	OutNextItems.Add(LaneKey, Item);
	return true;
}
