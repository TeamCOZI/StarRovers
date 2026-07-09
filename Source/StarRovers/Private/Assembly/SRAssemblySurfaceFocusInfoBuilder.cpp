#include "Assembly/SRAssemblySurfaceFocusInfoBuilder.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructureSurfacePortConnection.h"
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
		FSRPlanetSurfaceGridCellId NeighborCellId;
		if (!StarRovers::Structure::SurfacePorts::TryGetPortConnectionCellId(
			SurfaceGrid,
			CellId,
			Direction,
			NeighborCellId))
		{
			return false;
		}

		OutNeighborCellIds.Add(NeighborCellId);
		return true;
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
}

bool StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::TryBuildSelectedStructureInfo(
	AActor* FocusedActor,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& ClickedCell,
	FSRFocusedSurfaceStructureInfo& OutStructureInfo)
{
	OutStructureInfo = FSRFocusedSurfaceStructureInfo();
	if (!IsValid(FocusedActor) || !IsValid(SurfaceGrid))
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

	OutStructureInfo.bIsValid = true;
	OutStructureInfo.OccupantId = ClickedCellInfo.OccupantId;
	OutStructureInfo.ClickedCellInfo = ClickedCellInfo;
	OutStructureInfo.OriginCellId = ClickedCell.CellId;

	FSRStructureData StructureDataForPorts;
	bool bHasStructureDataForPorts = false;
	int32 StructureFootprintCellsX = 1;
	int32 PlacedStructureRotationSteps = 0;

	if (USRStructureInstanceManagerComponent* StructureInstanceManager = FocusedActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
	{
		FSRPlacedStructureInstance PlacedStructure;
		if (StructureInstanceManager->GetPlacedStructure(ClickedCellInfo.OccupantId, PlacedStructure))
		{
			OutStructureInfo.StructureId = PlacedStructure.StructureId;
			OutStructureInfo.OriginCellId = PlacedStructure.OriginCellId;
			OutStructureInfo.FootprintCellIds = PlacedStructure.FootprintCellIds;
			PlacedStructureRotationSteps = PlacedStructure.PlacementRotationSteps;
			OutStructureInfo.StructureDataAsset = PlacedStructure.StructureDataAsset;
			OutStructureInfo.bNaturalStructure = PlacedStructure.bNaturalStructure;
		}
	}

	if (IsValid(OutStructureInfo.StructureDataAsset.Get()))
	{
		const FSRStructureData StructureData = OutStructureInfo.StructureDataAsset->BuildData();
		StructureDataForPorts = StructureData;
		bHasStructureDataForPorts = true;
		StructureFootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacedStructureRotationSteps);
		OutStructureInfo.StructureId = OutStructureInfo.StructureId.IsNone() ? StructureData.StructureId : OutStructureInfo.StructureId;
		OutStructureInfo.DisplayName = StructureData.DisplayName.IsEmpty()
			? FText::FromName(OutStructureInfo.StructureId)
			: StructureData.DisplayName;
		OutStructureInfo.Description = StructureData.Description;
		OutStructureInfo.BuildKind = StructureData.BuildKind;
		OutStructureInfo.bHasFacilityDataAsset = IsValid(StructureData.FacilityDataAsset.Get());
	}
	else
	{
		OutStructureInfo.DisplayName = FText::FromName(ClickedCellInfo.OccupantId);
	}

	if (OutStructureInfo.FootprintCellIds.IsEmpty())
	{
		OutStructureInfo.FootprintCellIds.Add(ClickedCell.CellId);
	}

	if (bHasStructureDataForPorts)
	{
		BuildFocusedFacilityPortInfo(
			SurfaceGrid,
			OutStructureInfo,
			StructureDataForPorts,
			StructureFootprintCellsX,
			PlacedStructureRotationSteps,
			OutStructureInfo.FacilityPorts);
	}

	PopulateFocusedFacilityRuntimeInfo(FocusedActor, OutStructureInfo);
	return true;
}

void StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::BuildFocusedFacilityPortInfo(
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

void StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::GatherFacilityPortPreviewCells(
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
