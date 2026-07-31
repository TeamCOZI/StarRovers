#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Conveyor/SRConveyorTickCoordinator.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Utility/SRLog.h"

namespace
{
	bool LaneLess(const FSRConveyorLaneKey& Left, const FSRConveyorLaneKey& Right)
	{
		if (Left.CellId.Face != Right.CellId.Face)
		{
			return static_cast<int32>(Left.CellId.Face) < static_cast<int32>(Right.CellId.Face);
		}
		if (Left.CellId.CellX != Right.CellId.CellX)
		{
			return Left.CellId.CellX < Right.CellId.CellX;
		}
		if (Left.CellId.CellY != Right.CellId.CellY)
		{
			return Left.CellId.CellY < Right.CellId.CellY;
		}
		return Left.Layer < Right.Layer;
	}

	FString BuildPathIdentity(const FSRConveyorBeltPath& Path)
	{
		FString Identity = FString::Printf(TEXT("%d|%s|"), Path.Layer, *Path.NetworkId.ToString());
		for (const FSRPlanetSurfaceGridCellId& CellId : Path.CellIds)
		{
			Identity += FString::Printf(
				TEXT("%d,%d,%d;"),
				static_cast<int32>(CellId.Face),
				CellId.CellX,
				CellId.CellY);
		}
		return Identity;
	}
}

void USRConveyorNetworkComponent::ExportSaveData(FSRConveyorNetworkSaveData& OutSaveData) const
{
	OutSaveData = FSRConveyorNetworkSaveData();
	OutSaveData.BeltPaths = BeltPaths;
	TransportState.ItemsByLane.GenerateValueArray(OutSaveData.Items);
	OutSaveData.Items.Sort([](const FSRConveyorItem& Left, const FSRConveyorItem& Right)
	{
		return LaneLess(Left.CurrentLane, Right.CurrentLane);
	});
	OutSaveData.SegmentFlows.Reserve(Segments.Num());
	for (const TPair<FSRConveyorLaneKey, FSRConveyorSegment>& Pair : Segments)
	{
		FSRConveyorSegmentFlowSaveData& Flow = OutSaveData.SegmentFlows.AddDefaulted_GetRef();
		Flow.Lane = Pair.Key;
		Flow.NextOutputDirectionIndex = Pair.Value.NextOutputDirectionIndex;
		Flow.NextInputDirectionIndex = Pair.Value.NextInputDirectionIndex;
	}
	OutSaveData.SegmentFlows.Sort([](const FSRConveyorSegmentFlowSaveData& Left, const FSRConveyorSegmentFlowSaveData& Right)
	{
		return LaneLess(Left.Lane, Right.Lane);
	});
}

bool USRConveyorNetworkComponent::CanImportSaveData(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorNetworkSaveData& SaveData,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!IsValid(SurfaceGrid))
	{
		OutFailureReason = TEXT("Conveyor save requires a valid surface grid.");
		return false;
	}
	if (!StarRovers::Save::Conveyor::IsSupportedVersion(SaveData.Version))
	{
		OutFailureReason = FString::Printf(TEXT("Unsupported conveyor save version %d."), SaveData.Version);
		return false;
	}

	TSet<FString> PathIdentities;
	TSet<FSRConveyorLaneKey> SavedLanes;
	for (const FSRConveyorBeltPath& Path : SaveData.BeltPaths)
	{
		const FSRStructureData StructureData = IsValid(Path.StructureDataAsset.Get())
			? Path.StructureDataAsset->BuildData()
			: FSRStructureData();
		bool bDuplicatePath = false;
		PathIdentities.Add(BuildPathIdentity(Path), &bDuplicatePath);
		if (Path.CellIds.IsEmpty()
			|| Path.Layer < 0
			|| !FMath::IsFinite(Path.LayerHeight)
			|| Path.LayerHeight < 0.0f
			|| !IsValid(Path.StructureDataAsset.Get())
			|| StructureData.BuildKind != ESRStructureBuildKind::Conveyor
			|| bDuplicatePath)
		{
			OutFailureReason = TEXT("Conveyor save contains an invalid or duplicate belt path.");
			return false;
		}
		for (const FSRPlanetSurfaceGridCellId& CellId : Path.CellIds)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
			{
				OutFailureReason = TEXT("Conveyor path references a cell outside the current surface topology.");
				return false;
			}
			FSRConveyorLaneKey Lane;
			Lane.CellId = CellId;
			Lane.Layer = Path.Layer;
			SavedLanes.Add(Lane);
		}
	}

	TSet<FSRConveyorLaneKey> ItemLanes;
	for (const FSRConveyorItem& Item : SaveData.Items)
	{
		bool bDuplicateLane = false;
		ItemLanes.Add(Item.CurrentLane, &bDuplicateLane);
		if (bDuplicateLane
			|| !SavedLanes.Contains(Item.CurrentLane)
			|| !FMath::IsFinite(Item.Progress)
			|| Item.Progress < 0.0f
			|| Item.Progress >= 1.0f
			|| !StarRovers::PatternRouting::IsValidPatternPayload(Item.ResourceInstance))
		{
			OutFailureReason = TEXT("Conveyor save contains an invalid, duplicate, or off-network Pattern item.");
			return false;
		}
	}

	TSet<FSRConveyorLaneKey> FlowLanes;
	for (const FSRConveyorSegmentFlowSaveData& Flow : SaveData.SegmentFlows)
	{
		bool bDuplicateLane = false;
		FlowLanes.Add(Flow.Lane, &bDuplicateLane);
		if (bDuplicateLane
			|| !SavedLanes.Contains(Flow.Lane)
			|| Flow.NextOutputDirectionIndex < 0
			|| Flow.NextInputDirectionIndex < 0)
		{
			OutFailureReason = TEXT("Conveyor save contains invalid segment flow state.");
			return false;
		}
	}
	return true;
}

bool USRConveyorNetworkComponent::ApplySaveDataUnchecked(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorNetworkSaveData& SaveData)
{
	ClearConveyors();
	for (const FSRConveyorBeltPath& Path : SaveData.BeltPaths)
	{
		if (!TryPlaceConveyorPath(
			SurfaceGrid,
			Path.CellIds,
			Path.Layer,
			Path.LayerHeight,
			Path.StructureDataAsset,
			Path.NetworkId))
		{
			return false;
		}
	}
	for (const FSRConveyorSegmentFlowSaveData& Flow : SaveData.SegmentFlows)
	{
		FSRConveyorSegment* Segment = Segments.Find(Flow.Lane);
		if (!Segment)
		{
			return false;
		}
		Segment->NextOutputDirectionIndex = Flow.NextOutputDirectionIndex;
		Segment->NextInputDirectionIndex = Flow.NextInputDirectionIndex;
	}
	TransportState.ResetItems();
	for (const FSRConveyorItem& Item : SaveData.Items)
	{
		TransportState.ItemsByLane.Add(Item.CurrentLane, Item);
	}
	SetComponentTickEnabled(StarRovers::Conveyor::FSRConveyorTickCoordinator::ShouldKeepTickEnabled(
		HasDirtyConveyorActorGroups(),
		ShouldKeepTransportTickEnabled(),
		bShowPathDebugLine,
		bShowConnectionDebugLine));
	return true;
}

bool USRConveyorNetworkComponent::ImportSaveData(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorNetworkSaveData& SaveData)
{
	FString FailureReason;
	if (!CanImportSaveData(SurfaceGrid, SaveData, FailureReason))
	{
		SR_LOG(Conveyor, LogTemp, Error, TEXT("Conveyor save import rejected for '%s': %s"), *GetNameSafe(GetOwner()), *FailureReason);
		return false;
	}

	FSRConveyorNetworkSaveData RollbackData;
	ExportSaveData(RollbackData);
	if (ApplySaveDataUnchecked(SurfaceGrid, SaveData))
	{
		return true;
	}
	ApplySaveDataUnchecked(SurfaceGrid, RollbackData);
	SR_LOG(Conveyor, LogTemp, Error, TEXT("Conveyor save import failed during commit for '%s'; previous state was restored."), *GetNameSafe(GetOwner()));
	return false;
}
