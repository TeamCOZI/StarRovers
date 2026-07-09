#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorTickCoordinator
	{
		static USRPlanetSurfaceGrid* ResolveSurfaceGrid(
			AActor* OwnerActor,
			TWeakObjectPtr<USRPlanetSurfaceGrid>& CachedSurfaceGrid);

		static float ResolveTransportDeltaTime(
			const UWorld* World,
			float DeltaTime);

		static bool ShouldKeepTickEnabled(
			bool bHasDirtyActorGroups,
			bool bShouldKeepTransportTickEnabled,
			bool bShowPathDebugLine,
			bool bShowConnectionDebugLine);
	};
}
