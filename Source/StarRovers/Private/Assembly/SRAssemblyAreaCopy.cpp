#include "Assembly/SRAssemblyAreaCopy.h"

#include "Assembly/SRAssemblyConstructionReplacement.h"
#include "Assembly/SRAssemblyPlacementRestoration.h"
#include "Assembly/SRAssemblyPreviewState.h"
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

	bool IsCopySourceConveyorPath(
		const FSRConveyorBeltPath& BeltPath,
		const FSRPlanetSurfaceGridCellId& SelectionCenterCellId)
	{
		USRStructureDataAsset* StructureDataAsset = BeltPath.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset) || BeltPath.CellIds.IsEmpty())
		{
			return false;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.BuildKind != ESRStructureBuildKind::Conveyor)
		{
			return false;
		}

		for (const FSRPlanetSurfaceGridCellId& CellId : BeltPath.CellIds)
		{
			if (CellId.Face != SelectionCenterCellId.Face)
			{
				return false;
			}
		}

		return true;
	}

	bool IsReplaceableOccupant(
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		FName OccupantId)
	{
		return IsValid(StructureInstanceManager)
			&& StructureInstanceManager->CanDestroyStructureForConstruction(OccupantId);
	}

	void CollectReplaceableOccupiedCellIds(
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		const TSet<FName>& OccupantIds,
		TSet<FSRPlanetSurfaceGridCellId>& OutCellIds)
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

	bool TryBuildConnectedAreaCopyConveyorPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRConveyorBeltPath& TargetBeltPath,
		const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds,
		FSRConveyorBeltPath& OutBeltPath)
	{
		OutBeltPath = TargetBeltPath;
		if (!IsValid(SurfaceGrid)
			|| !IsValid(ConveyorNetwork)
			|| TargetBeltPath.CellIds.IsEmpty())
		{
			return false;
		}

		auto TryFindConnectableNeighbor = [SurfaceGrid, ConveyorNetwork, &IgnoredOccupiedCellIds, Layer = FMath::Max(0, TargetBeltPath.Layer)](
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

		TArray<FSRPlanetSurfaceGridCellId> ConnectedCellIds = TargetBeltPath.CellIds;
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

		OutBeltPath.CellIds = MoveTemp(ConnectedCellIds);
		return ConveyorNetwork->CanPlaceConveyorPath(
			SurfaceGrid,
			OutBeltPath.CellIds,
			OutBeltPath.Layer,
			IgnoredOccupiedCellIds);
	}

	int32 MirrorPlacementRotationSteps(int32 PlacementRotationSteps, bool bMirrorLeftRight)
	{
		const int32 NormalizedSteps = StarRovers::Structure::NormalizePlacementRotationSteps(PlacementRotationSteps);
		return bMirrorLeftRight
			? StarRovers::Structure::NormalizePlacementRotationSteps(2 - NormalizedSteps)
			: StarRovers::Structure::NormalizePlacementRotationSteps(-NormalizedSteps);
	}

	FIntPoint RotateCellOffsetClockwise(const FIntPoint& Offset)
	{
		return FIntPoint(-Offset.Y, Offset.X);
	}

	FIntPoint RotateStructureAnchorOffsetClockwise(
		const FIntPoint& AnchorOffset,
		int32 FootprintCellsX,
		int32 FootprintCellsY)
	{
		return FIntPoint(
			-AnchorOffset.Y - (FMath::Max(1, FootprintCellsY) - 1),
			AnchorOffset.X);
	}

	FName MakeAreaCopyConveyorNetworkId(AActor* SurfaceOwner, int32 Layer)
	{
		return FName(*FString::Printf(TEXT("ConveyorCopy_%s_%d"), *GetNameSafe(SurfaceOwner), FMath::Max(0, Layer)));
	}
}

namespace StarRovers::Assembly
{
	bool FSRAssemblyAreaCopy::IsPlacementActive() const
	{
		return bIsPlacementActive;
	}

	bool FSRAssemblyAreaCopy::HasPayload() const
	{
		return !CopiedStructures.IsEmpty() || !CopiedConveyorPaths.IsEmpty();
	}

	bool FSRAssemblyAreaCopy::HasCachedPreviewForHover(const FSRPlanetSurfaceGridCellId& HoverCellId) const
	{
		return bHasLastPreviewHoverCell && LastPreviewHoverCellId == HoverCellId;
	}

	void FSRAssemblyAreaCopy::BeginPlacement(
		TArray<FSRAssemblyAreaCopiedStructure>&& NewCopiedStructures,
		TArray<FSRAssemblyAreaCopiedConveyorPath>&& NewCopiedConveyorPaths)
	{
		CopiedStructures = MoveTemp(NewCopiedStructures);
		CopiedConveyorPaths = MoveTemp(NewCopiedConveyorPaths);
		bIsPlacementActive = true;
		ResetPreviewCache();
	}

	bool FSRAssemblyAreaCopy::BuildPlacementPayloadFromSelection(
		const TArray<FSRPlanetSurfaceGridCellId>& SelectionCellIds,
		const FSRPlanetSurfaceGridCellId& SelectionCenterCellId,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		USRConveyorNetworkComponent* ConveyorNetwork,
		TArray<FSRAssemblyAreaCopiedStructure>& OutCopiedStructures,
		TArray<FSRAssemblyAreaCopiedConveyorPath>& OutCopiedConveyorPaths)
	{
		OutCopiedStructures.Reset();
		OutCopiedConveyorPaths.Reset();
		if (SelectionCellIds.IsEmpty())
		{
			return false;
		}

		TSet<FSRPlanetSurfaceGridCellId> SelectedCellIds;
		SelectedCellIds.Reserve(SelectionCellIds.Num());
		for (const FSRPlanetSurfaceGridCellId& CellId : SelectionCellIds)
		{
			SelectedCellIds.Add(CellId);
		}

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

				FSRAssemblyAreaCopiedStructure CopiedStructure;
				CopiedStructure.StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
				CopiedStructure.AnchorOffset = FIntPoint(
					PlacedStructure.OriginCellId.CellX - SelectionCenterCellId.CellX,
					PlacedStructure.OriginCellId.CellY - SelectionCenterCellId.CellY);
				CopiedStructure.PlacementRotationSteps = PlacedStructure.PlacementRotationSteps;
				OutCopiedStructures.Add(CopiedStructure);
			}
		}

		if (IsValid(ConveyorNetwork))
		{
			TArray<FSRConveyorBeltPath> SelectedConveyorBeltPaths;
			ConveyorNetwork->GetConveyorBeltPathsInCells(SelectedCellIds, SelectedConveyorBeltPaths);
			for (const FSRConveyorBeltPath& BeltPath : SelectedConveyorBeltPaths)
			{
				if (!IsCopySourceConveyorPath(BeltPath, SelectionCenterCellId))
				{
					continue;
				}

				FSRAssemblyAreaCopiedConveyorPath CopiedConveyorPath;
				CopiedConveyorPath.StructureDataAsset = BeltPath.StructureDataAsset.Get();
				CopiedConveyorPath.Layer = FMath::Max(0, BeltPath.Layer);
				CopiedConveyorPath.LayerHeight = BeltPath.LayerHeight;
				CopiedConveyorPath.NetworkId = BeltPath.NetworkId;
				CopiedConveyorPath.AnchorOffsets.Reserve(BeltPath.CellIds.Num());
				for (const FSRPlanetSurfaceGridCellId& CellId : BeltPath.CellIds)
				{
					CopiedConveyorPath.AnchorOffsets.Add(FIntPoint(
						CellId.CellX - SelectionCenterCellId.CellX,
						CellId.CellY - SelectionCenterCellId.CellY));
				}
				OutCopiedConveyorPaths.Add(CopiedConveyorPath);
			}
		}

		return !OutCopiedStructures.IsEmpty() || !OutCopiedConveyorPaths.IsEmpty();
	}

	void FSRAssemblyAreaCopy::Cancel()
	{
		bIsPlacementActive = false;
		ResetPreviewCache();
		CopiedStructures.Reset();
		CopiedConveyorPaths.Reset();
	}

	void FSRAssemblyAreaCopy::ResetPreviewCache()
	{
		ClearPreviewHoverCache();
		LastPreviewState = ESRAssemblyAreaCopyPlacementPreviewState::Blocked;
		LastPreviewHoverCellId = FSRPlanetSurfaceGridCellId();
	}

	void FSRAssemblyAreaCopy::ClearPreviewHoverCache()
	{
		bHasLastPreviewHoverCell = false;
		LastReplaceableOccupantIds.Reset();
		LastReplaceableOccupiedCellIds.Reset();
		LastReplaceableConveyorCellIds.Reset();
	}

	void FSRAssemblyAreaCopy::StorePreviewEvaluation(
		const FSRPlanetSurfaceGridCellId& HoverCellId,
		const FSRAssemblyAreaCopyPlacementEvaluation& Evaluation)
	{
		LastPreviewHoverCellId = HoverCellId;
		bHasLastPreviewHoverCell = true;
		LastPreviewState = Evaluation.PreviewState;
		LastReplaceableOccupantIds = Evaluation.ReplaceableOccupantIds;
		LastReplaceableOccupiedCellIds = Evaluation.ReplaceableOccupiedCellIds;
		LastReplaceableConveyorCellIds = Evaluation.ReplaceableConveyorCellIds;
	}

	void FSRAssemblyAreaCopy::SetPreviewState(ESRAssemblyAreaCopyPlacementPreviewState PreviewState)
	{
		LastPreviewState = PreviewState;
	}

	void FSRAssemblyAreaCopy::DestroyPreviewActors(USRPlanetSurfaceGrid* HoveredSurfaceGrid)
	{
		if (IsValid(HoveredSurfaceGrid))
		{
			HoveredSurfaceGrid->ClearConstructionReplacementPreviewCells();
			if (AActor* SurfaceOwner = HoveredSurfaceGrid->GetOwner())
			{
				if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
				{
					StructureInstanceManager->ClearDeletePreviewedStructures();
				}
			}
		}

		for (FSRAssemblyAreaCopiedStructure& CopiedStructure : CopiedStructures)
		{
			if (AActor* PreviewActor = CopiedStructure.PreviewActor.Get())
			{
				PreviewActor->Destroy();
			}
			CopiedStructure.PreviewActor = nullptr;
		}

		for (FSRAssemblyAreaCopiedConveyorPath& CopiedConveyorPath : CopiedConveyorPaths)
		{
			if (ASRConveyorBeltActor* PreviewActor = CopiedConveyorPath.PreviewActor.Get())
			{
				PreviewActor->Destroy();
			}
			CopiedConveyorPath.PreviewActor = nullptr;
		}
	}

	void FSRAssemblyAreaCopy::RebuildPreviewActors(
		UWorld* World,
		AActor* SurfaceOwner,
		AActor* FallbackOwner,
		USRPlanetSurfaceGrid* HoveredSurfaceGrid)
	{
		DestroyPreviewActors(HoveredSurfaceGrid);
		if (!IsValid(World) || (!IsValid(SurfaceOwner) && !IsValid(FallbackOwner)))
		{
			return;
		}

		for (FSRAssemblyAreaCopiedStructure& CopiedStructure : CopiedStructures)
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
			SpawnParameters.Owner = IsValid(SurfaceOwner) ? SurfaceOwner : FallbackOwner;
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

		for (FSRAssemblyAreaCopiedConveyorPath& CopiedConveyorPath : CopiedConveyorPaths)
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
			PreviewActor->SetConveyorGhostMode(true, ResolvePreviewMaterial(StructureDataAsset, LastPreviewState));
			CopiedConveyorPath.PreviewActor = PreviewActor;
		}
	}

	void FSRAssemblyAreaCopy::ApplyPreviewState(ESRAssemblyAreaCopyPlacementPreviewState PreviewState)
	{
		SetPreviewState(PreviewState);
		for (const FSRAssemblyAreaCopiedStructure& CopiedStructure : CopiedStructures)
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
			ApplyPreviewMaterial(PreviewActor, ResolvePreviewMaterial(StructureDataAsset, PreviewState));
		}

		for (const FSRAssemblyAreaCopiedConveyorPath& CopiedConveyorPath : CopiedConveyorPaths)
		{
			ASRConveyorBeltActor* PreviewActor = CopiedConveyorPath.PreviewActor.Get();
			USRStructureDataAsset* StructureDataAsset = CopiedConveyorPath.StructureDataAsset.Get();
			if (!IsValid(PreviewActor) || !IsValid(StructureDataAsset))
			{
				continue;
			}

			PreviewActor->SetConveyorGhostMode(true, ResolvePreviewMaterial(StructureDataAsset, PreviewState));
		}
	}

	bool FSRAssemblyAreaCopy::UpdatePlacementPreview(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& HoverCellId,
		FSRAssemblyConveyorPreviewState& ConveyorPreview)
	{
		if (!IsPlacementActive() || !HasPayload() || !IsValid(SurfaceGrid))
		{
			return false;
		}

		AActor* SurfaceOwner = SurfaceGrid->GetOwner();
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		if (HasCachedPreviewForHover(HoverCellId))
		{
			ApplyPreviewState(LastPreviewState);
			if (IsValid(StructureInstanceManager))
			{
				StructureInstanceManager->SetConstructionReplacementPreviewedStructures(LastReplaceableOccupantIds);
			}
			SurfaceGrid->SetConstructionReplacementPreviewCells(LastReplaceableOccupiedCellIds.Array());
			USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
				? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
				: nullptr;
			TArray<FSRConveyorBeltPath> ReplaceableConveyorBeltPaths;
			ConstructionReplacement::CollectGroundConveyorBeltPaths(
				ConveyorNetwork,
				LastReplaceableConveyorCellIds,
				ReplaceableConveyorBeltPaths);
			ConstructionReplacement::ApplyConveyorReplacementPreview(
				SurfaceGrid,
				ConveyorNetwork,
				ConveyorPreview,
				LastReplaceableConveyorCellIds,
				ReplaceableConveyorBeltPaths,
				HoverCellId);

			TArray<FSRPlanetSurfaceGridCellId> TargetOriginCellIds;
			TargetOriginCellIds.SetNum(CopiedStructures.Num());
			for (int32 CopyIndex = 0; CopyIndex < CopiedStructures.Num(); ++CopyIndex)
			{
				const FSRAssemblyAreaCopiedStructure& CopiedStructure = CopiedStructures[CopyIndex];
				TargetOriginCellIds[CopyIndex].Face = HoverCellId.Face;
				TargetOriginCellIds[CopyIndex].CellX = HoverCellId.CellX + CopiedStructure.AnchorOffset.X;
				TargetOriginCellIds[CopyIndex].CellY = HoverCellId.CellY + CopiedStructure.AnchorOffset.Y;
			}

			UpdateStructurePreviewActors(SurfaceGrid, TargetOriginCellIds, LastPreviewState);
			return true;
		}

		FSRAssemblyAreaCopyPlacementEvaluation Evaluation;
		if (!BuildPlacementEvaluation(SurfaceGrid, HoverCellId, Evaluation))
		{
			if (IsValid(StructureInstanceManager))
			{
				StructureInstanceManager->ClearDeletePreviewedStructures();
			}
			SurfaceGrid->ClearConstructionReplacementPreviewCells();
			ConveyorPreview.ClearBulkDeletionPreview();
			ApplyPreviewState(ESRAssemblyAreaCopyPlacementPreviewState::Blocked);
			return false;
		}

		USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
			: nullptr;
		StorePreviewEvaluation(HoverCellId, Evaluation);
		if (IsValid(StructureInstanceManager))
		{
			StructureInstanceManager->SetConstructionReplacementPreviewedStructures(Evaluation.ReplaceableOccupantIds);
		}
		SurfaceGrid->SetConstructionReplacementPreviewCells(Evaluation.ReplaceableOccupiedCellIds.Array());
		ConstructionReplacement::ApplyConveyorReplacementPreview(
			SurfaceGrid,
			ConveyorNetwork,
			ConveyorPreview,
			Evaluation.ReplaceableConveyorCellIds,
			Evaluation.ReplaceableConveyorBeltPaths,
			HoverCellId);
		ApplyPreviewState(Evaluation.PreviewState);
		UpdateStructurePreviewActors(SurfaceGrid, Evaluation.TargetOriginCellIds, Evaluation.PreviewState);

		for (int32 CopyIndex = 0; CopyIndex < CopiedConveyorPaths.Num(); ++CopyIndex)
		{
			if (!Evaluation.TargetConveyorBeltPaths.IsValidIndex(CopyIndex))
			{
				continue;
			}

			FSRAssemblyAreaCopiedConveyorPath& CopiedConveyorPath = CopiedConveyorPaths[CopyIndex];
			ASRConveyorBeltActor* PreviewActor = CopiedConveyorPath.PreviewActor.Get();
			if (!IsValid(PreviewActor) || !IsValid(ConveyorNetwork))
			{
				continue;
			}

			TArray<FSRConveyorBeltPath> PreviewBeltPaths;
			PreviewBeltPaths.Add(Evaluation.TargetConveyorBeltPaths[CopyIndex]);
			if (!PreviewActor->InitializeConveyorPaths(
				SurfaceGrid,
				PreviewBeltPaths,
				ConveyorNetwork->GetConveyorActorSplineComponentTag(),
				ConveyorNetwork->GetConveyorActorSurfaceOffset()))
			{
				PreviewActor->SetActorHiddenInGame(true);
				continue;
			}

			PreviewActor->SetConveyorGhostMode(
				true,
				ResolvePreviewMaterial(CopiedConveyorPath.StructureDataAsset.Get(), Evaluation.PreviewState));
			PreviewActor->SetActorHiddenInGame(PreviewActor->IsConveyorGhostGenerationPending());
		}

		return true;
	}

	void FSRAssemblyAreaCopy::UpdateStructurePreviewActors(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& TargetOriginCellIds,
		ESRAssemblyAreaCopyPlacementPreviewState PreviewState)
	{
		if (!IsValid(SurfaceGrid))
		{
			return;
		}

		for (int32 CopyIndex = 0; CopyIndex < CopiedStructures.Num(); ++CopyIndex)
		{
			if (!TargetOriginCellIds.IsValidIndex(CopyIndex))
			{
				continue;
			}

			FSRAssemblyAreaCopiedStructure& CopiedStructure = CopiedStructures[CopyIndex];
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
			ApplyPreviewMaterial(PreviewActor, ResolvePreviewMaterial(StructureDataAsset, PreviewState));
			PreviewActor->SetActorHiddenInGame(false);
		}
	}

	UMaterialInterface* FSRAssemblyAreaCopy::ResolvePreviewMaterial(
		USRStructureDataAsset* StructureDataAsset,
		ESRAssemblyAreaCopyPlacementPreviewState PreviewState)
	{
		if (!IsValid(StructureDataAsset))
		{
			return nullptr;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		switch (PreviewState)
		{
		case ESRAssemblyAreaCopyPlacementPreviewState::Placeable:
			return IsValid(StructureData.CopyPlaceableMaterial.Get())
				? StructureData.CopyPlaceableMaterial.Get()
				: IsValid(StructureData.GhostMaterial.Get())
				? StructureData.GhostMaterial.Get()
				: StructureData.Material.Get();
		case ESRAssemblyAreaCopyPlacementPreviewState::Replaceable:
			return IsValid(StructureData.ReplaceableMaterial.Get())
				? StructureData.ReplaceableMaterial.Get()
				: IsValid(StructureData.DeleteMaterial.Get())
				? StructureData.DeleteMaterial.Get()
				: IsValid(StructureData.GhostMaterial.Get())
				? StructureData.GhostMaterial.Get()
				: StructureData.Material.Get();
		case ESRAssemblyAreaCopyPlacementPreviewState::Blocked:
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

	void FSRAssemblyAreaCopy::ApplyPreviewMaterial(AActor* PreviewActor, UMaterialInterface* Material)
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

	bool FSRAssemblyAreaCopy::BuildPlacementEvaluation(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& HoverCellId,
		FSRAssemblyAreaCopyPlacementEvaluation& OutEvaluation) const
	{
		OutEvaluation = FSRAssemblyAreaCopyPlacementEvaluation();
		OutEvaluation.TargetOriginCellIds.SetNum(CopiedStructures.Num());
		OutEvaluation.TargetConveyorBeltPaths.SetNum(CopiedConveyorPaths.Num());
		if (!IsValid(SurfaceGrid) || !HasPayload())
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
		if (!CopiedStructures.IsEmpty() && !IsValid(StructureInstanceManager))
		{
			bBlocked = true;
		}

		for (int32 CopyIndex = 0; CopyIndex < CopiedStructures.Num(); ++CopyIndex)
		{
			const FSRAssemblyAreaCopiedStructure& CopiedStructure = CopiedStructures[CopyIndex];
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

				if (!CellInfo.OccupantId.IsNone()
					&& IsReplaceableOccupant(StructureInstanceManager, CellInfo.OccupantId))
				{
					OutEvaluation.ReplaceableOccupantIds.Add(CellInfo.OccupantId);
					continue;
				}

				if (ConstructionReplacement::HasGroundConveyorAtCell(ConveyorNetwork, FootprintCellId))
				{
					OutEvaluation.ReplaceableConveyorCellIds.AddUnique(FootprintCellId);
					continue;
				}

				bBlocked = true;
			}
		}

		CollectReplaceableOccupiedCellIds(
			StructureInstanceManager,
			OutEvaluation.ReplaceableOccupantIds,
			OutEvaluation.ReplaceableOccupiedCellIds);
		for (const FSRPlanetSurfaceGridCellId& ConveyorCellId : OutEvaluation.ReplaceableConveyorCellIds)
		{
			OutEvaluation.ReplaceableOccupiedCellIds.Add(ConveyorCellId);
		}
		ConstructionReplacement::CollectGroundConveyorBeltPaths(
			ConveyorNetwork,
			OutEvaluation.ReplaceableConveyorCellIds,
			OutEvaluation.ReplaceableConveyorBeltPaths);

		if (!CopiedConveyorPaths.IsEmpty() && !IsValid(ConveyorNetwork))
		{
			bBlocked = true;
		}

		for (int32 CopyIndex = 0; CopyIndex < CopiedConveyorPaths.Num(); ++CopyIndex)
		{
			const FSRAssemblyAreaCopiedConveyorPath& CopiedConveyorPath = CopiedConveyorPaths[CopyIndex];
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

			FSRConveyorBeltPath TargetBeltPath;
			TargetBeltPath.Layer = FMath::Max(0, CopiedConveyorPath.Layer);
			TargetBeltPath.LayerHeight = CopiedConveyorPath.LayerHeight;
			TargetBeltPath.NetworkId = CopiedConveyorPath.NetworkId;
			TargetBeltPath.StructureDataAsset = StructureDataAsset;
			TargetBeltPath.CellIds.Reserve(CopiedConveyorPath.AnchorOffsets.Num());
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

				if (TargetBeltPath.Layer == 0 && ProposedFootprintCellIds.Contains(TargetCellId))
				{
					bBlocked = true;
					continue;
				}

				FSRConveyorLaneKey TargetLaneKey;
				TargetLaneKey.CellId = TargetCellId;
				TargetLaneKey.Layer = TargetBeltPath.Layer;

				FSRPlanetSurfaceGridCellInfo CellInfo;
				if (!SurfaceGrid->GetCellInfoById(TargetCellId, CellInfo))
				{
					bBlocked = true;
					continue;
				}

				if (TargetBeltPath.Layer == 0
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
				TargetBeltPath.CellIds.Add(TargetCellId);
			}

			if (TargetBeltPath.CellIds.IsEmpty())
			{
				bBlocked = true;
				continue;
			}

			FSRConveyorBeltPath ConnectedBeltPath;
			if (!TryBuildConnectedAreaCopyConveyorPath(
				SurfaceGrid,
				ConveyorNetwork,
				TargetBeltPath,
				OutEvaluation.ReplaceableOccupiedCellIds,
				ConnectedBeltPath))
			{
				bBlocked = true;
				OutEvaluation.TargetConveyorBeltPaths[CopyIndex] = TargetBeltPath;
				continue;
			}

			OutEvaluation.TargetConveyorBeltPaths[CopyIndex] = MoveTemp(ConnectedBeltPath);
		}

		if (bBlocked)
		{
			OutEvaluation.PreviewState = ESRAssemblyAreaCopyPlacementPreviewState::Blocked;
			OutEvaluation.bCanPlace = false;
			return true;
		}

		OutEvaluation.PreviewState = OutEvaluation.ReplaceableOccupantIds.IsEmpty()
			&& OutEvaluation.ReplaceableConveyorCellIds.IsEmpty()
				? ESRAssemblyAreaCopyPlacementPreviewState::Placeable
				: ESRAssemblyAreaCopyPlacementPreviewState::Replaceable;
		OutEvaluation.bCanPlace = true;
		return true;
	}

	bool FSRAssemblyAreaCopy::TryCommitPlacement(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& HoverCellId,
		const FSRAssemblyPlacementHistory& PlacementHistory,
		FSRAssemblyAreaCopyCommitResult& OutResult)
	{
		OutResult = FSRAssemblyAreaCopyCommitResult();
		if (!IsPlacementActive() || !HasPayload())
		{
			return false;
		}

		if (!IsValid(SurfaceGrid))
		{
			return true;
		}

		FSRAssemblyAreaCopyPlacementEvaluation Evaluation;
		if (!BuildPlacementEvaluation(SurfaceGrid, HoverCellId, Evaluation) || !Evaluation.bCanPlace)
		{
			ApplyPreviewState(ESRAssemblyAreaCopyPlacementPreviewState::Blocked);
			return true;
		}

		AActor* SurfaceOwner = SurfaceGrid->GetOwner();
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
			: nullptr;
		if ((!CopiedStructures.IsEmpty() && !IsValid(StructureInstanceManager))
			|| (!CopiedConveyorPaths.IsEmpty() && !IsValid(ConveyorNetwork)))
		{
			return true;
		}

		SurfaceGrid->BeginInteractionHighlightBatch();
		TArray<FSRPlacedStructureInstance> RemovedStructures;
		TArray<FSRRestorableNaturalStructure> RemovedRestorableStructures;
		TArray<FSRConveyorBeltPath> RemovedConveyorBeltPaths = Evaluation.ReplaceableConveyorBeltPaths;
		if (!Evaluation.ReplaceableOccupantIds.IsEmpty() && IsValid(StructureInstanceManager))
		{
			StructureInstanceManager->RemoveConstructionDestructibleStructuresByOccupantIds(
				SurfaceGrid,
				Evaluation.ReplaceableOccupantIds,
				&RemovedStructures);
			AppendRestorableStructures(RemovedStructures, RemovedRestorableStructures);
		}
		if (!Evaluation.ReplaceableConveyorCellIds.IsEmpty())
		{
			if (!IsValid(ConveyorNetwork))
			{
				RestoreRemovedStructures(SurfaceGrid, StructureInstanceManager, RemovedStructures);
				SurfaceGrid->EndInteractionHighlightBatch();
				return true;
			}

			bool bRemovedAnyConveyor = false;
			for (const FSRPlanetSurfaceGridCellId& ConveyorCellId : Evaluation.ReplaceableConveyorCellIds)
			{
				bRemovedAnyConveyor |= ConveyorNetwork->TryRemoveConveyorAtCell(
					SurfaceGrid,
					ConveyorCellId,
					ConstructionReplacement::GroundConveyorLayer);
			}

			if (!bRemovedAnyConveyor)
			{
				RestoreRemovedStructures(SurfaceGrid, StructureInstanceManager, RemovedStructures);
				SurfaceGrid->EndInteractionHighlightBatch();
				return true;
			}
		}

		TArray<FSRAssemblyPlacementHistoryEntry> AreaCopyHistoryEntries;
		for (int32 CopyIndex = 0; CopyIndex < CopiedStructures.Num(); ++CopyIndex)
		{
			if (!Evaluation.TargetOriginCellIds.IsValidIndex(CopyIndex))
			{
				continue;
			}

			USRStructureDataAsset* StructureDataAsset = CopiedStructures[CopyIndex].StructureDataAsset.Get();
			if (!IsValid(StructureDataAsset))
			{
				continue;
			}

			FName NewOccupantId = NAME_None;
			const int32 PlacementRotationSteps = CopiedStructures[CopyIndex].PlacementRotationSteps;
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
				OutResult.bPlacedAny = true;
			}
		}

		for (int32 CopyIndex = 0; CopyIndex < CopiedConveyorPaths.Num(); ++CopyIndex)
		{
			if (!Evaluation.TargetConveyorBeltPaths.IsValidIndex(CopyIndex))
			{
				continue;
			}

			const FSRConveyorBeltPath& TargetBeltPath = Evaluation.TargetConveyorBeltPaths[CopyIndex];
			USRStructureDataAsset* StructureDataAsset = TargetBeltPath.StructureDataAsset.Get();
			if (!IsValid(StructureDataAsset) || TargetBeltPath.CellIds.IsEmpty())
			{
				continue;
			}

			const FSRStructureData ConveyorData = StructureDataAsset->BuildData();
			const FName NetworkId = MakeAreaCopyConveyorNetworkId(SurfaceOwner, TargetBeltPath.Layer);
			FSRConveyorBeltPath HistoryBeltPath;
			TArray<FSRPlanetSurfaceGridCellId> HistoryPlacedCellIds;
			TArray<FSRRestorableNaturalStructure> HistoryRemovedNaturalStructures;
			PlacementHistory.BuildConveyorPlacementPayload(
				SurfaceGrid,
				ConveyorNetwork,
				StructureDataAsset,
				TargetBeltPath.CellIds,
				TargetBeltPath.Layer,
				TargetBeltPath.LayerHeight > KINDA_SMALL_NUMBER ? TargetBeltPath.LayerHeight : ConveyorData.ConveyorLayerHeight,
				NetworkId,
				HistoryBeltPath,
				HistoryPlacedCellIds,
				HistoryRemovedNaturalStructures);
			if (ConveyorNetwork->TryPlaceConveyorPath(
				SurfaceGrid,
				TargetBeltPath.CellIds,
				TargetBeltPath.Layer,
				TargetBeltPath.LayerHeight > KINDA_SMALL_NUMBER ? TargetBeltPath.LayerHeight : ConveyorData.ConveyorLayerHeight,
				StructureDataAsset,
				NetworkId))
			{
				if (!HistoryPlacedCellIds.IsEmpty() && IsValid(HistoryBeltPath.StructureDataAsset.Get()))
				{
					FSRAssemblyPlacementHistoryEntry HistoryEntry;
					HistoryEntry.Kind = ESRAssemblyPlacementHistoryKind::Conveyor;
					HistoryEntry.SurfaceGrid = SurfaceGrid;
					HistoryEntry.ConveyorNetwork = ConveyorNetwork;
					HistoryEntry.StructureDataAsset = HistoryBeltPath.StructureDataAsset.Get();
					HistoryEntry.ConveyorBeltPath = HistoryBeltPath;
					HistoryEntry.ConveyorPlacedCellIds = HistoryPlacedCellIds;
					HistoryEntry.RemovedNaturalStructures = HistoryRemovedNaturalStructures;
					AreaCopyHistoryEntries.Add(HistoryEntry);
				}
				OutResult.bPlacedAny = true;
			}
		}
		SurfaceGrid->EndInteractionHighlightBatch();

		if (!OutResult.bPlacedAny)
		{
			RestoreRemovedStructures(SurfaceGrid, StructureInstanceManager, RemovedStructures);
			ConstructionReplacement::RestoreConveyorBeltPaths(SurfaceGrid, ConveyorNetwork, RemovedConveyorBeltPaths);
			return true;
		}

		if (!RemovedRestorableStructures.IsEmpty() && !AreaCopyHistoryEntries.IsEmpty())
		{
			AreaCopyHistoryEntries[0].RemovedNaturalStructures.Append(RemovedRestorableStructures);
		}
		if (!RemovedConveyorBeltPaths.IsEmpty() && !AreaCopyHistoryEntries.IsEmpty())
		{
			AreaCopyHistoryEntries[0].RemovedConveyorBeltPaths.Append(RemovedConveyorBeltPaths);
			AreaCopyHistoryEntries[0].ConveyorNetwork = ConveyorNetwork;
		}

		OutResult.SurfaceOwner = SurfaceOwner;
		OutResult.HistoryEntries = MoveTemp(AreaCopyHistoryEntries);
		ClearPreviewHoverCache();
		return true;
	}

	bool FSRAssemblyAreaCopy::MirrorPlacement(bool bMirrorLeftRight)
	{
		if (!IsPlacementActive() || !HasPayload())
		{
			return false;
		}

		for (FSRAssemblyAreaCopiedStructure& CopiedStructure : CopiedStructures)
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

		for (FSRAssemblyAreaCopiedConveyorPath& CopiedConveyorPath : CopiedConveyorPaths)
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

		ClearPreviewHoverCache();
		return true;
	}

	bool FSRAssemblyAreaCopy::RotatePlacement(int32 StepDelta)
	{
		if (!IsPlacementActive() || !HasPayload())
		{
			return false;
		}

		const int32 RotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(StepDelta);
		if (RotationSteps == 0)
		{
			return false;
		}

		for (int32 StepIndex = 0; StepIndex < RotationSteps; ++StepIndex)
		{
			for (FSRAssemblyAreaCopiedStructure& CopiedStructure : CopiedStructures)
			{
				int32 FootprintCellsX = 1;
				int32 FootprintCellsY = 1;
				const int32 CurrentRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(CopiedStructure.PlacementRotationSteps);

				if (USRStructureDataAsset* StructureDataAsset = CopiedStructure.StructureDataAsset.Get())
				{
					const FSRStructureData StructureData = StructureDataAsset->BuildData();
					FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, CurrentRotationSteps);
					FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, CurrentRotationSteps);
				}

				CopiedStructure.AnchorOffset = RotateStructureAnchorOffsetClockwise(
					CopiedStructure.AnchorOffset,
					FootprintCellsX,
					FootprintCellsY);
				CopiedStructure.PlacementRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(CurrentRotationSteps + 1);
			}

			for (FSRAssemblyAreaCopiedConveyorPath& CopiedConveyorPath : CopiedConveyorPaths)
			{
				for (FIntPoint& AnchorOffset : CopiedConveyorPath.AnchorOffsets)
				{
					AnchorOffset = RotateCellOffsetClockwise(AnchorOffset);
				}
			}
		}

		ClearPreviewHoverCache();
		return true;
	}
}
