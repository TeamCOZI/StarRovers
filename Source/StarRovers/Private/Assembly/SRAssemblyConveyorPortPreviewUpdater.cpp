#include "Assembly/SRAssemblyConveyorPortPreviewUpdater.h"

#include "Assembly/SRAssemblyPreviewState.h"
#include "Assembly/SRAssemblySurfaceFocusInfoBuilder.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool AppendHoveredCellIdIfPresent(
		const TArray<FSRPlanetSurfaceGridCellId>& SourceCellIds,
		const FSRPlanetSurfaceGridCellId& HoveredCellId,
		TArray<FSRPlanetSurfaceGridCellId>& TargetCellIds)
	{
		if (TargetCellIds.IsEmpty() && SourceCellIds.Contains(HoveredCellId))
		{
			TargetCellIds.Add(HoveredCellId);
			return true;
		}

		return false;
	}
}

bool StarRovers::Assembly::FSRAssemblyConveyorPortPreviewUpdater::Update(
	USRPlanetSurfaceGrid* HoveredSurfaceGrid,
	USRStructureDataAsset* SelectedStructureDataAsset,
	FSRAssemblyConveyorPreviewState& ConveyorPreview)
{
	if (!IsValid(HoveredSurfaceGrid)
		|| !IsValid(SelectedStructureDataAsset)
		|| SelectedStructureDataAsset->BuildData().BuildKind != ESRStructureBuildKind::Conveyor)
	{
		return false;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	if (!HoveredSurfaceGrid->GetHoveredCell(HoveredCell))
	{
		return false;
	}

	AActor* SurfaceOwner = HoveredSurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	if (!IsValid(StructureInstanceManager))
	{
		return false;
	}

	TArray<FSRPlacedStructureInstance> PlacedStructures;
	StructureInstanceManager->GetPlacedStructures(PlacedStructures);

	TArray<FSRPlanetSurfaceGridCellId> InputConnectionCellIds;
	TArray<FSRPlanetSurfaceGridCellId> OutputConnectionCellIds;
	InputConnectionCellIds.Reserve(1);
	OutputConnectionCellIds.Reserve(1);
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
		StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::BuildFocusedFacilityPortInfo(
			HoveredSurfaceGrid,
			StructureInfo,
			StructureData,
			StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacedStructure.PlacementRotationSteps),
			StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacedStructure.PlacementRotationSteps),
			PlacedStructure.PlacementRotationSteps,
			FacilityPorts);

		TArray<FSRPlanetSurfaceGridCellId> StructureInputConnectionCellIds;
		TArray<FSRPlanetSurfaceGridCellId> StructureOutputConnectionCellIds;
		StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::GatherFacilityPortPreviewCells(
			FacilityPorts,
			StructureInputConnectionCellIds,
			StructureOutputConnectionCellIds);
		AppendHoveredCellIdIfPresent(StructureInputConnectionCellIds, HoveredCell.CellId, InputConnectionCellIds);
		AppendHoveredCellIdIfPresent(StructureOutputConnectionCellIds, HoveredCell.CellId, OutputConnectionCellIds);
		if (!InputConnectionCellIds.IsEmpty() && !OutputConnectionCellIds.IsEmpty())
		{
			break;
		}
	}

	if (InputConnectionCellIds.IsEmpty() && OutputConnectionCellIds.IsEmpty())
	{
		return false;
	}

	if (IsValid(ConveyorPreview.ConveyorPortPreviewSurfaceGrid)
		&& ConveyorPreview.ConveyorPortPreviewSurfaceGrid != HoveredSurfaceGrid)
	{
		ConveyorPreview.ConveyorPortPreviewSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	HoveredSurfaceGrid->SetFacilityPortPreviewCells(InputConnectionCellIds, OutputConnectionCellIds);
	ConveyorPreview.ConveyorPortPreviewSurfaceGrid = HoveredSurfaceGrid;
	ConveyorPreview.bHasConveyorPortPreview = true;
	return true;
}
