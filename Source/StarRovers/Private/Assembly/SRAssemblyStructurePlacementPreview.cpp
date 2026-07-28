#include "Assembly/SRAssemblyStructurePlacementPreview.h"

#include "Assembly/SRAssemblyConstructionReplacement.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	void PopulateCapacity(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRStructureData& StructureData,
		FSRStructurePlacementPreview& Preview)
	{
		const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
		Preview.OperationalLoad = IsValid(FacilityDataAsset)
			? FMath::Max(0, FacilityDataAsset->OperationalLoad)
			: 0;

		AActor* SurfaceOwner = IsValid(SurfaceGrid) ? SurfaceGrid->GetOwner() : nullptr;
		const USRFacilityNetworkComponent* FacilityNetwork = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRFacilityNetworkComponent>()
			: nullptr;
		if (!IsValid(FacilityNetwork))
		{
			return;
		}

		const FSROperationalCapacityReportV2 CapacityReport = FacilityNetwork->GetOperationalCapacityReport();
		Preview.bHasCapacityData = true;
		Preview.CurrentDemand = FMath::Max(0, CapacityReport.TotalDemand);
		Preview.TotalCapacity = FMath::Max(0, CapacityReport.TotalCapacity);
		Preview.RemainingCapacity = CapacityReport.RemainingCapacity;
		Preview.ProjectedDemand = Preview.CurrentDemand + Preview.OperationalLoad;
		Preview.bCapacityWarning = Preview.OperationalLoad > FMath::Max(0.0f, Preview.RemainingCapacity);
		Preview.CapacityText = FText::Format(
			Preview.bCapacityWarning
				? NSLOCTEXT("StarRoversPlacementPreview", "CapacityWarning", "Capacity {0}/{1} -> {2}/{1}. Placement is allowed, but this facility may be throttled.")
				: NSLOCTEXT("StarRoversPlacementPreview", "CapacityReady", "Capacity {0}/{1} -> {2}/{1}"),
			FText::AsNumber(Preview.CurrentDemand),
			FText::AsNumber(Preview.TotalCapacity),
			FText::AsNumber(Preview.ProjectedDemand));
	}

	void SetStatus(
		FSRStructurePlacementPreview& Preview,
		ESRStructurePlacementPreviewStatus Status,
		const FText& StatusText,
		const FText& DetailText)
	{
		Preview.Status = Status;
		Preview.StatusText = StatusText;
		Preview.DetailText = DetailText;
	}
}

FSRStructurePlacementEvaluation FSRAssemblyStructurePlacementPreviewEvaluator::Evaluate(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRStructureDataAsset* StructureDataAsset,
	int32 PlacementRotationSteps)
{
	FSRStructurePlacementEvaluation Evaluation;
	FSRStructurePlacementPreview& Preview = Evaluation.Preview;
	if (!IsValid(StructureDataAsset))
	{
		SetStatus(
			Preview,
			ESRStructurePlacementPreviewStatus::Inactive,
			NSLOCTEXT("StarRoversPlacementPreview", "Inactive", "No structure selected"),
			NSLOCTEXT("StarRoversPlacementPreview", "InactiveDetail", "Select a Build Dock card to begin placement."));
		return Evaluation;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	Preview.FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacementRotationSteps);
	Preview.FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacementRotationSteps);
	Preview.FootprintCellCount = Preview.FootprintCellsX * Preview.FootprintCellsY;
	PopulateCapacity(SurfaceGrid, StructureData, Preview);

	if (!StructureData.bAvailableForConstruction || StructureData.bIsResourceDeposit)
	{
		SetStatus(
			Preview,
			ESRStructurePlacementPreviewStatus::InvalidDefinition,
			NSLOCTEXT("StarRoversPlacementPreview", "Unavailable", "Unavailable"),
			NSLOCTEXT("StarRoversPlacementPreview", "UnavailableDetail", "This definition cannot be constructed by the player."));
		return Evaluation;
	}

	if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
	{
		SetStatus(
			Preview,
			ESRStructurePlacementPreviewStatus::ConveyorPath,
			NSLOCTEXT("StarRoversPlacementPreview", "ConveyorPath", "Route conveyor path"),
			NSLOCTEXT("StarRoversPlacementPreview", "ConveyorPathDetail", "Click or drag across valid surface cells to define the route."));
		return Evaluation;
	}

	if (!IsValid(SurfaceGrid))
	{
		SetStatus(
			Preview,
			ESRStructurePlacementPreviewStatus::AwaitingSurface,
			NSLOCTEXT("StarRoversPlacementPreview", "AwaitingSurface", "Choose a surface cell"),
			NSLOCTEXT("StarRoversPlacementPreview", "AwaitingSurfaceDetail", "Move the cursor over a constructible celestial-body surface."));
		return Evaluation;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	if (!SurfaceGrid->GetHoveredCell(HoveredCell))
	{
		SetStatus(
			Preview,
			ESRStructurePlacementPreviewStatus::AwaitingSurface,
			NSLOCTEXT("StarRoversPlacementPreview", "AwaitingCell", "Choose a surface cell"),
			NSLOCTEXT("StarRoversPlacementPreview", "AwaitingCellDetail", "The selected structure is ready; move the cursor onto the grid."));
		return Evaluation;
	}
	Preview.bHasTarget = true;

	if (!SurfaceGrid->GetFootprintCellIds(
		HoveredCell.CellId,
		Preview.FootprintCellsX,
		Preview.FootprintCellsY,
		Evaluation.FootprintCellIds))
	{
		Evaluation.FootprintCellIds.Add(HoveredCell.CellId);
		SetStatus(
			Preview,
			ESRStructurePlacementPreviewStatus::OutsideSurface,
			NSLOCTEXT("StarRoversPlacementPreview", "OutsideSurface", "Footprint crosses the surface boundary"),
			NSLOCTEXT("StarRoversPlacementPreview", "OutsideSurfaceDetail", "Move the origin cell inward or rotate the structure."));
		return Evaluation;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr;

	StarRovers::Assembly::ConstructionReplacement::FSRConstructionReplacementTargets ReplacementTargets;
	const bool bCanBuild = IsValid(StructureInstanceManager)
		? StarRovers::Assembly::ConstructionReplacement::CanBuildOverCellsForStructureConstruction(
			SurfaceGrid,
			StructureInstanceManager,
			ConveyorNetwork,
			Evaluation.FootprintCellIds,
			ReplacementTargets)
		: SurfaceGrid->CanOccupyCells(Evaluation.FootprintCellIds);

	if (!bCanBuild)
	{
		bool bHasTerrainBlock = false;
		for (const FSRPlanetSurfaceGridCellId& CellId : Evaluation.FootprintCellIds)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo) || (!CellInfo.bOccupied && !CellInfo.bCanConstruct))
			{
				bHasTerrainBlock = true;
				break;
			}
		}

		SetStatus(
			Preview,
			bHasTerrainBlock
				? ESRStructurePlacementPreviewStatus::BlockedTerrain
				: ESRStructurePlacementPreviewStatus::BlockedOccupancy,
			bHasTerrainBlock
				? NSLOCTEXT("StarRoversPlacementPreview", "BlockedTerrain", "Terrain blocks this footprint")
				: NSLOCTEXT("StarRoversPlacementPreview", "BlockedOccupancy", "Another structure blocks this footprint"),
			bHasTerrainBlock
				? NSLOCTEXT("StarRoversPlacementPreview", "BlockedTerrainDetail", "Every footprint cell must support construction.")
				: NSLOCTEXT("StarRoversPlacementPreview", "BlockedOccupancyDetail", "Move the structure or remove the non-replaceable occupant."));
		return Evaluation;
	}

	Evaluation.ReplacementStructureIds = MoveTemp(ReplacementTargets.StructureOccupantIds);
	Evaluation.ReplacementConveyorCellIds = MoveTemp(ReplacementTargets.ConveyorCellIds);
	Evaluation.ReplacementConveyorBeltPaths = MoveTemp(ReplacementTargets.ConveyorBeltPaths);
	Preview.ReplacementStructureCount = Evaluation.ReplacementStructureIds.Num();
	Preview.ReplacementConveyorCellCount = Evaluation.ReplacementConveyorCellIds.Num();
	Preview.bWillReplace = Preview.ReplacementStructureCount > 0 || Preview.ReplacementConveyorCellCount > 0;
	Preview.bCanPlace = true;

	if (Preview.bWillReplace)
	{
		SetStatus(
			Preview,
			ESRStructurePlacementPreviewStatus::Replacement,
			NSLOCTEXT("StarRoversPlacementPreview", "Replacement", "Ready - replacement"),
			FText::Format(
				NSLOCTEXT("StarRoversPlacementPreview", "ReplacementDetail", "Will replace {0} structure(s) and {1} conveyor cell(s)."),
				FText::AsNumber(Preview.ReplacementStructureCount),
				FText::AsNumber(Preview.ReplacementConveyorCellCount)));
	}
	else
	{
		SetStatus(
			Preview,
			ESRStructurePlacementPreviewStatus::Ready,
			NSLOCTEXT("StarRoversPlacementPreview", "Ready", "Ready to place"),
			FText::Format(
				NSLOCTEXT("StarRoversPlacementPreview", "ReadyDetail", "Footprint {0}x{1} is clear."),
				FText::AsNumber(Preview.FootprintCellsX),
				FText::AsNumber(Preview.FootprintCellsY)));
	}
	return Evaluation;
}
