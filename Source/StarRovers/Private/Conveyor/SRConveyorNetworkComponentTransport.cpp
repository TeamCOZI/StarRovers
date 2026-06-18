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
	if (!IsValid(SurfaceGrid))
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
	const float ProgressDelta = FMath::Max(0.0f, DeltaTime) * FMath::Max(0.01f, ItemSpeedCellsPerSecond);
	for (const FSRConveyorLaneKey& LaneKey : ItemLaneKeys)
	{
		const FSRConveyorItem* ExistingItem = ConveyorItemsByLane.Find(LaneKey);
		const FSRConveyorSegment* Segment = Segments.Find(LaneKey);
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
			const bool bHasNextLane = TryResolveNextLane(SurfaceGrid, *Segment, NextLaneKey)
				&& Segments.Contains(NextLaneKey);
			if (bHasNextLane)
			{
				if (!ConveyorItemsByLane.Contains(NextLaneKey) && !NextItemsByLane.Contains(NextLaneKey))
				{
					Item.CurrentLane = NextLaneKey;
					Item.Progress = 0.0f;
					NextItemsByLane.Add(NextLaneKey, Item);
					continue;
				}
			}
			else if (TransferCount < FMath::Max(1, MaxItemTransfersPerTick)
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
	OutNextLane = FSRConveyorLaneKey();
	if (!IsValid(SurfaceGrid) || Segment.OutputDirection == ESRConveyorGridDirection::None)
	{
		return false;
	}

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	if (!SurfaceGrid->GetCellNeighbors(Segment.Lane.CellId, Neighbors))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellId NextCellId;
	if (!GetNeighborCellIdByDirection(Neighbors, Segment.OutputDirection, NextCellId))
	{
		return false;
	}

	OutNextLane = MakeLaneKey(NextCellId, Segment.Lane.Layer);
	return true;
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
	return bAutoTransportItems && (!Segments.IsEmpty() || !ConveyorItemsByLane.IsEmpty());
}
