#include "Assembly/SRAssemblyComponent.h"

#include "Automation/SRFacilityNetworkComponent.h"
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
		int32 PlacementRotationSteps,
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
			const FSRStructurePortSpec RotatedPortSpec = StarRovers::Structure::RotateStructurePortSpec(
				PortSpec,
				StructureData,
				PlacementRotationSteps);
			AppendFocusedFacilityPortInfo(
				SurfaceGrid,
				StructureInfo,
				ESRStructurePortKind::Input,
				RotatedPortSpec,
				FootprintCellsX,
				OutFacilityPorts);
		}

		for (const FSRStructurePortSpec& PortSpec : StructureData.OutputPorts)
		{
			const FSRStructurePortSpec RotatedPortSpec = StarRovers::Structure::RotateStructurePortSpec(
				PortSpec,
				StructureData,
				PlacementRotationSteps);
			AppendFocusedFacilityPortInfo(
				SurfaceGrid,
				StructureInfo,
				ESRStructurePortKind::Output,
				RotatedPortSpec,
				FootprintCellsX,
				OutFacilityPorts);
		}
	}

	float ResolveFocusedFacilityProcessSeconds(const FSRFacilityInstance& FacilityInstance)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			return 0.0f;
		}

		float ProcessSeconds = FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds);
		if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			ProcessSeconds *= 2.0f;
		}
		return ProcessSeconds;
	}

	void PopulateFocusedFacilityRuntimeInfo(AActor* FocusedActor, FSRFocusedSurfaceStructureInfo& StructureInfo)
	{
		if (!IsValid(FocusedActor) || StructureInfo.OccupantId.IsNone())
		{
			return;
		}

		USRFacilityNetworkComponent* FacilityNetwork = FocusedActor->FindComponentByClass<USRFacilityNetworkComponent>();
		if (!IsValid(FacilityNetwork))
		{
			return;
		}

		FSRFacilityInstance FacilityInstance;
		if (!FacilityNetwork->GetFacilityInstance(StructureInfo.OccupantId, FacilityInstance))
		{
			return;
		}

		FSRFocusedFacilityRuntimeInfo RuntimeInfo;
		RuntimeInfo.bIsValid = true;
		RuntimeInfo.TemperatureState = FacilityInstance.TemperatureState;
		RuntimeInfo.ProcessProgressSeconds = FacilityInstance.ProcessProgressSeconds;
		RuntimeInfo.ProcessSeconds = ResolveFocusedFacilityProcessSeconds(FacilityInstance);
		RuntimeInfo.bProcessing = FacilityInstance.bProcessing;
		RuntimeInfo.InputInventory = FacilityInstance.InputInventory;
		RuntimeInfo.ProcessingInventory = FacilityInstance.ProcessingInventory;
		RuntimeInfo.OutputInventory = FacilityInstance.OutputInventory;

		if (const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get())
		{
			RuntimeInfo.FacilityId = FacilityDataAsset->FacilityId;
			RuntimeInfo.DisplayName = FacilityDataAsset->DisplayName.IsEmpty()
				? FText::FromName(FacilityDataAsset->FacilityId)
				: FacilityDataAsset->DisplayName;
			RuntimeInfo.OperationKind = FacilityDataAsset->OperationKind;
		}

		StructureInfo.bHasFacilityRuntimeInfo = true;
		StructureInfo.FacilityRuntimeInfo = RuntimeInfo;
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

	void AppendUniqueCellIdsInPatch(
		const TArray<FSRPlanetSurfaceGridCellId>& SourceCellIds,
		const TSet<FSRPlanetSurfaceGridCellId>& PatchCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& TargetCellIds)
	{
		for (const FSRPlanetSurfaceGridCellId& CellId : SourceCellIds)
		{
			if (PatchCellIds.Contains(CellId))
			{
				TargetCellIds.AddUnique(CellId);
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

void USRAssemblyComponent::UpdateConveyorPlacementPortPreview()
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!bAssemblyModeActive
		|| !IsValid(PlayerController)
		|| PlayerController->IsPointerOverBlockingUi()
		|| !IsValid(SelectedStructureDataAsset)
		|| SelectedStructureDataAsset->BuildData().BuildKind != ESRStructureBuildKind::Conveyor
		|| !IsValid(HoveredSurfaceGrid))
	{
		ClearConveyorPlacementPortPreview();
		return;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	TArray<FSRPlanetSurfaceGridCellId> HoverPatchCellIds;
	if (!HoveredSurfaceGrid->GetHoveredCell(HoveredCell)
		|| !HoveredSurfaceGrid->GetInteractionGridPatchCellIds(HoveredCell.CellId, HoverPatchCellIds)
		|| HoverPatchCellIds.IsEmpty())
	{
		ClearConveyorPlacementPortPreview();
		return;
	}

	TSet<FSRPlanetSurfaceGridCellId> HoverPatchCellSet;
	HoverPatchCellSet.Reserve(HoverPatchCellIds.Num());
	for (const FSRPlanetSurfaceGridCellId& PatchCellId : HoverPatchCellIds)
	{
		HoverPatchCellSet.Add(PatchCellId);
	}

	AActor* SurfaceOwner = HoveredSurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	if (!IsValid(StructureInstanceManager))
	{
		ClearConveyorPlacementPortPreview();
		return;
	}

	TArray<FSRPlacedStructureInstance> PlacedStructures;
	StructureInstanceManager->GetPlacedStructures(PlacedStructures);

	TArray<FSRPlanetSurfaceGridCellId> InputConnectionCellIds;
	TArray<FSRPlanetSurfaceGridCellId> OutputConnectionCellIds;
	for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
	{
		USRStructureDataAsset* StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
		if (PlacedStructure.bNaturalStructure
			|| !IsValid(StructureDataAsset)
			|| PlacedStructure.FootprintCellIds.IsEmpty())
		{
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.BuildKind != ESRStructureBuildKind::Structure
			|| (StructureData.InputPorts.IsEmpty() && StructureData.OutputPorts.IsEmpty()))
		{
			continue;
		}

		FSRFocusedSurfaceStructureInfo StructureInfo;
		StructureInfo.bIsValid = true;
		StructureInfo.OccupantId = PlacedStructure.OccupantId;
		StructureInfo.StructureId = PlacedStructure.StructureId;
		StructureInfo.OriginCellId = PlacedStructure.OriginCellId;
		StructureInfo.FootprintCellIds = PlacedStructure.FootprintCellIds;
		StructureInfo.StructureDataAsset = StructureDataAsset;
		StructureInfo.BuildKind = StructureData.BuildKind;
		StructureInfo.bNaturalStructure = PlacedStructure.bNaturalStructure;

		TArray<FSRFocusedFacilityPortInfo> FacilityPorts;
		BuildFocusedFacilityPortInfo(
			HoveredSurfaceGrid,
			StructureInfo,
			StructureData,
			StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacedStructure.PlacementRotationSteps),
			PlacedStructure.PlacementRotationSteps,
			FacilityPorts);

		TArray<FSRPlanetSurfaceGridCellId> StructureInputConnectionCellIds;
		TArray<FSRPlanetSurfaceGridCellId> StructureOutputConnectionCellIds;
		GatherFacilityPortPreviewCells(FacilityPorts, StructureInputConnectionCellIds, StructureOutputConnectionCellIds);
		AppendUniqueCellIdsInPatch(StructureInputConnectionCellIds, HoverPatchCellSet, InputConnectionCellIds);
		AppendUniqueCellIdsInPatch(StructureOutputConnectionCellIds, HoverPatchCellSet, OutputConnectionCellIds);
	}

	if (InputConnectionCellIds.IsEmpty() && OutputConnectionCellIds.IsEmpty())
	{
		ClearConveyorPlacementPortPreview();
		return;
	}

	if (IsValid(ConveyorPortPreviewSurfaceGrid) && ConveyorPortPreviewSurfaceGrid != HoveredSurfaceGrid)
	{
		ConveyorPortPreviewSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	HoveredSurfaceGrid->SetFacilityPortPreviewCells(InputConnectionCellIds, OutputConnectionCellIds);
	ConveyorPortPreviewSurfaceGrid = HoveredSurfaceGrid;
	bHasConveyorPortPreview = true;
}

void USRAssemblyComponent::ClearConveyorPlacementPortPreview()
{
	if (bHasConveyorPortPreview && IsValid(ConveyorPortPreviewSurfaceGrid))
	{
		ConveyorPortPreviewSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	ConveyorPortPreviewSurfaceGrid = nullptr;
	bHasConveyorPortPreview = false;
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
	int32 PlacedStructureRotationSteps = 0;

	if (USRStructureInstanceManagerComponent* StructureInstanceManager = FocusedActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
	{
		FSRPlacedStructureInstance PlacedStructure;
		if (StructureInstanceManager->GetPlacedStructure(ClickedCellInfo.OccupantId, PlacedStructure))
		{
			StructureInfo.StructureId = PlacedStructure.StructureId;
			StructureInfo.OriginCellId = PlacedStructure.OriginCellId;
			StructureInfo.FootprintCellIds = PlacedStructure.FootprintCellIds;
			PlacedStructureRotationSteps = PlacedStructure.PlacementRotationSteps;
			StructureInfo.StructureDataAsset = PlacedStructure.StructureDataAsset;
			StructureInfo.bNaturalStructure = PlacedStructure.bNaturalStructure;
		}
	}

	if (IsValid(StructureInfo.StructureDataAsset.Get()))
	{
		const FSRStructureData StructureData = StructureInfo.StructureDataAsset->BuildData();
		StructureDataForPorts = StructureData;
		bHasStructureDataForPorts = true;
		StructureFootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacedStructureRotationSteps);
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
		BuildFocusedFacilityPortInfo(
			SurfaceGrid,
			StructureInfo,
			StructureDataForPorts,
			StructureFootprintCellsX,
			PlacedStructureRotationSteps,
			StructureInfo.FacilityPorts);
	}
	PopulateFocusedFacilityRuntimeInfo(FocusedActor, StructureInfo);

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

void USRAssemblyComponent::ClearSelectedStructureFocus()
{
	ClearSelectedStructureInfo();
}
