#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	void SortConveyorLaneKeys(TArray<FSRConveyorLaneKey>& LaneKeys)
	{
		LaneKeys.Sort([](const FSRConveyorLaneKey& Left, const FSRConveyorLaneKey& Right)
		{
			const int32 LeftFace = static_cast<int32>(Left.CellId.Face);
			const int32 RightFace = static_cast<int32>(Right.CellId.Face);
			if (LeftFace != RightFace)
			{
				return LeftFace < RightFace;
			}
			if (Left.CellId.CellY != Right.CellId.CellY)
			{
				return Left.CellId.CellY < Right.CellId.CellY;
			}
			if (Left.CellId.CellX != Right.CellId.CellX)
			{
				return Left.CellId.CellX < Right.CellId.CellX;
			}
			return Left.Layer < Right.Layer;
		});
	}
}

int32 USRConveyorNetworkComponent::GetConveyorItemCount() const
{
	return ConveyorItemsByLane.Num();
}

void USRConveyorNetworkComponent::ProcessConveyorTransport(USRPlanetSurfaceGrid* SurfaceGrid, float DeltaTime)
{
	const float ClampedDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (!IsValid(SurfaceGrid) || ClampedDeltaTime <= 0.0f)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	USRFacilityNetworkComponent* FacilityNetwork = IsValid(OwnerActor)
		? OwnerActor->FindComponentByClass<USRFacilityNetworkComponent>()
		: nullptr;
	if (!IsValid(FacilityNetwork))
	{
		return;
	}

	TArray<FSRConveyorLaneKey> ItemLaneKeys;
	ConveyorItemsByLane.GetKeys(ItemLaneKeys);
	SortConveyorLaneKeys(ItemLaneKeys);

	int32 TransferCount = 0;
	TMap<FSRConveyorLaneKey, FSRConveyorItem> NextItemsByLane;
	const float ProgressDelta = ClampedDeltaTime * FMath::Max(0.01f, ItemSpeedCellsPerSecond);
	for (const FSRConveyorLaneKey& LaneKey : ItemLaneKeys)
	{
		const FSRConveyorItem* ExistingItem = ConveyorItemsByLane.Find(LaneKey);
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
			TArray<ESRConveyorGridDirection> OutputDirections;
			CollectConveyorOutputDirections(*Segment, OutputDirections);
			bool bHasConnectedOutputLane = false;
			for (const ESRConveyorGridDirection OutputDirection : OutputDirections)
			{
				FSRConveyorLaneKey CandidateLaneKey;
				if (TryResolveNextLaneByDirection(SurfaceGrid, *Segment, OutputDirection, CandidateLaneKey)
					&& Segments.Contains(CandidateLaneKey))
				{
					bHasConnectedOutputLane = true;
					break;
				}
			}

			FSRConveyorLaneKey NextLaneKey;
			if (TryResolveNextTransferLane(SurfaceGrid, *Segment, NextItemsByLane, NextLaneKey))
			{
				Item.CurrentLane = NextLaneKey;
				Item.Progress = 0.0f;
				NextItemsByLane.Add(NextLaneKey, Item);
				continue;
			}
			else if (!bHasConnectedOutputLane
				&& TransferCount < FMath::Max(1, MaxItemTransfersPerTick)
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

	TArray<FSRConveyorLaneKey> SegmentLaneKeys;
	Segments.GetKeys(SegmentLaneKeys);
	SortConveyorLaneKeys(SegmentLaneKeys);
	for (const FSRConveyorLaneKey& LaneKey : SegmentLaneKeys)
	{
		if (TransferCount >= FMath::Max(1, MaxItemTransfersPerTick))
		{
			break;
		}

		const FSRConveyorSegment* Segment = Segments.Find(LaneKey);
		if (!Segment || Segment->InputDirection != ESRConveyorGridDirection::None || NextItemsByLane.Contains(LaneKey))
		{
			continue;
		}

		if (TryPullFacilityOutputToConveyor(SurfaceGrid, FacilityNetwork, LaneKey, NextItemsByLane))
		{
			++TransferCount;
		}
	}

	ConveyorItemsByLane = MoveTemp(NextItemsByLane);
}

bool USRConveyorNetworkComponent::TryResolveNextLane(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorSegment& Segment,
	FSRConveyorLaneKey& OutNextLane) const
{
	return TryResolveNextLaneByDirection(SurfaceGrid, Segment, Segment.OutputDirection, OutNextLane);
}

bool USRConveyorNetworkComponent::TryResolveNextLaneByDirection(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorSegment& Segment,
	ESRConveyorGridDirection Direction,
	FSRConveyorLaneKey& OutNextLane) const
{
	OutNextLane = FSRConveyorLaneKey();
	if (!IsValid(SurfaceGrid) || Direction == ESRConveyorGridDirection::None)
	{
		return false;
	}

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	if (!SurfaceGrid->GetCellNeighbors(Segment.Lane.CellId, Neighbors))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellId NextCellId;
	if (!GetNeighborCellIdByDirection(Neighbors, Direction, NextCellId))
	{
		return false;
	}

	OutNextLane = MakeLaneKey(NextCellId, Segment.Lane.Layer);
	return true;
}

bool USRConveyorNetworkComponent::TryResolveNextTransferLane(
	USRPlanetSurfaceGrid* SurfaceGrid,
	FSRConveyorSegment& Segment,
	const TMap<FSRConveyorLaneKey, FSRConveyorItem>& NextItemsByLane,
	FSRConveyorLaneKey& OutNextLane)
{
	OutNextLane = FSRConveyorLaneKey();

	TArray<ESRConveyorGridDirection> OutputDirections;
	CollectConveyorOutputDirections(Segment, OutputDirections);
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
		if (!TryResolveNextLaneByDirection(SurfaceGrid, Segment, OutputDirections[OutputIndex], CandidateLaneKey)
			|| !Segments.Contains(CandidateLaneKey)
			|| ConveyorItemsByLane.Contains(CandidateLaneKey)
			|| NextItemsByLane.Contains(CandidateLaneKey))
		{
			continue;
		}

		OutNextLane = CandidateLaneKey;
		Segment.NextOutputDirectionIndex = (OutputIndex + 1) % OutputCount;
		return true;
	}

	return false;
}

bool USRConveyorNetworkComponent::TryPullFacilityOutputToConveyor(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRFacilityNetworkComponent* FacilityNetwork,
	const FSRConveyorLaneKey& LaneKey,
	TMap<FSRConveyorLaneKey, FSRConveyorItem>& OutNextItems) const
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

bool USRConveyorNetworkComponent::ShouldKeepTransportTickEnabled() const
{
	return (bAutoTransportItems && (!Segments.IsEmpty() || !ConveyorItemsByLane.IsEmpty()))
		|| (bShowTransportItemVisuals && !ConveyorItemsByLane.IsEmpty());
}
