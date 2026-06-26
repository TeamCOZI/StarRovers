#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructure.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureSurfacePortHelpers.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool GetGhostPortNeighborCellId(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRStructurePortDirection Direction,
		FSRPlanetSurfaceGridCellId& OutNeighborCellId)
	{
		return StarRovers::Structure::SurfacePorts::TryGetPortConnectionCellId(
			SurfaceGrid,
			CellId,
			Direction,
			OutNeighborCellId);
	}

	bool ResolveGhostPortFootprintCellId(
		const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
		int32 FootprintCellsX,
		int32 FootprintCellsY,
		const FSRStructurePortSpec& PortSpec,
		FSRPlanetSurfaceGridCellId& OutFootprintCellId)
	{
		OutFootprintCellId = FSRPlanetSurfaceGridCellId();

		const int32 SafeFootprintCellsX = FMath::Max(1, FootprintCellsX);
		const int32 SafeFootprintCellsY = FMath::Max(1, FootprintCellsY);
		if (PortSpec.CellOffsetX < 0
			|| PortSpec.CellOffsetY < 0
			|| PortSpec.CellOffsetX >= SafeFootprintCellsX
			|| PortSpec.CellOffsetY >= SafeFootprintCellsY)
		{
			return false;
		}

		const int32 FootprintIndex = PortSpec.CellOffsetY * SafeFootprintCellsX + PortSpec.CellOffsetX;
		if (!FootprintCellIds.IsValidIndex(FootprintIndex))
		{
			return false;
		}

		OutFootprintCellId = FootprintCellIds[FootprintIndex];
		return true;
	}

	void AppendGhostPortPreviewCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
		int32 FootprintCellsX,
		int32 FootprintCellsY,
		const FSRStructurePortSpec& PortSpec,
		TArray<FSRPlanetSurfaceGridCellId>& OutConnectionCellIds)
	{
		FSRPlanetSurfaceGridCellId PortFootprintCellId;
		if (!ResolveGhostPortFootprintCellId(FootprintCellIds, FootprintCellsX, FootprintCellsY, PortSpec, PortFootprintCellId))
		{
			return;
		}

		FSRPlanetSurfaceGridCellId ConnectionCellId;
		if (!GetGhostPortNeighborCellId(SurfaceGrid, PortFootprintCellId, PortSpec.Direction, ConnectionCellId)
			|| FootprintCellIds.Contains(ConnectionCellId))
		{
			return;
		}

		OutConnectionCellIds.AddUnique(ConnectionCellId);
	}
}

void USRAssemblyComponent::UpdateStructureGhostPreview()
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!bAssemblyModeActive || !IsValid(PlayerController) || !IsValid(SelectedStructureDataAsset) || !IsValid(HoveredSurfaceGrid))
	{
		DestroyStructureGhostPreview();
		return;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	if (!HoveredSurfaceGrid->GetHoveredCell(HoveredCell))
	{
		DestroyStructureGhostPreview();
		return;
	}

	FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
	if (!HoveredSurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo)
		|| !HoveredCellInfo.bCanConstruct
		|| HoveredCellInfo.bOccupied)
	{
		DestroyStructureGhostPreview();
		return;
	}

	const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
	if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
	{
		DestroyStructureGhostPreview();
		return;
	}
	const int32 RotationSteps = GetStructurePlacementRotationSteps();
	const int32 FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, RotationSteps);
	const int32 FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, RotationSteps);

	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	if (!HoveredSurfaceGrid->GetFootprintCellIds(HoveredCell.CellId, FootprintCellsX, FootprintCellsY, FootprintCellIds)
		|| !HoveredSurfaceGrid->CanOccupyCells(FootprintCellIds))
	{
		DestroyStructureGhostPreview();
		return;
	}

	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		StructureActorClass = ASRStructure::StaticClass();
	}
	if (!IsValid(StructureActorClass))
	{
		LogInvalidGhostDataAssetOnce(SelectedStructureDataAsset, TEXT("StructureActorClass is not set"));
		DestroyStructureGhostPreview();
		return;
	}

	if (!StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		LogInvalidGhostDataAssetOnce(SelectedStructureDataAsset, TEXT("StructureActorClass does not implement ISRBuildableStructureInterface"));
		DestroyStructureGhostPreview();
		return;
	}

	FTransform GhostTransform;
	if (!BuildStructureGhostTransform(HoveredSurfaceGrid, HoveredCell.CellId, SelectedStructureDataAsset, GhostTransform))
	{
		DestroyStructureGhostPreview();
		return;
	}

	const bool bNeedsNewGhostActor = !IsValid(StructureGhostActor)
		|| StructureGhostDataAsset != SelectedStructureDataAsset
		|| StructureGhostActor->GetClass() != StructureActorClass;
	if (bNeedsNewGhostActor)
	{
		DestroyStructureGhostPreview();

		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = PlayerController;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		StructureGhostActor = World->SpawnActor<AActor>(StructureActorClass, GhostTransform, SpawnParameters);
		if (!IsValid(StructureGhostActor))
		{
			return;
		}

		StructureGhostDataAsset = SelectedStructureDataAsset;
		ISRBuildableStructureInterface::Execute_ApplyStructureDataAsset(StructureGhostActor, SelectedStructureDataAsset);
		ISRBuildableStructureInterface::Execute_SetStructureGhostMode(StructureGhostActor, true);
		if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(StructureGhostActor, HoveredCellInfo))
		{
			DestroyStructureGhostPreview();
			return;
		}
		StructureGhostActor->SetActorHiddenInGame(false);
		LastLoggedInvalidGhostDataAsset = nullptr;
	}
	else if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(StructureGhostActor, HoveredCellInfo))
	{
		DestroyStructureGhostPreview();
		return;
	}

	StructureGhostActor->SetActorTransform(GhostTransform);
	StructureGhostActor->SetActorHiddenInGame(false);
	UpdateStructureGhostPortPreview(HoveredSurfaceGrid, StructureData, FootprintCellIds, RotationSteps);
	PublishStructureGhostPlacementDebug(
		HoveredSurfaceGrid,
		HoveredCell,
		GhostTransform,
		StructureData.ConstructionHeightOffset,
		!bHasStructureGhostCellId || !(StructureGhostCellId == HoveredCell.CellId));
	StructureGhostCellId = HoveredCell.CellId;
	bHasStructureGhostCellId = true;
}

void USRAssemblyComponent::DestroyStructureGhostPreview()
{
	ClearStructureGhostPortPreview();

	if (IsValid(StructureGhostActor))
	{
		StructureGhostActor->Destroy();
	}

	StructureGhostActor = nullptr;
	StructureGhostDataAsset = nullptr;
	StructureGhostCellId = FSRPlanetSurfaceGridCellId();
	bHasStructureGhostCellId = false;
}

void USRAssemblyComponent::DestroyStructurePlacementDragPreviewActors()
{
	for (FSRStructurePlacementDragPreviewActor& PreviewInfo : StructurePlacementDragPreviewActors)
	{
		if (AActor* PreviewActor = PreviewInfo.PreviewActor.Get())
		{
			PreviewActor->Destroy();
		}
	}

	StructurePlacementDragPreviewActors.Reset();
}

AActor* USRAssemblyComponent::SpawnStructurePlacementDragPreviewActor(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRStructureDataAsset* StructureDataAsset)
{
	UWorld* World = GetWorld();
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!IsValid(World) || !IsValid(PlayerController) || !IsValid(SurfaceGrid) || !IsValid(StructureDataAsset))
	{
		return nullptr;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		StructureActorClass = ASRStructure::StaticClass();
	}
	if (!IsValid(StructureActorClass)
		|| !StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		return nullptr;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = IsValid(SurfaceOwner) ? SurfaceOwner : Cast<AActor>(PlayerController);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;

	AActor* PreviewActor = World->SpawnActor<AActor>(StructureActorClass, FTransform::Identity, SpawnParameters);
	if (!IsValid(PreviewActor))
	{
		return nullptr;
	}

	if (IsValid(SurfaceOwner))
	{
		PreviewActor->SetOwner(SurfaceOwner);
		PreviewActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform);
	}

	ISRBuildableStructureInterface::Execute_ApplyStructureDataAsset(PreviewActor, StructureDataAsset);
	ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PreviewActor, true);
	PreviewActor->SetActorEnableCollision(false);
	PreviewActor->SetActorHiddenInGame(true);
	return PreviewActor;
}

bool USRAssemblyComponent::UpdateStructurePlacementDragPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& TargetCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* StructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(SurfaceGrid)
		|| !IsValid(StructureDataAsset)
		|| !bIsStructurePlacementDragActive
		|| !IsValid(StructurePlacementDragSurfaceGrid)
		|| SurfaceGrid != StructurePlacementDragSurfaceGrid)
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (StructureData.BuildKind != ESRStructureBuildKind::Structure)
	{
		return false;
	}

	if (bHasLastStructurePlacementDragCellId
		&& LastStructurePlacementDragSurfaceGrid == SurfaceGrid
		&& LastStructurePlacementDragCellId == TargetCell.CellId
		&& !StructurePlacementDragCellIds.IsEmpty())
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> CandidateCellIds;
	if (!BuildAreaSelectionCellIds(SurfaceGrid, StructurePlacementDragStartCellId, TargetCell.CellId, CandidateCellIds))
	{
		return false;
	}

	TMap<FSRPlanetSurfaceGridCellId, AActor*> ExistingPreviewActorsByCellId;
	for (const FSRStructurePlacementDragPreviewActor& PreviewInfo : StructurePlacementDragPreviewActors)
	{
		if (AActor* PreviewActor = PreviewInfo.PreviewActor.Get())
		{
			ExistingPreviewActorsByCellId.Add(PreviewInfo.CellId, PreviewActor);
		}
	}

	const int32 PlacementRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(StructurePlacementDragRotationSteps);
	const int32 FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacementRotationSteps);
	const int32 FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacementRotationSteps);

	TSet<FSRPlanetSurfaceGridCellId> ReservedFootprintCellIds;
	TArray<FSRPlanetSurfaceGridCellId> NewPlacementCellIds;
	TArray<FSRStructurePlacementDragPreviewActor> NewPreviewActors;
	NewPlacementCellIds.Reserve(CandidateCellIds.Num());
	NewPreviewActors.Reserve(CandidateCellIds.Num());

	for (const FSRPlanetSurfaceGridCellId& CandidateCellId : CandidateCellIds)
	{
		FSRPlanetSurfaceGridCellInfo CandidateCellInfo;
		TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
		if (!SurfaceGrid->GetCellInfoById(CandidateCellId, CandidateCellInfo)
			|| !CandidateCellInfo.bCanConstruct
			|| !SurfaceGrid->GetFootprintCellIds(CandidateCellId, FootprintCellsX, FootprintCellsY, FootprintCellIds)
			|| !SurfaceGrid->CanOccupyCells(FootprintCellIds))
		{
			continue;
		}

		bool bOverlapsReservedFootprint = false;
		for (const FSRPlanetSurfaceGridCellId& FootprintCellId : FootprintCellIds)
		{
			if (ReservedFootprintCellIds.Contains(FootprintCellId))
			{
				bOverlapsReservedFootprint = true;
				break;
			}
		}
		if (bOverlapsReservedFootprint)
		{
			continue;
		}

		AActor* PreviewActor = nullptr;
		if (AActor** ExistingPreviewActor = ExistingPreviewActorsByCellId.Find(CandidateCellId))
		{
			PreviewActor = *ExistingPreviewActor;
			ExistingPreviewActorsByCellId.Remove(CandidateCellId);
		}
		if (!IsValid(PreviewActor))
		{
			PreviewActor = SpawnStructurePlacementDragPreviewActor(SurfaceGrid, StructureDataAsset);
		}
		if (!IsValid(PreviewActor))
		{
			continue;
		}

		FTransform PreviewTransform;
		if (!BuildStructureGhostTransform(SurfaceGrid, CandidateCellId, StructureDataAsset, PreviewTransform))
		{
			PreviewActor->SetActorHiddenInGame(true);
			continue;
		}

		if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(PreviewActor, CandidateCellInfo))
		{
			PreviewActor->SetActorHiddenInGame(true);
			continue;
		}

		for (const FSRPlanetSurfaceGridCellId& FootprintCellId : FootprintCellIds)
		{
			ReservedFootprintCellIds.Add(FootprintCellId);
		}

		ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PreviewActor, true);
		PreviewActor->SetActorTransform(PreviewTransform);
		PreviewActor->SetActorEnableCollision(false);
		PreviewActor->SetActorHiddenInGame(false);

		FSRStructurePlacementDragPreviewActor PreviewInfo;
		PreviewInfo.CellId = CandidateCellId;
		PreviewInfo.PreviewActor = PreviewActor;
		NewPreviewActors.Add(PreviewInfo);
		NewPlacementCellIds.Add(CandidateCellId);
	}

	for (const TPair<FSRPlanetSurfaceGridCellId, AActor*>& RemovedPreviewPair : ExistingPreviewActorsByCellId)
	{
		if (IsValid(RemovedPreviewPair.Value))
		{
			RemovedPreviewPair.Value->Destroy();
		}
	}

	StructurePlacementDragCellIds = MoveTemp(NewPlacementCellIds);
	StructurePlacementDragPreviewActors = MoveTemp(NewPreviewActors);
	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	LastStructurePlacementDragSurfaceGrid = SurfaceGrid;
	LastStructurePlacementDragCellId = TargetCell.CellId;
	bHasLastStructurePlacementDragCellId = true;
	ClearStructureGhostPortPreview();
	return true;
}

void USRAssemblyComponent::UpdateStructureGhostPortPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRStructureData& StructureData,
	const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
	int32 PlacementRotationSteps)
{
	if (!IsValid(SurfaceGrid)
		|| StructureData.BuildKind != ESRStructureBuildKind::Structure
		|| FootprintCellIds.IsEmpty())
	{
		ClearStructureGhostPortPreview();
		return;
	}

	TArray<FSRPlanetSurfaceGridCellId> InputConnectionCellIds;
	TArray<FSRPlanetSurfaceGridCellId> OutputConnectionCellIds;
	const int32 SafeFootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacementRotationSteps);
	const int32 SafeFootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacementRotationSteps);

	for (const FSRStructurePortSpec& InputPort : StructureData.InputPorts)
	{
		const FSRStructurePortSpec RotatedInputPort = StarRovers::Structure::RotateStructurePortSpec(InputPort, StructureData, PlacementRotationSteps);
		AppendGhostPortPreviewCell(
			SurfaceGrid,
			FootprintCellIds,
			SafeFootprintCellsX,
			SafeFootprintCellsY,
			RotatedInputPort,
			InputConnectionCellIds);
	}

	for (const FSRStructurePortSpec& OutputPort : StructureData.OutputPorts)
	{
		const FSRStructurePortSpec RotatedOutputPort = StarRovers::Structure::RotateStructurePortSpec(OutputPort, StructureData, PlacementRotationSteps);
		AppendGhostPortPreviewCell(
			SurfaceGrid,
			FootprintCellIds,
			SafeFootprintCellsX,
			SafeFootprintCellsY,
			RotatedOutputPort,
			OutputConnectionCellIds);
	}

	if (InputConnectionCellIds.IsEmpty() && OutputConnectionCellIds.IsEmpty())
	{
		ClearStructureGhostPortPreview();
		return;
	}

	if (IsValid(StructureGhostPortPreviewSurfaceGrid) && StructureGhostPortPreviewSurfaceGrid != SurfaceGrid)
	{
		StructureGhostPortPreviewSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	SurfaceGrid->SetFacilityPortPreviewCells(InputConnectionCellIds, OutputConnectionCellIds);
	StructureGhostPortPreviewSurfaceGrid = SurfaceGrid;
	bHasStructureGhostPortPreview = true;
}

void USRAssemblyComponent::ClearStructureGhostPortPreview()
{
	if (bHasStructureGhostPortPreview && IsValid(StructureGhostPortPreviewSurfaceGrid))
	{
		StructureGhostPortPreviewSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	StructureGhostPortPreviewSurfaceGrid = nullptr;
	bHasStructureGhostPortPreview = false;
}

bool USRAssemblyComponent::BuildStructureGhostTransform(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	USRStructureDataAsset* StructureDataAsset,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset))
	{
		return false;
	}

	return USRStructurePlacementLibrary::BuildStructurePlacementTransform(
		SurfaceGrid,
		CellId,
		StructureDataAsset,
		OutTransform,
		GetStructurePlacementAdditionalYawDegrees());
}

void USRAssemblyComponent::PublishStructureGhostPlacementDebug(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& HoveredCell,
	const FTransform& GhostTransform,
	float StructureHeightOffset,
	bool bLogDebug) const
{
	(void)SurfaceGrid;
	(void)HoveredCell;
	(void)GhostTransform;
	(void)StructureHeightOffset;
	(void)bLogDebug;
}

void USRAssemblyComponent::LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason)
{
	if (!IsValid(StructureDataAsset) || LastLoggedInvalidGhostDataAsset == StructureDataAsset)
	{
		return;
	}

	LastLoggedInvalidGhostDataAsset = StructureDataAsset;
}
