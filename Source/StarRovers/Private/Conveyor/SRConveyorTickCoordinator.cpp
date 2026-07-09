#include "Conveyor/SRConveyorTickCoordinator.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Surface/SRPlanetSurfaceGrid.h"

USRPlanetSurfaceGrid* StarRovers::Conveyor::FSRConveyorTickCoordinator::ResolveSurfaceGrid(
	AActor* OwnerActor,
	TWeakObjectPtr<USRPlanetSurfaceGrid>& CachedSurfaceGrid)
{
	USRPlanetSurfaceGrid* SurfaceGrid = CachedSurfaceGrid.Get();
	if (IsValid(SurfaceGrid))
	{
		return SurfaceGrid;
	}

	SurfaceGrid = IsValid(OwnerActor)
		? OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>()
		: nullptr;
	CachedSurfaceGrid = SurfaceGrid;
	return SurfaceGrid;
}

float StarRovers::Conveyor::FSRConveyorTickCoordinator::ResolveTransportDeltaTime(
	const UWorld* World,
	float DeltaTime)
{
	float TransportDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (const USRTimeControlSubsystem* TimeControlSubsystem = World ? World->GetSubsystem<USRTimeControlSubsystem>() : nullptr)
	{
		TransportDeltaTime *= FMath::Max(0.0f, TimeControlSubsystem->GetEffectiveTimeScale());
	}

	return TransportDeltaTime;
}

bool StarRovers::Conveyor::FSRConveyorTickCoordinator::ShouldKeepTickEnabled(
	bool bHasDirtyActorGroups,
	bool bShouldKeepTransportTickEnabled,
	bool bShowPathDebugLine,
	bool bShowConnectionDebugLine)
{
	return bHasDirtyActorGroups || bShouldKeepTransportTickEnabled || bShowPathDebugLine || bShowConnectionDebugLine;
}
