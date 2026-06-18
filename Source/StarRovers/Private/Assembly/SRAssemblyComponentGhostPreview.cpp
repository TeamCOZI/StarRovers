#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructure.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

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

	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	if (!HoveredSurfaceGrid->GetFootprintCellIds(HoveredCell.CellId, StructureData.FootprintCellsX, StructureData.FootprintCellsY, FootprintCellIds)
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
	if (IsValid(StructureGhostActor))
	{
		StructureGhostActor->Destroy();
	}

	StructureGhostActor = nullptr;
	StructureGhostDataAsset = nullptr;
	StructureGhostCellId = FSRPlanetSurfaceGridCellId();
	bHasStructureGhostCellId = false;
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

	return USRStructurePlacementLibrary::BuildStructurePlacementTransform(SurfaceGrid, CellId, StructureDataAsset, OutTransform);
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