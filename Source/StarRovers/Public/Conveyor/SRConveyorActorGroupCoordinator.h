#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorNetworkRuntimeState.h"
#include "Templates/Function.h"

class ASRConveyorBeltActor;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

namespace StarRovers::Conveyor
{
	struct FSRConveyorActorGroupRefreshSettings
	{
		FName PCGSplineComponentTag = NAME_None;
		float ConveyorActorSurfaceOffset = 0.0f;
	};

	struct STARROVERS_API FSRConveyorActorGroupCoordinator
	{
		static FName MakeGroupKey(USRStructureDataAsset* StructureDataAsset, int32 Layer);

		static void MarkGroupDirty(
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			USRStructureDataAsset* StructureDataAsset,
			int32 Layer);

		static void MarkPlacementDiagnosticPending(
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			USRStructureDataAsset* StructureDataAsset,
			int32 Layer);

		static void MarkDeletionDiagnosticPending(
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			USRStructureDataAsset* StructureDataAsset,
			int32 Layer);

		static void MarkGroupsDirtyForBeltPaths(
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			const TArray<FSRConveyorBeltPath>& BeltPaths);

		static void DestroyActors(
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			TArray<TObjectPtr<ASRConveyorBeltActor>>& PlacedConveyorActors);

		static bool RefreshDirtyGroups(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			TArray<TObjectPtr<ASRConveyorBeltActor>>& PlacedConveyorActors,
			const FSRConveyorActorGroupRefreshSettings& Settings,
			int32 MaxGroupCount,
			TFunctionRef<void(const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection)> LogMutationDiagnostics);

	private:
		static ASRConveyorBeltActor* SpawnActorForBeltPaths(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRConveyorBeltPath>& GroupedBeltPaths,
			const FSRConveyorActorGroupRefreshSettings& Settings);

		static bool RefreshGroup(
			USRPlanetSurfaceGrid* SurfaceGrid,
			FName ActorGroupKey,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			FSRConveyorActorGroupRuntimeState& ActorGroupState,
			TArray<TObjectPtr<ASRConveyorBeltActor>>& PlacedConveyorActors,
			const FSRConveyorActorGroupRefreshSettings& Settings,
			TFunctionRef<void(const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection)> LogMutationDiagnostics);
	};
}
