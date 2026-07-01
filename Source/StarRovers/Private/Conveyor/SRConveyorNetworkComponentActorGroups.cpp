#include "Conveyor/SRConveyorNetworkComponent.h"

#include "SRConveyorNetworkComponentInternal.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

FName USRConveyorNetworkComponent::MakeActorGroupKey(USRStructureDataAsset* StructureDataAsset, int32 Layer)
{
	if (!IsValid(StructureDataAsset))
	{
		return NAME_None;
	}

	return FName(*FString::Printf(
		TEXT("%s|Layer_%d"),
		*StructureDataAsset->GetPathName(),
		FMath::Max(0, Layer)));
}

ASRConveyorBeltActor* USRConveyorNetworkComponent::SpawnConveyorActorForVisualPaths(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorVisualPath>& GroupedVisualPaths)
{
	if (!IsValid(SurfaceGrid) || GroupedVisualPaths.IsEmpty())
	{
		return nullptr;
	}

	const FSRConveyorVisualPath& FirstVisualPath = GroupedVisualPaths[0];
	if (!IsValid(FirstVisualPath.StructureDataAsset) || FirstVisualPath.CellIds.IsEmpty())
	{
		return nullptr;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	UWorld* World = SurfaceOwner ? SurfaceOwner->GetWorld() : nullptr;
	const FSRStructureData StructureData = FirstVisualPath.StructureDataAsset->BuildData();
	UClass* ConveyorActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(SurfaceOwner) || !World || !IsValid(ConveyorActorClass) || !ConveyorActorClass->IsChildOf(ASRConveyorBeltActor::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot place conveyor from '%s': StructureActorClass must be set to a subclass of ASRConveyorBeltActor."), *GetNameSafe(FirstVisualPath.StructureDataAsset));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = SurfaceOwner;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASRConveyorBeltActor* PlacedConveyorActor = World->SpawnActor<ASRConveyorBeltActor>(ConveyorActorClass, SurfaceOwner->GetActorTransform(), SpawnParameters);
	if (!IsValid(PlacedConveyorActor))
	{
		return nullptr;
	}

	PlacedConveyorActor->SetOwner(SurfaceOwner);
	PlacedConveyorActor->SetActorHiddenInGame(false);
	if (!PlacedConveyorActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform))
	{
		PlacedConveyorActor->Destroy();
		return nullptr;
	}

	if (!PlacedConveyorActor->InitializeConveyorPaths(SurfaceGrid, GroupedVisualPaths, PCGSplineComponentTag, BeltSurfaceOffset + PCGSplineHeightOffset))
	{
		PlacedConveyorActor->Destroy();
		return nullptr;
	}

	return PlacedConveyorActor;
}

void USRConveyorNetworkComponent::DestroyPlacedConveyorActors()
{
	for (ASRConveyorBeltActor* ConveyorActor : PlacedConveyorActors)
	{
		if (IsValid(ConveyorActor))
		{
			ConveyorActor->Destroy();
		}
	}

	PlacedConveyorActors.Reset();
	ActorGroupState.Reset();
	PendingConveyorActorRefreshSurfaceGrid.Reset();
	SetComponentTickEnabled(ShouldKeepTransportTickEnabled() || bShowPathDebugLine || bShowConnectionDebugLine);
}

void USRConveyorNetworkComponent::MarkConveyorActorGroupDirty(USRStructureDataAsset* StructureDataAsset, int32 Layer)
{
	const FName ActorGroupKey = MakeActorGroupKey(StructureDataAsset, Layer);
	if (ActorGroupKey.IsNone())
	{
		return;
	}

	FSRConveyorActorGroupState& ActorGroup = ActorGroupState.GroupsByKey.FindOrAdd(ActorGroupKey);
	ActorGroup.bDirty = true;
}

void USRConveyorNetworkComponent::MarkConveyorActorGroupPlacementDiagnosticPending(USRStructureDataAsset* StructureDataAsset, int32 Layer)
{
	const FName ActorGroupKey = MakeActorGroupKey(StructureDataAsset, Layer);
	if (!ActorGroupKey.IsNone())
	{
		ActorGroupState.PendingPlacementDiagnosticKeys.Add(ActorGroupKey);
	}
}

void USRConveyorNetworkComponent::MarkConveyorActorGroupDeletionDiagnosticPending(USRStructureDataAsset* StructureDataAsset, int32 Layer)
{
	const FName ActorGroupKey = MakeActorGroupKey(StructureDataAsset, Layer);
	if (!ActorGroupKey.IsNone())
	{
		ActorGroupState.PendingDeletionDiagnosticKeys.Add(ActorGroupKey);
	}
}

void USRConveyorNetworkComponent::ScheduleDirtyConveyorActorGroupRefresh(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bSpawnConveyorBeltActors || !HasDirtyConveyorActorGroups())
	{
		return;
	}

	if (IsValid(SurfaceGrid))
	{
		PendingConveyorActorRefreshSurfaceGrid = SurfaceGrid;
	}
	SetComponentTickEnabled(true);
}

bool USRConveyorNetworkComponent::RefreshConveyorActorGroup(USRPlanetSurfaceGrid* SurfaceGrid, FName ActorGroupKey)
{
	if (ActorGroupKey.IsNone())
	{
		return true;
	}

	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRConveyorActorGroupState& ActorGroup = ActorGroupState.GroupsByKey.FindOrAdd(ActorGroupKey);
	const bool bLogPlacementDiagnostics = ActorGroupState.PendingPlacementDiagnosticKeys.Remove(ActorGroupKey) > 0;
	const bool bLogDeletionDiagnostics = ActorGroupState.PendingDeletionDiagnosticKeys.Remove(ActorGroupKey) > 0;
	ActorGroup.VisualPaths.Reset();
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (MakeActorGroupKey(VisualPath.StructureDataAsset.Get(), VisualPath.Layer) == ActorGroupKey)
		{
			ActorGroup.VisualPaths.Add(VisualPath);
		}
	}

	if (ActorGroup.VisualPaths.IsEmpty())
	{
		if (IsValid(ActorGroup.Actor))
		{
			PlacedConveyorActors.RemoveAll([&ActorGroup](const auto& PlacedActor)
			{
				return PlacedActor.Get() == ActorGroup.Actor;
			});
			ActorGroup.Actor->Destroy();
		}

		ActorGroupState.GroupsByKey.Remove(ActorGroupKey);
		if (bLogDeletionDiagnostics)
		{
			LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorDelete.ActorGroupRemoved"), ActorGroupKey, StarRovers::Conveyor::ShouldForceGCOnConveyorDelete());
		}
		return true;
	}

	if (!IsValid(ActorGroup.Actor))
	{
		ActorGroup.Actor = SpawnConveyorActorForVisualPaths(SurfaceGrid, ActorGroup.VisualPaths);
		if (IsValid(ActorGroup.Actor))
		{
			PlacedConveyorActors.AddUnique(ActorGroup.Actor);
		}
	}
	else if (!ActorGroup.Actor->InitializeConveyorPaths(SurfaceGrid, ActorGroup.VisualPaths, PCGSplineComponentTag, BeltSurfaceOffset + PCGSplineHeightOffset))
	{
		PlacedConveyorActors.RemoveAll([&ActorGroup](const auto& PlacedActor)
		{
			return PlacedActor.Get() == ActorGroup.Actor;
		});
		ActorGroup.Actor->Destroy();
		ActorGroup.Actor = nullptr;
	}

	ActorGroup.bDirty = false;
	if (bLogPlacementDiagnostics)
	{
		LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorPlace.ActorGroupRefreshed"), ActorGroupKey, false);
	}
	if (bLogDeletionDiagnostics)
	{
		LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorDelete.ActorGroupRefreshed"), ActorGroupKey, StarRovers::Conveyor::ShouldForceGCOnConveyorDelete());
	}
	return IsValid(ActorGroup.Actor);
}

bool USRConveyorNetworkComponent::RefreshDirtyConveyorActorGroups(USRPlanetSurfaceGrid* SurfaceGrid, int32 MaxGroupCount)
{
	if (!bSpawnConveyorBeltActors)
	{
		return true;
	}

	TArray<FName> DirtyActorGroupKeys;
	const int32 GroupBudget = MaxGroupCount == INDEX_NONE
		? TNumericLimits<int32>::Max()
		: FMath::Max(1, MaxGroupCount);
	for (const TPair<FName, FSRConveyorActorGroupState>& ActorGroupPair : ActorGroupState.GroupsByKey)
	{
		if (ActorGroupPair.Value.bDirty)
		{
			DirtyActorGroupKeys.Add(ActorGroupPair.Key);
			if (DirtyActorGroupKeys.Num() >= GroupBudget)
			{
				break;
			}
		}
	}

	bool bAllGroupsRefreshed = true;
	for (const FName ActorGroupKey : DirtyActorGroupKeys)
	{
		if (!RefreshConveyorActorGroup(SurfaceGrid, ActorGroupKey))
		{
			bAllGroupsRefreshed = false;
		}
	}

	return bAllGroupsRefreshed;
}

bool USRConveyorNetworkComponent::HasDirtyConveyorActorGroups() const
{
	return ActorGroupState.HasDirtyGroups();
}

void USRConveyorNetworkComponent::RebuildSegmentsFromVisualPaths(USRPlanetSurfaceGrid* SurfaceGrid)
{
	Segments.Reset();

	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		for (int32 PathIndex = 0; PathIndex < VisualPath.CellIds.Num(); ++PathIndex)
		{
			const FSRPlanetSurfaceGridCellId& CellId = VisualPath.CellIds[PathIndex];
			ESRConveyorGridDirection InputDirection = ESRConveyorGridDirection::None;
			ESRConveyorGridDirection OutputDirection = ESRConveyorGridDirection::None;
			if (PathIndex > 0)
			{
				FindDirectionBetweenCells(SurfaceGrid, CellId, VisualPath.CellIds[PathIndex - 1], InputDirection);
			}
			if (PathIndex + 1 < VisualPath.CellIds.Num())
			{
				FindDirectionBetweenCells(SurfaceGrid, CellId, VisualPath.CellIds[PathIndex + 1], OutputDirection);
			}

			FSRConveyorSegment Segment;
			Segment.Lane = MakeLaneKey(CellId, VisualPath.Layer);
			Segment.InputDirection = InputDirection;
			Segment.OutputDirection = OutputDirection;
			Segment.Shape = ResolveSegmentShape(InputDirection, OutputDirection);
			Segment.NetworkId = VisualPath.NetworkId;
			Segment.StructureDataAsset = VisualPath.StructureDataAsset;
			MergeConveyorSegment(Segment);
		}
	}
}

bool USRConveyorNetworkComponent::RebuildPlacedConveyorActors(USRPlanetSurfaceGrid* SurfaceGrid)
{
	DestroyPlacedConveyorActors();

	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!IsValid(VisualPath.StructureDataAsset) || VisualPath.CellIds.IsEmpty())
		{
			continue;
		}

		MarkConveyorActorGroupDirty(VisualPath.StructureDataAsset.Get(), VisualPath.Layer);
	}

	return RefreshDirtyConveyorActorGroups(SurfaceGrid);
}
