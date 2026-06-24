#include "Assembly/SRAssemblyComponent.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void USRAssemblyComponent::BuildCandidateConveyorLayers(TArray<int32>& OutLayers) const
{
	OutLayers.Reset();
	if (const ASRPlayerController* PlayerController = GetOwnerController())
	{
		if (USRStructureDataAsset* SelectedStructureDataAsset = PlayerController->GetSelectedStructureDataAsset())
		{
			const FSRStructureData SelectedStructureData = SelectedStructureDataAsset->BuildData();
			if (SelectedStructureData.BuildKind == ESRStructureBuildKind::Conveyor)
			{
				OutLayers.Add(FMath::Max(0, SelectedStructureData.ConveyorLayer));
			}
		}
	}
	OutLayers.AddUnique(0);
}

bool USRAssemblyComponent::TryDeleteStructureAtCell(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId)
{
	if (!IsValid(FocusedActor) || !IsValid(SurfaceGrid))
	{
		return false;
	}

	USRConveyorNetworkComponent* ConveyorNetwork = FocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	if (IsValid(ConveyorNetwork))
	{
		TArray<int32> CandidateConveyorLayers;
		BuildCandidateConveyorLayers(CandidateConveyorLayers);

		for (const int32 CandidateLayer : CandidateConveyorLayers)
		{
			if (ConveyorNetwork->TryRemoveConveyorAtCell(SurfaceGrid, TargetCellId, CandidateLayer))
			{
				return true;
			}
		}
	}

	FSRPlanetSurfaceGridCellInfo TargetCellInfo;
	if (!SurfaceGrid->GetCellInfoById(TargetCellId, TargetCellInfo) || !TargetCellInfo.bOccupied || TargetCellInfo.OccupantId.IsNone())
	{
		return false;
	}

	if (USRStructureInstanceManagerComponent* StructureInstanceManager = FocusedActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
	{
		FSRPlacedStructureInstance PlacedStructure;
		if (StructureInstanceManager->GetPlacedStructure(TargetCellInfo.OccupantId, PlacedStructure)
			&& PlacedStructure.bNaturalStructure)
		{
			return false;
		}

		if (StructureInstanceManager->TryRemoveStructureAtCell(SurfaceGrid, TargetCellId))
		{
			return true;
		}
	}

	TArray<FSRPlanetSurfaceGridCellId> OccupantCellIds;
	for (const FSRPlanetSurfaceGridCell& Cell : SurfaceGrid->GetCells())
	{
		if (Cell.bOccupied && Cell.OccupantId == TargetCellInfo.OccupantId)
		{
			OccupantCellIds.Add(Cell.CellId);
		}
	}

	if (OccupantCellIds.IsEmpty())
	{
		OccupantCellIds.Add(TargetCellId);
	}

	TryDestroyAttachedOccupantActor(FocusedActor, TargetCellInfo.OccupantId);
	if (USRFacilityNetworkComponent* FacilityNetwork = FocusedActor->FindComponentByClass<USRFacilityNetworkComponent>())
	{
		FacilityNetwork->UnregisterFacility(TargetCellInfo.OccupantId);
	}
	return SurfaceGrid->SetCellsOccupied(OccupantCellIds, false, NAME_None);
}

bool USRAssemblyComponent::TryDestroyAttachedOccupantActor(AActor* SurfaceOwner, FName OccupantId) const
{
	if (!IsValid(SurfaceOwner) || OccupantId.IsNone())
	{
		return false;
	}

	TArray<AActor*> AttachedActors;
	SurfaceOwner->GetAttachedActors(AttachedActors);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (IsValid(AttachedActor) && AttachedActor->GetFName() == OccupantId)
		{
			AttachedActor->Destroy();
			return true;
		}
	}

	return false;
}

bool USRAssemblyComponent::TryDeleteConnectedConveyorsAtCell(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId)
{
	if (!IsValid(FocusedActor) || !IsValid(SurfaceGrid))
	{
		return false;
	}

	USRConveyorNetworkComponent* ConveyorNetwork = FocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	if (!IsValid(ConveyorNetwork))
	{
		return false;
	}

	TArray<int32> CandidateConveyorLayers;
	BuildCandidateConveyorLayers(CandidateConveyorLayers);
	for (const int32 CandidateLayer : CandidateConveyorLayers)
	{
		if (ConveyorNetwork->TryRemoveConnectedConveyorsAtCell(SurfaceGrid, TargetCellId, CandidateLayer))
		{
			return true;
		}
	}

	return false;
}

bool USRAssemblyComponent::UpdateConveyorBulkDeletionPreview()
{
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!bAssemblyModeActive
		|| !IsValid(PlayerController)
		|| !PlayerController->IsConveyorBulkDeleteModifierActive()
		|| PlayerController->IsPointerOverBlockingUi())
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, HoveredCell, HoverHitLocation))
	{
		return false;
	}

	USRConveyorNetworkComponent* ConveyorNetwork = FocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	if (!IsValid(ConveyorNetwork))
	{
		return false;
	}

	TArray<int32> CandidateConveyorLayers;
	BuildCandidateConveyorLayers(CandidateConveyorLayers);
	for (const int32 CandidateLayer : CandidateConveyorLayers)
	{
		TArray<FSRPlanetSurfaceGridCellId> ConnectedCellIds;
		if (!ConveyorNetwork->GetConnectedConveyorCellIdsAtCell(SurfaceGrid, HoveredCell.CellId, CandidateLayer, ConnectedCellIds))
		{
			continue;
		}

		TArray<FSRConveyorVisualPath> ConnectedVisualPaths;
		ConveyorNetwork->GetConnectedConveyorVisualPathsAtCell(SurfaceGrid, HoveredCell.CellId, CandidateLayer, ConnectedVisualPaths);
		if (IsValid(ConveyorBulkDeletionPreviewSurfaceGrid) && ConveyorBulkDeletionPreviewSurfaceGrid != SurfaceGrid)
		{
			ConveyorBulkDeletionPreviewSurfaceGrid->ClearDeletionPreviewCells();
		}
		SurfaceGrid->SetDeletionPreviewCells(ConnectedCellIds);
		ConveyorBulkDeletionPreviewSurfaceGrid = SurfaceGrid;
		bHasConveyorBulkDeletionPreview = true;
		UpdateConveyorDeletionGhostPreview(SurfaceGrid, ConveyorNetwork, HoveredCell.CellId, CandidateLayer, ConnectedVisualPaths);
		return true;
	}

	return false;
}

void USRAssemblyComponent::ClearConveyorBulkDeletionPreview()
{
	if (bHasConveyorBulkDeletionPreview && IsValid(ConveyorBulkDeletionPreviewSurfaceGrid))
	{
		ConveyorBulkDeletionPreviewSurfaceGrid->ClearDeletionPreviewCells();
	}

	ConveyorBulkDeletionPreviewSurfaceGrid = nullptr;
	bHasConveyorBulkDeletionPreview = false;
	DestroyConveyorDeletionGhostPreview();
}

bool USRAssemblyComponent::UpdateConveyorDeletionGhostPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRConveyorNetworkComponent* ConveyorNetwork,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	int32 Layer,
	const TArray<FSRConveyorVisualPath>& VisualPaths)
{
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorNetwork) || VisualPaths.IsEmpty())
	{
		DestroyConveyorDeletionGhostPreview();
		return false;
	}

	USRStructureDataAsset* ConveyorDataAsset = VisualPaths[0].StructureDataAsset.Get();
	if (!IsValid(ConveyorDataAsset))
	{
		DestroyConveyorDeletionGhostPreview();
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	UClass* ConveyorActorClass = ConveyorData.StructureActorClass.Get();
	if (!IsValid(ConveyorActorClass) || !ConveyorActorClass->IsChildOf(ASRConveyorBeltActor::StaticClass()))
	{
		DestroyConveyorDeletionGhostPreview();
		return false;
	}

	if (IsValid(ConveyorDeletionGhostActor)
		&& ConveyorDeletionGhostDataAsset == ConveyorDataAsset
		&& ConveyorDeletionGhostSurfaceGrid == SurfaceGrid
		&& bHasConveyorDeletionGhostTargetCell
		&& ConveyorDeletionGhostTargetCellId == TargetCellId
		&& ConveyorDeletionGhostLayer == FMath::Max(0, Layer))
	{
		return true;
	}

	const bool bNeedsNewGhostActor = !IsValid(ConveyorDeletionGhostActor)
		|| ConveyorDeletionGhostDataAsset != ConveyorDataAsset
		|| ConveyorDeletionGhostSurfaceGrid != SurfaceGrid
		|| ConveyorDeletionGhostActor->GetClass() != ConveyorActorClass;
	if (bNeedsNewGhostActor)
	{
		DestroyConveyorDeletionGhostPreview();

		AActor* SurfaceOwner = SurfaceGrid->GetOwner();
		UWorld* World = IsValid(SurfaceOwner) ? SurfaceOwner->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = SurfaceOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		ConveyorDeletionGhostActor = World->SpawnActor<ASRConveyorBeltActor>(
			ConveyorActorClass,
			SurfaceOwner->GetActorTransform(),
			SpawnParameters);
		if (!IsValid(ConveyorDeletionGhostActor))
		{
			return false;
		}

		ConveyorDeletionGhostActor->SetOwner(SurfaceOwner);
		ConveyorDeletionGhostActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform);
		ConveyorDeletionGhostActor->SetActorHiddenInGame(false);
		ConveyorDeletionGhostDataAsset = ConveyorDataAsset;
		ConveyorDeletionGhostSurfaceGrid = SurfaceGrid;
	}

	if (!ConveyorDeletionGhostActor->InitializeConveyorPaths(
		SurfaceGrid,
		VisualPaths,
		ConveyorNetwork->GetConveyorActorSplineComponentTag(),
		ConveyorNetwork->GetConveyorActorSurfaceOffset()))
	{
		DestroyConveyorDeletionGhostPreview();
		return false;
	}

	ConveyorDeletionGhostActor->SetConveyorGhostMode(true, ConveyorData.GhostMaterial);
	ConveyorDeletionGhostActor->SetActorHiddenInGame(false);
	ConveyorDeletionGhostTargetCellId = TargetCellId;
	bHasConveyorDeletionGhostTargetCell = true;
	ConveyorDeletionGhostLayer = FMath::Max(0, Layer);
	return true;
}

void USRAssemblyComponent::DestroyConveyorDeletionGhostPreview()
{
	if (IsValid(ConveyorDeletionGhostActor))
	{
		ConveyorDeletionGhostActor->Destroy();
	}

	ConveyorDeletionGhostActor = nullptr;
	ConveyorDeletionGhostDataAsset = nullptr;
	ConveyorDeletionGhostSurfaceGrid = nullptr;
	ConveyorDeletionGhostTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasConveyorDeletionGhostTargetCell = false;
	ConveyorDeletionGhostLayer = 0;
}
