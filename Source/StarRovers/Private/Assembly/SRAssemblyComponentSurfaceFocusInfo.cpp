#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool GetFacilityPortNeighborCellIds(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRStructurePortDirection Direction,
		TArray<FSRPlanetSurfaceGridCellId>& OutNeighborCellIds)
	{
		OutNeighborCellIds.Reset();
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(CellId, Neighbors))
		{
			return false;
		}

		switch (Direction)
		{
		case ESRStructurePortDirection::Left:
			OutNeighborCellIds.Add(Neighbors.NegativeU);
			break;
		case ESRStructurePortDirection::Right:
			OutNeighborCellIds.Add(Neighbors.PositiveU);
			break;
		case ESRStructurePortDirection::Top:
			OutNeighborCellIds.Add(Neighbors.NegativeV);
			break;
		case ESRStructurePortDirection::Bottom:
			OutNeighborCellIds.Add(Neighbors.PositiveV);
			break;
		default:
			break;
		}

		return !OutNeighborCellIds.IsEmpty();
	}

	bool ResolveFacilityFootprintCellId(
		const FSRFocusedSurfaceStructureInfo& StructureInfo,
		int32 FootprintCellsX,
		int32 FootprintCellX,
		int32 FootprintCellY,
		FSRPlanetSurfaceGridCellId& OutCellId)
	{
		OutCellId = FSRPlanetSurfaceGridCellId();
		if (StructureInfo.FootprintCellIds.IsEmpty())
		{
			return false;
		}

		const int32 SafeFootprintCellsX = FMath::Max(1, FootprintCellsX);
		if (FootprintCellX < 0 || FootprintCellY < 0)
		{
			return false;
		}

		const int32 FootprintIndex = FootprintCellY * SafeFootprintCellsX + FootprintCellX;
		if (StructureInfo.FootprintCellIds.IsValidIndex(FootprintIndex))
		{
			OutCellId = StructureInfo.FootprintCellIds[FootprintIndex];
			return true;
		}

		return false;
	}

	void AppendFocusedFacilityPortInfo(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFocusedSurfaceStructureInfo& StructureInfo,
		ESRStructurePortKind PortKind,
		const FSRStructurePortSpec& PortSpec,
		int32 FootprintCellsX,
		TArray<FSRFocusedFacilityPortInfo>& OutFacilityPorts)
	{
		FSRFocusedFacilityPortInfo PortInfo;
		PortInfo.PortKind = PortKind;
		PortInfo.Direction = PortSpec.Direction;
		PortInfo.FootprintCellX = FMath::Max(0, PortSpec.CellOffsetX);
		PortInfo.FootprintCellY = FMath::Max(0, PortSpec.CellOffsetY);
		if (!ResolveFacilityFootprintCellId(StructureInfo, FootprintCellsX, PortSpec.CellOffsetX, PortSpec.CellOffsetY, PortInfo.FootprintCellId))
		{
			return;
		}

		GetFacilityPortNeighborCellIds(SurfaceGrid, PortInfo.FootprintCellId, PortSpec.Direction, PortInfo.ConnectionCellIds);
		PortInfo.ConnectionCellIds.RemoveAll([&StructureInfo](const FSRPlanetSurfaceGridCellId& ConnectionCellId)
		{
			return StructureInfo.FootprintCellIds.Contains(ConnectionCellId);
		});

		OutFacilityPorts.Add(PortInfo);
	}

	void BuildFocusedFacilityPortInfo(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFocusedSurfaceStructureInfo& StructureInfo,
		const FSRStructureData& StructureData,
		int32 FootprintCellsX,
		TArray<FSRFocusedFacilityPortInfo>& OutFacilityPorts)
	{
		OutFacilityPorts.Reset();
		if (!IsValid(SurfaceGrid)
			|| StructureData.BuildKind != ESRStructureBuildKind::Structure
			|| StructureInfo.bNaturalStructure)
		{
			return;
		}

		for (const FSRStructurePortSpec& PortSpec : StructureData.InputPorts)
		{
			AppendFocusedFacilityPortInfo(
				SurfaceGrid,
				StructureInfo,
				ESRStructurePortKind::Input,
				PortSpec,
				FootprintCellsX,
				OutFacilityPorts);
		}

		for (const FSRStructurePortSpec& PortSpec : StructureData.OutputPorts)
		{
			AppendFocusedFacilityPortInfo(
				SurfaceGrid,
				StructureInfo,
				ESRStructurePortKind::Output,
				PortSpec,
				FootprintCellsX,
				OutFacilityPorts);
		}
	}

	void GatherFacilityPortPreviewCells(
		const TArray<FSRFocusedFacilityPortInfo>& FacilityPorts,
		TArray<FSRPlanetSurfaceGridCellId>& OutInputConnectionCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& OutOutputConnectionCellIds)
	{
		OutInputConnectionCellIds.Reset();
		OutOutputConnectionCellIds.Reset();

		for (const FSRFocusedFacilityPortInfo& PortInfo : FacilityPorts)
		{
			TArray<FSRPlanetSurfaceGridCellId>& TargetCellIds = PortInfo.PortKind == ESRStructurePortKind::Output
				? OutOutputConnectionCellIds
				: OutInputConnectionCellIds;
			for (const FSRPlanetSurfaceGridCellId& ConnectionCellId : PortInfo.ConnectionCellIds)
			{
				TargetCellIds.AddUnique(ConnectionCellId);
			}
		}
	}
}

void USRAssemblyComponent::PublishHoveredCellInfo(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !IsValid(SurfaceGrid))
	{
		return;
	}

	if (bHasLastPublishedHoveredCellInfo
		&& LastPublishedHoveredSurfaceGrid == SurfaceGrid
		&& LastPublishedHoveredCellId == HoveredCell.CellId)
	{
		return;
	}

	FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
	if (!SurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo))
	{
		ClearPublishedHoveredCellInfo();
		return;
	}

	bHasLastPublishedHoveredCellInfo = true;
	LastPublishedHoveredSurfaceGrid = SurfaceGrid;
	LastPublishedHoveredCellId = HoveredCell.CellId;
	PlayerController->SetHoveredSurfaceCellInfo(true, HoveredCellInfo);
}

void USRAssemblyComponent::ClearPublishedHoveredCellInfo()
{
	if (!bHasLastPublishedHoveredCellInfo)
	{
		return;
	}

	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	if (ASRPlayerController* PlayerController = GetOwnerController())
	{
		PlayerController->SetHoveredSurfaceCellInfo(false, FSRPlanetSurfaceGridCellInfo());
	}
}

bool USRAssemblyComponent::TryPublishSelectedStructureInfo(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& ClickedCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !IsValid(FocusedActor) || !IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo ClickedCellInfo;
	if (!SurfaceGrid->GetCellInfoById(ClickedCell.CellId, ClickedCellInfo)
		|| !ClickedCellInfo.bOccupied
		|| ClickedCellInfo.OccupantId.IsNone())
	{
		return false;
	}

	FSRFocusedSurfaceStructureInfo StructureInfo;
	StructureInfo.bIsValid = true;
	StructureInfo.OccupantId = ClickedCellInfo.OccupantId;
	StructureInfo.ClickedCellInfo = ClickedCellInfo;
	StructureInfo.OriginCellId = ClickedCell.CellId;
	FSRStructureData StructureDataForPorts;
	bool bHasStructureDataForPorts = false;
	int32 StructureFootprintCellsX = 1;

	if (USRStructureInstanceManagerComponent* StructureInstanceManager = FocusedActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
	{
		FSRPlacedStructureInstance PlacedStructure;
		if (StructureInstanceManager->GetPlacedStructure(ClickedCellInfo.OccupantId, PlacedStructure))
		{
			StructureInfo.StructureId = PlacedStructure.StructureId;
			StructureInfo.OriginCellId = PlacedStructure.OriginCellId;
			StructureInfo.FootprintCellIds = PlacedStructure.FootprintCellIds;
			StructureInfo.StructureDataAsset = PlacedStructure.StructureDataAsset;
			StructureInfo.bNaturalStructure = PlacedStructure.bNaturalStructure;
		}
	}

	if (IsValid(StructureInfo.StructureDataAsset.Get()))
	{
		const FSRStructureData StructureData = StructureInfo.StructureDataAsset->BuildData();
		StructureDataForPorts = StructureData;
		bHasStructureDataForPorts = true;
		StructureFootprintCellsX = FMath::Max(1, StructureData.FootprintCellsX);
		StructureInfo.StructureId = StructureInfo.StructureId.IsNone() ? StructureData.StructureId : StructureInfo.StructureId;
		StructureInfo.DisplayName = StructureData.DisplayName.IsEmpty()
			? FText::FromName(StructureInfo.StructureId)
			: StructureData.DisplayName;
		StructureInfo.Description = StructureData.Description;
		StructureInfo.BuildKind = StructureData.BuildKind;
		StructureInfo.bHasFacilityDataAsset = IsValid(StructureData.FacilityDataAsset.Get());
	}
	else
	{
		StructureInfo.DisplayName = FText::FromName(ClickedCellInfo.OccupantId);
	}

	if (StructureInfo.FootprintCellIds.IsEmpty())
	{
		StructureInfo.FootprintCellIds.Add(ClickedCell.CellId);
	}

	if (bHasStructureDataForPorts)
	{
		BuildFocusedFacilityPortInfo(SurfaceGrid, StructureInfo, StructureDataForPorts, StructureFootprintCellsX, StructureInfo.FacilityPorts);
	}

	TArray<FSRPlanetSurfaceGridCellId> InputConnectionCellIds;
	TArray<FSRPlanetSurfaceGridCellId> OutputConnectionCellIds;
	GatherFacilityPortPreviewCells(StructureInfo.FacilityPorts, InputConnectionCellIds, OutputConnectionCellIds);
	SurfaceGrid->SetFacilityPortPreviewCells(InputConnectionCellIds, OutputConnectionCellIds);

	PlayerController->SetSelectedActorSurfaceStructureInfo(FocusedActor, StructureInfo);
	return true;
}

void USRAssemblyComponent::ClearSelectedStructureInfo()
{
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	if (TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid))
	{
		FocusedSurfaceGrid->ClearFacilityPortPreviewCells();
	}
	else if (IsValid(ActiveAssemblySurfaceGrid))
	{
		ActiveAssemblySurfaceGrid->ClearFacilityPortPreviewCells();
	}

	if (ASRPlayerController* PlayerController = GetOwnerController())
	{
		PlayerController->SetSelectedSurfaceStructureInfo(false, FSRFocusedSurfaceStructureInfo());
	}
}
