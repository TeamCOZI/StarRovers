#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructure.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool DoesStructureIntersectSelection(
		const FSRPlacedStructureInstance& PlacedStructure,
		const TSet<FSRPlanetSurfaceGridCellId>& SelectedCellIds)
	{
		for (const FSRPlanetSurfaceGridCellId& FootprintCellId : PlacedStructure.FootprintCellIds)
		{
			if (SelectedCellIds.Contains(FootprintCellId))
			{
				return true;
			}
		}

		return false;
	}

	bool IsCopySourceStructure(const FSRPlacedStructureInstance& PlacedStructure)
	{
		USRStructureDataAsset* StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
		if (PlacedStructure.bNaturalStructure || !IsValid(StructureDataAsset))
		{
			return false;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		return StructureData.BuildKind == ESRStructureBuildKind::Structure
			&& !StructureData.bIsResourceDeposit
			&& StructureData.bAvailableForConstruction;
	}

	bool IsReplaceableOccupant(
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		FName OccupantId)
	{
		if (!IsValid(StructureInstanceManager) || OccupantId.IsNone())
		{
			return false;
		}

		FSRPlacedStructureInstance PlacedStructure;
		if (!StructureInstanceManager->GetPlacedStructure(OccupantId, PlacedStructure)
			|| !IsValid(PlacedStructure.StructureDataAsset.Get()))
		{
			return false;
		}

		const FSRStructureData StructureData = PlacedStructure.StructureDataAsset->BuildData();
		return !StructureData.bIsResourceDeposit
			&& StructureData.bDestroyableByConstruction;
	}

	bool IsCopySourceConveyorPath(
		const FSRConveyorVisualPath& VisualPath,
		const FSRPlanetSurfaceGridCellId& SelectionCenterCellId)
	{
		USRStructureDataAsset* StructureDataAsset = VisualPath.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset) || VisualPath.CellIds.IsEmpty())
		{
			return false;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.BuildKind != ESRStructureBuildKind::Conveyor)
		{
			return false;
		}

		for (const FSRPlanetSurfaceGridCellId& CellId : VisualPath.CellIds)
		{
			if (CellId.Face != SelectionCenterCellId.Face)
			{
				return false;
			}
		}

		return true;
	}

	FName MakeAreaCopyConveyorNetworkId(AActor* SurfaceOwner, int32 Layer)
	{
		return FName(*FString::Printf(TEXT("ConveyorCopy_%s_%d"), *GetNameSafe(SurfaceOwner), FMath::Max(0, Layer)));
	}

	int32 MirrorPlacementRotationSteps(int32 PlacementRotationSteps, bool bMirrorLeftRight)
	{
		const int32 NormalizedSteps = StarRovers::Structure::NormalizePlacementRotationSteps(PlacementRotationSteps);
		return bMirrorLeftRight
			? StarRovers::Structure::NormalizePlacementRotationSteps(2 - NormalizedSteps)
			: StarRovers::Structure::NormalizePlacementRotationSteps(-NormalizedSteps);
	}
}

bool USRAssemblyComponent::IsAreaCopyPlacementActive() const
{
	return AreaCopy.IsPlacementActive();
}

bool USRAssemblyComponent::ResolveAreaSelectionCenterCellId(FSRPlanetSurfaceGridCellId& OutCenterCellId) const
{
	return AreaSelection.ResolveSelectionCenterCellId(OutCenterCellId);
}

bool USRAssemblyComponent::HasAreaCopyPayload() const
{
	return AreaCopy.HasPayload();
}

bool USRAssemblyComponent::TryBeginAreaSelectionCopyPlacement()
{
	USRPlanetSurfaceGrid* SelectionSurfaceGrid = AreaSelection.GetSelectionSurfaceGrid();
	const TArray<FSRPlanetSurfaceGridCellId>& SelectionCellIds = AreaSelection.GetSelectionCellIds();
	if (!bAssemblyModeActive || !IsValid(SelectionSurfaceGrid) || SelectionCellIds.IsEmpty())
	{
		return false;
	}

	FSRPlanetSurfaceGridCellId SelectionCenterCellId;
	if (!ResolveAreaSelectionCenterCellId(SelectionCenterCellId))
	{
		return false;
	}

	AActor* SurfaceOwner = SelectionSurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr;

	TSet<FSRPlanetSurfaceGridCellId> SelectedCellIds;
	SelectedCellIds.Reserve(SelectionCellIds.Num());
	for (const FSRPlanetSurfaceGridCellId& CellId : SelectionCellIds)
	{
		SelectedCellIds.Add(CellId);
	}

	TArray<FSRAreaCopiedStructure> NewCopiedStructures;
	if (IsValid(StructureInstanceManager))
	{
		TArray<FSRPlacedStructureInstance> PlacedStructures;
		StructureInstanceManager->GetPlacedStructures(PlacedStructures);

		for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
		{
			if (!IsCopySourceStructure(PlacedStructure) || !DoesStructureIntersectSelection(PlacedStructure, SelectedCellIds))
			{
				continue;
			}

			FSRAreaCopiedStructure CopiedStructure;
			CopiedStructure.StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
			CopiedStructure.AnchorOffset = FIntPoint(
				PlacedStructure.OriginCellId.CellX - SelectionCenterCellId.CellX,
				PlacedStructure.OriginCellId.CellY - SelectionCenterCellId.CellY);
			CopiedStructure.PlacementRotationSteps = PlacedStructure.PlacementRotationSteps;
			NewCopiedStructures.Add(CopiedStructure);
		}
	}

	TArray<FSRAreaCopiedConveyorPath> NewCopiedConveyorPaths;
	if (IsValid(ConveyorNetwork))
	{
		TArray<FSRConveyorVisualPath> SelectedConveyorVisualPaths;
		ConveyorNetwork->GetConveyorVisualPathsInCells(SelectedCellIds, SelectedConveyorVisualPaths);
		for (const FSRConveyorVisualPath& VisualPath : SelectedConveyorVisualPaths)
		{
			if (!IsCopySourceConveyorPath(VisualPath, SelectionCenterCellId))
			{
				continue;
			}

			FSRAreaCopiedConveyorPath CopiedConveyorPath;
			CopiedConveyorPath.StructureDataAsset = VisualPath.StructureDataAsset.Get();
			CopiedConveyorPath.Layer = FMath::Max(0, VisualPath.Layer);
			CopiedConveyorPath.LayerHeight = VisualPath.LayerHeight;
			CopiedConveyorPath.NetworkId = VisualPath.NetworkId;
			CopiedConveyorPath.AnchorOffsets.Reserve(VisualPath.CellIds.Num());
			for (const FSRPlanetSurfaceGridCellId& CellId : VisualPath.CellIds)
			{
				CopiedConveyorPath.AnchorOffsets.Add(FIntPoint(
					CellId.CellX - SelectionCenterCellId.CellX,
					CellId.CellY - SelectionCenterCellId.CellY));
			}
			NewCopiedConveyorPaths.Add(CopiedConveyorPath);
		}
	}

	if (NewCopiedStructures.IsEmpty() && NewCopiedConveyorPaths.IsEmpty())
	{
		return false;
	}

	EndStructurePlacementDrag(false);
	ClearAreaDeletion();
	ClearConveyorBulkDeletionPreview();
	ClearConveyorInvalidPlacementPreview();
	ClearConveyorPlacementPortPreview();
	ClearPendingConveyorPathStart();
	PlacementQueue.Reset();
	DestroyStructureGhostPreview();
	DestroyConveyorGhostPreview();
	DestroyConveyorDeletionGhostPreview();
	DestroyAreaCopyPreviewActors();

	AreaCopy.BeginPlacement(MoveTemp(NewCopiedStructures), MoveTemp(NewCopiedConveyorPaths));

	ClearAreaSelection();
	RebuildAreaCopyPreviewActors();
	UpdateAreaCopyPlacementPreview();
	return true;
}

bool USRAssemblyComponent::MirrorAreaCopyPlacement(bool bMirrorLeftRight)
{
	if (!AreaCopy.IsPlacementActive() || !HasAreaCopyPayload())
	{
		return false;
	}

	for (FSRAreaCopiedStructure& CopiedStructure : AreaCopy.CopiedStructures)
	{
		USRStructureDataAsset* StructureDataAsset = CopiedStructure.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset))
		{
			if (bMirrorLeftRight)
			{
				CopiedStructure.AnchorOffset.X = -CopiedStructure.AnchorOffset.X;
			}
			else
			{
				CopiedStructure.AnchorOffset.Y = -CopiedStructure.AnchorOffset.Y;
			}
			CopiedStructure.PlacementRotationSteps = MirrorPlacementRotationSteps(CopiedStructure.PlacementRotationSteps, bMirrorLeftRight);
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		const int32 CurrentRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(CopiedStructure.PlacementRotationSteps);
		const int32 FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, CurrentRotationSteps);
		const int32 FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, CurrentRotationSteps);
		if (bMirrorLeftRight)
		{
			CopiedStructure.AnchorOffset.X = -CopiedStructure.AnchorOffset.X - (FootprintCellsX - 1);
		}
		else
		{
			CopiedStructure.AnchorOffset.Y = -CopiedStructure.AnchorOffset.Y - (FootprintCellsY - 1);
		}
		CopiedStructure.PlacementRotationSteps = MirrorPlacementRotationSteps(CurrentRotationSteps, bMirrorLeftRight);
	}

	for (FSRAreaCopiedConveyorPath& CopiedConveyorPath : AreaCopy.CopiedConveyorPaths)
	{
		for (FIntPoint& AnchorOffset : CopiedConveyorPath.AnchorOffsets)
		{
			if (bMirrorLeftRight)
			{
				AnchorOffset.X = -AnchorOffset.X;
			}
			else
			{
				AnchorOffset.Y = -AnchorOffset.Y;
			}
		}
	}

	AreaCopy.ClearPreviewHoverCache();
	UpdateAreaCopyPlacementPreview();
	return true;
}

void USRAssemblyComponent::CancelAreaCopyPlacement()
{
	if (!AreaCopy.IsPlacementActive() && !HasAreaCopyPayload())
	{
		return;
	}

	DestroyAreaCopyPreviewActors();
	AreaCopy.Cancel();
}

void USRAssemblyComponent::DestroyAreaCopyPreviewActors()
{
	for (FSRAreaCopiedStructure& CopiedStructure : AreaCopy.CopiedStructures)
	{
		if (AActor* PreviewActor = CopiedStructure.PreviewActor.Get())
		{
			PreviewActor->Destroy();
		}
		CopiedStructure.PreviewActor = nullptr;
	}

	for (FSRAreaCopiedConveyorPath& CopiedConveyorPath : AreaCopy.CopiedConveyorPaths)
	{
		if (ASRConveyorBeltActor* PreviewActor = CopiedConveyorPath.PreviewActor.Get())
		{
			PreviewActor->Destroy();
		}
		CopiedConveyorPath.PreviewActor = nullptr;
	}
}

void USRAssemblyComponent::RebuildAreaCopyPreviewActors()
{
	DestroyAreaCopyPreviewActors();

	UWorld* World = GetWorld();
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!IsValid(World) || !IsValid(PlayerController))
	{
		return;
	}

	AActor* SurfaceOwner = IsValid(HoveredSurfaceGrid) ? HoveredSurfaceGrid->GetOwner() : nullptr;
	USRPlanetSurfaceGrid* SelectionSurfaceGrid = AreaSelection.GetSelectionSurfaceGrid();
	if (!IsValid(SurfaceOwner) && IsValid(SelectionSurfaceGrid))
	{
		SurfaceOwner = SelectionSurfaceGrid->GetOwner();
	}

	for (FSRAreaCopiedStructure& CopiedStructure : AreaCopy.CopiedStructures)
	{
		USRStructureDataAsset* StructureDataAsset = CopiedStructure.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset))
		{
			continue;
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
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = IsValid(SurfaceOwner) ? SurfaceOwner : Cast<AActor>(PlayerController);
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* PreviewActor = World->SpawnActor<AActor>(StructureActorClass, FTransform::Identity, SpawnParameters);
		if (!IsValid(PreviewActor))
		{
			continue;
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
		CopiedStructure.PreviewActor = PreviewActor;
	}

	for (FSRAreaCopiedConveyorPath& CopiedConveyorPath : AreaCopy.CopiedConveyorPaths)
	{
		USRStructureDataAsset* StructureDataAsset = CopiedConveyorPath.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset) || CopiedConveyorPath.AnchorOffsets.IsEmpty())
		{
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		UClass* ConveyorActorClass = StructureData.StructureActorClass.Get();
		if (!IsValid(ConveyorActorClass) || !ConveyorActorClass->IsChildOf(ASRConveyorBeltActor::StaticClass()))
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = SurfaceOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		ASRConveyorBeltActor* PreviewActor = World->SpawnActor<ASRConveyorBeltActor>(
			ConveyorActorClass,
			IsValid(SurfaceOwner) ? SurfaceOwner->GetActorTransform() : FTransform::Identity,
			SpawnParameters);
		if (!IsValid(PreviewActor))
		{
			continue;
		}

		if (IsValid(SurfaceOwner))
		{
			PreviewActor->SetOwner(SurfaceOwner);
			PreviewActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform);
		}
		PreviewActor->SetActorEnableCollision(false);
		PreviewActor->SetActorHiddenInGame(true);
		PreviewActor->SetConveyorGhostMode(true, ResolveAreaCopyPreviewMaterial(StructureDataAsset, AreaCopy.LastPreviewState));
		CopiedConveyorPath.PreviewActor = PreviewActor;
	}
}

void USRAssemblyComponent::UpdateAreaCopyPlacementPreview()
{
	if (!AreaCopy.IsPlacementActive() || !HasAreaCopyPayload())
	{
		return;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = HoveredSurfaceGrid.Get();
	FSRPlanetSurfaceGridCell HoveredCell;
	if (!IsValid(SurfaceGrid) || !SurfaceGrid->GetHoveredCell(HoveredCell))
	{
		return;
	}

	if (AreaCopy.HasCachedPreviewForHover(HoveredCell.CellId))
	{
		ApplyAreaCopyPreviewState(AreaCopy.LastPreviewState);
		TArray<FSRPlanetSurfaceGridCellId> TargetOriginCellIds;
		TargetOriginCellIds.SetNum(AreaCopy.CopiedStructures.Num());
		for (int32 CopyIndex = 0; CopyIndex < AreaCopy.CopiedStructures.Num(); ++CopyIndex)
		{
			const FSRAreaCopiedStructure& CopiedStructure = AreaCopy.CopiedStructures[CopyIndex];
			TargetOriginCellIds[CopyIndex].Face = HoveredCell.CellId.Face;
			TargetOriginCellIds[CopyIndex].CellX = HoveredCell.CellId.CellX + CopiedStructure.AnchorOffset.X;
			TargetOriginCellIds[CopyIndex].CellY = HoveredCell.CellId.CellY + CopiedStructure.AnchorOffset.Y;
		}
		UpdateAreaCopyStructurePreviewActors(SurfaceGrid, TargetOriginCellIds, AreaCopy.LastPreviewState);
		return;
	}

	FSRAreaCopyPlacementEvaluation Evaluation;
	if (!BuildAreaCopyPlacementEvaluation(SurfaceGrid, HoveredCell.CellId, Evaluation))
	{
		ApplyAreaCopyPreviewState(ESRAreaCopyPlacementPreviewState::Blocked);
		return;
	}

	AreaCopy.StorePreviewEvaluation(HoveredCell.CellId, Evaluation);
	ApplyAreaCopyPreviewState(Evaluation.PreviewState);
	UpdateAreaCopyStructurePreviewActors(SurfaceGrid, Evaluation.TargetOriginCellIds, Evaluation.PreviewState);

	USRConveyorNetworkComponent* ConveyorNetwork = SurfaceGrid->GetOwner()
		? SurfaceGrid->GetOwner()->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr;
	for (int32 CopyIndex = 0; CopyIndex < AreaCopy.CopiedConveyorPaths.Num(); ++CopyIndex)
	{
		if (!Evaluation.TargetConveyorVisualPaths.IsValidIndex(CopyIndex))
		{
			continue;
		}

		FSRAreaCopiedConveyorPath& CopiedConveyorPath = AreaCopy.CopiedConveyorPaths[CopyIndex];
		ASRConveyorBeltActor* PreviewActor = CopiedConveyorPath.PreviewActor.Get();
		if (!IsValid(PreviewActor) || !IsValid(ConveyorNetwork))
		{
			continue;
		}

		TArray<FSRConveyorVisualPath> PreviewVisualPaths;
		PreviewVisualPaths.Add(Evaluation.TargetConveyorVisualPaths[CopyIndex]);
		if (!PreviewActor->InitializeConveyorPaths(
			SurfaceGrid,
			PreviewVisualPaths,
			ConveyorNetwork->GetConveyorActorSplineComponentTag(),
			ConveyorNetwork->GetConveyorActorSurfaceOffset()))
		{
			PreviewActor->SetActorHiddenInGame(true);
			continue;
		}

		PreviewActor->SetConveyorGhostMode(
			true,
			ResolveAreaCopyPreviewMaterial(CopiedConveyorPath.StructureDataAsset.Get(), Evaluation.PreviewState));
		PreviewActor->SetActorHiddenInGame(PreviewActor->IsConveyorGhostGenerationPending());
	}
}

void USRAssemblyComponent::UpdateAreaCopyStructurePreviewActors(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& TargetOriginCellIds,
	ESRAreaCopyPlacementPreviewState PreviewState)
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	for (int32 CopyIndex = 0; CopyIndex < AreaCopy.CopiedStructures.Num(); ++CopyIndex)
	{
		if (!TargetOriginCellIds.IsValidIndex(CopyIndex))
		{
			continue;
		}

		FSRAreaCopiedStructure& CopiedStructure = AreaCopy.CopiedStructures[CopyIndex];
		AActor* PreviewActor = CopiedStructure.PreviewActor.Get();
		USRStructureDataAsset* StructureDataAsset = CopiedStructure.StructureDataAsset.Get();
		if (!IsValid(PreviewActor) || !IsValid(StructureDataAsset))
		{
			continue;
		}

		FTransform PreviewTransform;
		if (!USRStructurePlacementLibrary::BuildStructurePlacementTransform(
			SurfaceGrid,
			TargetOriginCellIds[CopyIndex],
			StructureDataAsset,
			PreviewTransform,
			StarRovers::Structure::PlacementRotationStepsToYawDegrees(CopiedStructure.PlacementRotationSteps)))
		{
			PreviewActor->SetActorHiddenInGame(true);
			continue;
		}

		if (PreviewActor->GetClass()->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
		{
			ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PreviewActor, true);
		}
		PreviewActor->SetActorTransform(PreviewTransform);
		PreviewActor->SetActorEnableCollision(false);
		ApplyAreaCopyPreviewMaterial(PreviewActor, ResolveAreaCopyPreviewMaterial(StructureDataAsset, PreviewState));
		PreviewActor->SetActorHiddenInGame(false);
	}
}

void USRAssemblyComponent::CollectReplaceableOccupiedCellIds(
	USRStructureInstanceManagerComponent* StructureInstanceManager,
	const TSet<FName>& OccupantIds,
	TSet<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	OutCellIds.Reset();
	if (!IsValid(StructureInstanceManager) || OccupantIds.IsEmpty())
	{
		return;
	}

	for (const FName OccupantId : OccupantIds)
	{
		FSRPlacedStructureInstance PlacedStructure;
		if (!StructureInstanceManager->GetPlacedStructure(OccupantId, PlacedStructure))
		{
			continue;
		}

		for (const FSRPlanetSurfaceGridCellId& CellId : PlacedStructure.FootprintCellIds)
		{
			OutCellIds.Add(CellId);
		}
	}
}

bool USRAssemblyComponent::TryBuildConnectedAreaCopyConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRConveyorNetworkComponent* ConveyorNetwork,
	const FSRConveyorVisualPath& TargetVisualPath,
	const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds,
	FSRConveyorVisualPath& OutVisualPath) const
{
	OutVisualPath = TargetVisualPath;
	if (!IsValid(SurfaceGrid)
		|| !IsValid(ConveyorNetwork)
		|| TargetVisualPath.CellIds.IsEmpty())
	{
		return false;
	}

	auto TryFindConnectableNeighbor = [SurfaceGrid, ConveyorNetwork, &IgnoredOccupiedCellIds, Layer = FMath::Max(0, TargetVisualPath.Layer)](
		const TArray<FSRPlanetSurfaceGridCellId>& CurrentPath,
		bool bAtStart,
		FSRPlanetSurfaceGridCellId& OutNeighborCellId)
	{
		OutNeighborCellId = FSRPlanetSurfaceGridCellId();
		if (CurrentPath.IsEmpty())
		{
			return false;
		}

		const FSRPlanetSurfaceGridCellId EndpointCellId = bAtStart ? CurrentPath[0] : CurrentPath.Last();
		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(EndpointCellId, Neighbors))
		{
			return false;
		}

		const FSRPlanetSurfaceGridCellId NeighborCellIds[] =
		{
			Neighbors.NegativeU,
			Neighbors.PositiveU,
			Neighbors.NegativeV,
			Neighbors.PositiveV,
		};

		for (const FSRPlanetSurfaceGridCellId& NeighborCellId : NeighborCellIds)
		{
			if (CurrentPath.Contains(NeighborCellId))
			{
				continue;
			}

			FSRConveyorLaneKey NeighborLaneKey;
			NeighborLaneKey.CellId = NeighborCellId;
			NeighborLaneKey.Layer = Layer;
			if (!ConveyorNetwork->HasConveyorSegment(NeighborLaneKey))
			{
				continue;
			}

			TArray<FSRPlanetSurfaceGridCellId> CandidatePath = CurrentPath;
			if (bAtStart)
			{
				CandidatePath.Insert(NeighborCellId, 0);
			}
			else
			{
				CandidatePath.Add(NeighborCellId);
			}

			if (ConveyorNetwork->CanPlaceConveyorPath(SurfaceGrid, CandidatePath, Layer, IgnoredOccupiedCellIds))
			{
				OutNeighborCellId = NeighborCellId;
				return true;
			}
		}

		return false;
	};

	TArray<FSRPlanetSurfaceGridCellId> ConnectedCellIds = TargetVisualPath.CellIds;
	FSRPlanetSurfaceGridCellId StartNeighborCellId;
	if (TryFindConnectableNeighbor(ConnectedCellIds, true, StartNeighborCellId))
	{
		ConnectedCellIds.Insert(StartNeighborCellId, 0);
	}

	FSRPlanetSurfaceGridCellId EndNeighborCellId;
	if (TryFindConnectableNeighbor(ConnectedCellIds, false, EndNeighborCellId))
	{
		ConnectedCellIds.Add(EndNeighborCellId);
	}

	OutVisualPath.CellIds = MoveTemp(ConnectedCellIds);
	return ConveyorNetwork->CanPlaceConveyorPath(
		SurfaceGrid,
		OutVisualPath.CellIds,
		OutVisualPath.Layer,
		IgnoredOccupiedCellIds);
}

bool USRAssemblyComponent::BuildAreaCopyPlacementEvaluation(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& HoverCellId,
	FSRAreaCopyPlacementEvaluation& OutEvaluation) const
{
	OutEvaluation = FSRAreaCopyPlacementEvaluation();
	OutEvaluation.TargetOriginCellIds.SetNum(AreaCopy.CopiedStructures.Num());
	OutEvaluation.TargetConveyorVisualPaths.SetNum(AreaCopy.CopiedConveyorPaths.Num());
	if (!IsValid(SurfaceGrid) || !HasAreaCopyPayload())
	{
		return false;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr;

	bool bBlocked = false;
	TSet<FSRPlanetSurfaceGridCellId> ProposedFootprintCellIds;
	if (!AreaCopy.CopiedStructures.IsEmpty() && !IsValid(StructureInstanceManager))
	{
		bBlocked = true;
	}

	for (int32 CopyIndex = 0; CopyIndex < AreaCopy.CopiedStructures.Num(); ++CopyIndex)
	{
		const FSRAreaCopiedStructure& CopiedStructure = AreaCopy.CopiedStructures[CopyIndex];
		USRStructureDataAsset* StructureDataAsset = CopiedStructure.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset))
		{
			bBlocked = true;
			continue;
		}

		FSRPlanetSurfaceGridCellId TargetOriginCellId;
		TargetOriginCellId.Face = HoverCellId.Face;
		TargetOriginCellId.CellX = HoverCellId.CellX + CopiedStructure.AnchorOffset.X;
		TargetOriginCellId.CellY = HoverCellId.CellY + CopiedStructure.AnchorOffset.Y;
		OutEvaluation.TargetOriginCellIds[CopyIndex] = TargetOriginCellId;

		FSRPlanetSurfaceGridCell TargetOriginCell;
		if (!SurfaceGrid->GetCellById(TargetOriginCellId, TargetOriginCell))
		{
			bBlocked = true;
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.BuildKind != ESRStructureBuildKind::Structure
			|| StructureData.bIsResourceDeposit
			|| !StructureData.bAvailableForConstruction)
		{
			bBlocked = true;
			continue;
		}

		TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
		if (!SurfaceGrid->GetFootprintCellIds(
			TargetOriginCellId,
			StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, CopiedStructure.PlacementRotationSteps),
			StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, CopiedStructure.PlacementRotationSteps),
			FootprintCellIds))
		{
			bBlocked = true;
			continue;
		}

		for (const FSRPlanetSurfaceGridCellId& FootprintCellId : FootprintCellIds)
		{
			if (ProposedFootprintCellIds.Contains(FootprintCellId))
			{
				bBlocked = true;
				continue;
			}
			ProposedFootprintCellIds.Add(FootprintCellId);

			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(FootprintCellId, CellInfo))
			{
				bBlocked = true;
				continue;
			}

			if (!CellInfo.bOccupied)
			{
				if (!CellInfo.bCanConstruct)
				{
					bBlocked = true;
				}
				continue;
			}

			if (CellInfo.OccupantId.IsNone()
				|| !IsReplaceableOccupant(StructureInstanceManager, CellInfo.OccupantId))
			{
				bBlocked = true;
				continue;
			}

			OutEvaluation.ReplaceableOccupantIds.Add(CellInfo.OccupantId);
		}
	}

	CollectReplaceableOccupiedCellIds(
		StructureInstanceManager,
		OutEvaluation.ReplaceableOccupantIds,
		OutEvaluation.ReplaceableOccupiedCellIds);

	if (!AreaCopy.CopiedConveyorPaths.IsEmpty() && !IsValid(ConveyorNetwork))
	{
		bBlocked = true;
	}

	for (int32 CopyIndex = 0; CopyIndex < AreaCopy.CopiedConveyorPaths.Num(); ++CopyIndex)
	{
		const FSRAreaCopiedConveyorPath& CopiedConveyorPath = AreaCopy.CopiedConveyorPaths[CopyIndex];
		USRStructureDataAsset* StructureDataAsset = CopiedConveyorPath.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset) || CopiedConveyorPath.AnchorOffsets.IsEmpty())
		{
			bBlocked = true;
			continue;
		}

		const FSRStructureData ConveyorData = StructureDataAsset->BuildData();
		if (ConveyorData.BuildKind != ESRStructureBuildKind::Conveyor)
		{
			bBlocked = true;
			continue;
		}

		FSRConveyorVisualPath TargetVisualPath;
		TargetVisualPath.Layer = FMath::Max(0, CopiedConveyorPath.Layer);
		TargetVisualPath.LayerHeight = CopiedConveyorPath.LayerHeight;
		TargetVisualPath.NetworkId = CopiedConveyorPath.NetworkId;
		TargetVisualPath.StructureDataAsset = StructureDataAsset;
		TargetVisualPath.CellIds.Reserve(CopiedConveyorPath.AnchorOffsets.Num());
		TSet<FSRPlanetSurfaceGridCellId> PathCellIds;
		for (const FIntPoint& AnchorOffset : CopiedConveyorPath.AnchorOffsets)
		{
			FSRPlanetSurfaceGridCellId TargetCellId;
			TargetCellId.Face = HoverCellId.Face;
			TargetCellId.CellX = HoverCellId.CellX + AnchorOffset.X;
			TargetCellId.CellY = HoverCellId.CellY + AnchorOffset.Y;

			FSRPlanetSurfaceGridCell TargetCell;
			if (!SurfaceGrid->GetCellById(TargetCellId, TargetCell))
			{
				bBlocked = true;
				continue;
			}

			if (TargetVisualPath.Layer == 0 && ProposedFootprintCellIds.Contains(TargetCellId))
			{
				bBlocked = true;
				continue;
			}

			FSRConveyorLaneKey TargetLaneKey;
			TargetLaneKey.CellId = TargetCellId;
			TargetLaneKey.Layer = TargetVisualPath.Layer;

			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(TargetCellId, CellInfo))
			{
				bBlocked = true;
				continue;
			}

			if (TargetVisualPath.Layer == 0
				&& CellInfo.bOccupied
				&& !CellInfo.OccupantId.IsNone()
				&& (!IsValid(ConveyorNetwork) || !ConveyorNetwork->HasConveyorSegment(TargetLaneKey))
				&& !OutEvaluation.ReplaceableOccupiedCellIds.Contains(TargetCellId))
			{
				if (!IsReplaceableOccupant(StructureInstanceManager, CellInfo.OccupantId))
				{
					bBlocked = true;
					continue;
				}

				OutEvaluation.ReplaceableOccupantIds.Add(CellInfo.OccupantId);
				TSet<FSRPlanetSurfaceGridCellId> UpdatedReplaceableOccupiedCellIds;
				CollectReplaceableOccupiedCellIds(StructureInstanceManager, OutEvaluation.ReplaceableOccupantIds, UpdatedReplaceableOccupiedCellIds);
				OutEvaluation.ReplaceableOccupiedCellIds = MoveTemp(UpdatedReplaceableOccupiedCellIds);
			}

			if (PathCellIds.Contains(TargetCellId))
			{
				continue;
			}

			PathCellIds.Add(TargetCellId);
			TargetVisualPath.CellIds.Add(TargetCellId);
		}

		if (TargetVisualPath.CellIds.IsEmpty())
		{
			bBlocked = true;
			continue;
		}

		FSRConveyorVisualPath ConnectedVisualPath;
		if (!TryBuildConnectedAreaCopyConveyorPath(
			SurfaceGrid,
			ConveyorNetwork,
			TargetVisualPath,
			OutEvaluation.ReplaceableOccupiedCellIds,
			ConnectedVisualPath))
		{
			bBlocked = true;
			OutEvaluation.TargetConveyorVisualPaths[CopyIndex] = TargetVisualPath;
			continue;
		}

		OutEvaluation.TargetConveyorVisualPaths[CopyIndex] = MoveTemp(ConnectedVisualPath);
	}

	if (bBlocked)
	{
		OutEvaluation.PreviewState = ESRAreaCopyPlacementPreviewState::Blocked;
		OutEvaluation.bCanPlace = false;
		return true;
	}

	OutEvaluation.PreviewState = OutEvaluation.ReplaceableOccupantIds.IsEmpty()
		? ESRAreaCopyPlacementPreviewState::Placeable
		: ESRAreaCopyPlacementPreviewState::Replaceable;
	OutEvaluation.bCanPlace = true;
	return true;
}

bool USRAssemblyComponent::TryCommitAreaCopyPlacement(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!AreaCopy.IsPlacementActive() || !HasAreaCopyPayload())
	{
		return false;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = HoveredSurfaceGrid.Get();
	FSRPlanetSurfaceGridCell HoveredCell;
	if (!IsValid(SurfaceGrid) || !SurfaceGrid->GetHoveredCell(HoveredCell))
	{
		return true;
	}

	FSRAreaCopyPlacementEvaluation Evaluation;
	if (!BuildAreaCopyPlacementEvaluation(SurfaceGrid, HoveredCell.CellId, Evaluation) || !Evaluation.bCanPlace)
	{
		ApplyAreaCopyPreviewState(ESRAreaCopyPlacementPreviewState::Blocked);
		return true;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr;
	if ((!AreaCopy.CopiedStructures.IsEmpty() && !IsValid(StructureInstanceManager))
		|| (!AreaCopy.CopiedConveyorPaths.IsEmpty() && !IsValid(ConveyorNetwork)))
	{
		return true;
	}

	SurfaceGrid->BeginInteractionHighlightBatch();
	if (!Evaluation.ReplaceableOccupantIds.IsEmpty() && IsValid(StructureInstanceManager))
	{
		StructureInstanceManager->RemoveNonResourceStructuresByOccupantIds(SurfaceGrid, Evaluation.ReplaceableOccupantIds);
	}

	bool bPlacedAny = false;
	TArray<FSRAssemblyPlacementHistoryEntry> AreaCopyHistoryEntries;
	for (int32 CopyIndex = 0; CopyIndex < AreaCopy.CopiedStructures.Num(); ++CopyIndex)
	{
		if (!Evaluation.TargetOriginCellIds.IsValidIndex(CopyIndex))
		{
			continue;
		}

		USRStructureDataAsset* StructureDataAsset = AreaCopy.CopiedStructures[CopyIndex].StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset))
		{
			continue;
		}

		FName NewOccupantId = NAME_None;
		const int32 PlacementRotationSteps = AreaCopy.CopiedStructures[CopyIndex].PlacementRotationSteps;
		if (StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(
			SurfaceGrid,
			Evaluation.TargetOriginCellIds[CopyIndex],
			StructureDataAsset,
			NewOccupantId,
			false,
			false,
			PlacementRotationSteps))
		{
			FSRAssemblyPlacementHistoryEntry HistoryEntry;
			HistoryEntry.Kind = ESRAssemblyPlacementHistoryKind::Structure;
			HistoryEntry.SurfaceGrid = SurfaceGrid;
			HistoryEntry.StructureInstanceManager = StructureInstanceManager;
			HistoryEntry.StructureDataAsset = StructureDataAsset;
			HistoryEntry.OriginCellId = Evaluation.TargetOriginCellIds[CopyIndex];
			HistoryEntry.PlacementRotationSteps = PlacementRotationSteps;
			HistoryEntry.OccupantId = NewOccupantId;
			AreaCopyHistoryEntries.Add(HistoryEntry);
			bPlacedAny = true;
		}
	}

	for (int32 CopyIndex = 0; CopyIndex < AreaCopy.CopiedConveyorPaths.Num(); ++CopyIndex)
	{
		if (!Evaluation.TargetConveyorVisualPaths.IsValidIndex(CopyIndex))
		{
			continue;
		}

		const FSRConveyorVisualPath& TargetVisualPath = Evaluation.TargetConveyorVisualPaths[CopyIndex];
		USRStructureDataAsset* StructureDataAsset = TargetVisualPath.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset) || TargetVisualPath.CellIds.IsEmpty())
		{
			continue;
		}

		const FSRStructureData ConveyorData = StructureDataAsset->BuildData();
		const FName NetworkId = MakeAreaCopyConveyorNetworkId(SurfaceOwner, TargetVisualPath.Layer);
		FSRConveyorVisualPath HistoryVisualPath;
		TArray<FSRPlanetSurfaceGridCellId> HistoryPlacedCellIds;
		TArray<FSRRestorableNaturalStructure> HistoryRemovedNaturalStructures;
		BuildConveyorPlacementHistoryPayload(
			SurfaceGrid,
			ConveyorNetwork,
			StructureDataAsset,
			TargetVisualPath.CellIds,
			TargetVisualPath.Layer,
			TargetVisualPath.LayerHeight > KINDA_SMALL_NUMBER ? TargetVisualPath.LayerHeight : ConveyorData.ConveyorLayerHeight,
			NetworkId,
			HistoryVisualPath,
			HistoryPlacedCellIds,
			HistoryRemovedNaturalStructures);
		if (ConveyorNetwork->TryPlaceConveyorPath(
			SurfaceGrid,
			TargetVisualPath.CellIds,
			TargetVisualPath.Layer,
			TargetVisualPath.LayerHeight > KINDA_SMALL_NUMBER ? TargetVisualPath.LayerHeight : ConveyorData.ConveyorLayerHeight,
			StructureDataAsset,
			NetworkId))
		{
			if (!HistoryPlacedCellIds.IsEmpty() && IsValid(HistoryVisualPath.StructureDataAsset.Get()))
			{
				FSRAssemblyPlacementHistoryEntry HistoryEntry;
				HistoryEntry.Kind = ESRAssemblyPlacementHistoryKind::Conveyor;
				HistoryEntry.SurfaceGrid = SurfaceGrid;
				HistoryEntry.ConveyorNetwork = ConveyorNetwork;
				HistoryEntry.StructureDataAsset = HistoryVisualPath.StructureDataAsset.Get();
				HistoryEntry.ConveyorVisualPath = HistoryVisualPath;
				HistoryEntry.ConveyorPlacedCellIds = HistoryPlacedCellIds;
				HistoryEntry.RemovedNaturalStructures = HistoryRemovedNaturalStructures;
				AreaCopyHistoryEntries.Add(HistoryEntry);
			}
			bPlacedAny = true;
		}
	}
	SurfaceGrid->EndInteractionHighlightBatch();

	if (!bPlacedAny)
	{
		return true;
	}

	RecordAssemblyPlacementHistoryBatch(SurfaceGrid, AreaCopyHistoryEntries);
	OutSelectedActor = SurfaceOwner;
	AreaCopy.ClearPreviewHoverCache();
	UpdateAreaCopyPlacementPreview();
	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	PublishHoveredCellInfo(SurfaceGrid, HoveredCell);
	return true;
}

void USRAssemblyComponent::ApplyAreaCopyPreviewState(ESRAreaCopyPlacementPreviewState PreviewState)
{
	AreaCopy.SetPreviewState(PreviewState);
	for (const FSRAreaCopiedStructure& CopiedStructure : AreaCopy.CopiedStructures)
	{
		AActor* PreviewActor = CopiedStructure.PreviewActor.Get();
		USRStructureDataAsset* StructureDataAsset = CopiedStructure.StructureDataAsset.Get();
		if (!IsValid(PreviewActor) || !IsValid(StructureDataAsset))
		{
			continue;
		}

		if (PreviewActor->GetClass()->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
		{
			ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PreviewActor, true);
		}
		PreviewActor->SetActorEnableCollision(false);
		ApplyAreaCopyPreviewMaterial(PreviewActor, ResolveAreaCopyPreviewMaterial(StructureDataAsset, PreviewState));
	}

	for (const FSRAreaCopiedConveyorPath& CopiedConveyorPath : AreaCopy.CopiedConveyorPaths)
	{
		ASRConveyorBeltActor* PreviewActor = CopiedConveyorPath.PreviewActor.Get();
		USRStructureDataAsset* StructureDataAsset = CopiedConveyorPath.StructureDataAsset.Get();
		if (!IsValid(PreviewActor) || !IsValid(StructureDataAsset))
		{
			continue;
		}

		PreviewActor->SetConveyorGhostMode(true, ResolveAreaCopyPreviewMaterial(StructureDataAsset, PreviewState));
	}
}

UMaterialInterface* USRAssemblyComponent::ResolveAreaCopyPreviewMaterial(
	USRStructureDataAsset* StructureDataAsset,
	ESRAreaCopyPlacementPreviewState PreviewState) const
{
	if (!IsValid(StructureDataAsset))
	{
		return nullptr;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	switch (PreviewState)
	{
	case ESRAreaCopyPlacementPreviewState::Placeable:
		return IsValid(StructureData.CopyPlaceableMaterial.Get())
			? StructureData.CopyPlaceableMaterial.Get()
			: IsValid(StructureData.GhostMaterial.Get())
			? StructureData.GhostMaterial.Get()
			: StructureData.Material.Get();
	case ESRAreaCopyPlacementPreviewState::Replaceable:
		return IsValid(StructureData.CopyReplaceableMaterial.Get())
			? StructureData.CopyReplaceableMaterial.Get()
			: IsValid(StructureData.DeleteMaterial.Get())
			? StructureData.DeleteMaterial.Get()
			: IsValid(StructureData.GhostMaterial.Get())
			? StructureData.GhostMaterial.Get()
			: StructureData.Material.Get();
	case ESRAreaCopyPlacementPreviewState::Blocked:
	default:
		return IsValid(StructureData.CopyBlockedMaterial.Get())
			? StructureData.CopyBlockedMaterial.Get()
			: IsValid(StructureData.DeleteMaterial.Get())
			? StructureData.DeleteMaterial.Get()
			: IsValid(StructureData.GhostMaterial.Get())
			? StructureData.GhostMaterial.Get()
			: StructureData.Material.Get();
	}
}

void USRAssemblyComponent::ApplyAreaCopyPreviewMaterial(AActor* PreviewActor, UMaterialInterface* Material) const
{
	if (!IsValid(PreviewActor))
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	PreviewActor->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		MeshComponent->SetVisibility(true, true);
		MeshComponent->SetHiddenInGame(false, true);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetRenderCustomDepth(true);

		if (!IsValid(Material))
		{
			continue;
		}

		const int32 MaterialSlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
		{
			MeshComponent->SetMaterial(MaterialIndex, Material);
		}
	}
}
