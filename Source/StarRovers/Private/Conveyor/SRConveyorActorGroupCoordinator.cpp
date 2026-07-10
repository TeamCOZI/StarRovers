#include "Conveyor/SRConveyorActorGroupCoordinator.h"

#include "Utility/SRLog.h"
#include "SRConveyorDeletionDiagnostics.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

FName StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::MakeGroupKey(
	USRStructureDataAsset* StructureDataAsset,
	int32 Layer)
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

void StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::MarkGroupDirty(
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	USRStructureDataAsset* StructureDataAsset,
	int32 Layer)
{
	const FName ActorGroupKey = MakeGroupKey(StructureDataAsset, Layer);
	if (ActorGroupKey.IsNone())
	{
		return;
	}

	FSRConveyorActorGroupState& ActorGroup = ActorGroupState.GroupsByKey.FindOrAdd(ActorGroupKey);
	ActorGroup.bDirty = true;
}

void StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::MarkPlacementDiagnosticPending(
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	USRStructureDataAsset* StructureDataAsset,
	int32 Layer)
{
	const FName ActorGroupKey = MakeGroupKey(StructureDataAsset, Layer);
	if (!ActorGroupKey.IsNone())
	{
		ActorGroupState.PendingPlacementDiagnosticKeys.Add(ActorGroupKey);
	}
}

void StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::MarkDeletionDiagnosticPending(
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	USRStructureDataAsset* StructureDataAsset,
	int32 Layer)
{
	const FName ActorGroupKey = MakeGroupKey(StructureDataAsset, Layer);
	if (!ActorGroupKey.IsNone())
	{
		ActorGroupState.PendingDeletionDiagnosticKeys.Add(ActorGroupKey);
	}
}

void StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::MarkGroupsDirtyForBeltPaths(
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	const TArray<FSRConveyorBeltPath>& BeltPaths)
{
	for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
	{
		if (!IsValid(BeltPath.StructureDataAsset) || BeltPath.CellIds.IsEmpty())
		{
			continue;
		}

		MarkGroupDirty(ActorGroupState, BeltPath.StructureDataAsset.Get(), BeltPath.Layer);
	}
}

void StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::DestroyActors(
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	TArray<TObjectPtr<ASRConveyorBeltActor>>& PlacedConveyorActors)
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
}

bool StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::RefreshDirtyGroups(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	TArray<TObjectPtr<ASRConveyorBeltActor>>& PlacedConveyorActors,
	const FSRConveyorActorGroupRefreshSettings& Settings,
	int32 MaxGroupCount,
	TFunctionRef<void(const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection)> LogMutationDiagnostics)
{
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
		if (!RefreshGroup(SurfaceGrid, ActorGroupKey, BeltPaths, ActorGroupState, PlacedConveyorActors, Settings, LogMutationDiagnostics))
		{
			bAllGroupsRefreshed = false;
		}
	}

	return bAllGroupsRefreshed;
}

ASRConveyorBeltActor* StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::SpawnActorForBeltPaths(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorBeltPath>& GroupedBeltPaths,
	const FSRConveyorActorGroupRefreshSettings& Settings)
{
	if (!IsValid(SurfaceGrid) || GroupedBeltPaths.IsEmpty())
	{
		return nullptr;
	}

	const FSRConveyorBeltPath& FirstBeltPath = GroupedBeltPaths[0];
	if (!IsValid(FirstBeltPath.StructureDataAsset) || FirstBeltPath.CellIds.IsEmpty())
	{
		return nullptr;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	UWorld* World = SurfaceOwner ? SurfaceOwner->GetWorld() : nullptr;
	const FSRStructureData StructureData = FirstBeltPath.StructureDataAsset->BuildData();
	UClass* ConveyorActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(SurfaceOwner) || !World || !IsValid(ConveyorActorClass) || !ConveyorActorClass->IsChildOf(ASRConveyorBeltActor::StaticClass()))
	{
		SR_LOG(Conveyor, LogTemp, Error, TEXT("Cannot place conveyor from '%s': StructureActorClass must be set to a subclass of ASRConveyorBeltActor."), *GetNameSafe(FirstBeltPath.StructureDataAsset));
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

	if (!PlacedConveyorActor->InitializeConveyorPaths(
		SurfaceGrid,
		GroupedBeltPaths,
		Settings.PCGSplineComponentTag,
		Settings.ConveyorActorSurfaceOffset))
	{
		PlacedConveyorActor->Destroy();
		return nullptr;
	}

	return PlacedConveyorActor;
}

bool StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::RefreshGroup(
	USRPlanetSurfaceGrid* SurfaceGrid,
	FName ActorGroupKey,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	TArray<TObjectPtr<ASRConveyorBeltActor>>& PlacedConveyorActors,
	const FSRConveyorActorGroupRefreshSettings& Settings,
	TFunctionRef<void(const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection)> LogMutationDiagnostics)
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
	ActorGroup.BeltPaths.Reset();
	ActorGroup.BeltPaths.Reserve(BeltPaths.Num());
	for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
	{
		if (MakeGroupKey(BeltPath.StructureDataAsset.Get(), BeltPath.Layer) == ActorGroupKey)
		{
			ActorGroup.BeltPaths.Add(BeltPath);
		}
	}

	if (ActorGroup.BeltPaths.IsEmpty())
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
			LogMutationDiagnostics(TEXT("ConveyorDelete.ActorGroupRemoved"), ActorGroupKey, ShouldForceGCOnConveyorDelete());
		}
		return true;
	}

	if (!IsValid(ActorGroup.Actor))
	{
		ActorGroup.Actor = SpawnActorForBeltPaths(SurfaceGrid, ActorGroup.BeltPaths, Settings);
		if (IsValid(ActorGroup.Actor))
		{
			PlacedConveyorActors.AddUnique(ActorGroup.Actor);
		}
	}
	else if (!ActorGroup.Actor->InitializeConveyorPaths(
		SurfaceGrid,
		ActorGroup.BeltPaths,
		Settings.PCGSplineComponentTag,
		Settings.ConveyorActorSurfaceOffset))
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
		LogMutationDiagnostics(TEXT("ConveyorPlace.ActorGroupRefreshed"), ActorGroupKey, false);
	}
	if (bLogDeletionDiagnostics)
	{
		LogMutationDiagnostics(TEXT("ConveyorDelete.ActorGroupRefreshed"), ActorGroupKey, ShouldForceGCOnConveyorDelete());
	}
	return IsValid(ActorGroup.Actor);
}
