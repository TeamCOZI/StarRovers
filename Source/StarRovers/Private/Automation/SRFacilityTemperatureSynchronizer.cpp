#include "SRFacilityTemperatureSynchronizer.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	const USRPlanetSurfaceGrid* FindOwnerSurfaceGrid(const UActorComponent* Component)
	{
		const AActor* Owner = IsValid(Component) ? Component->GetOwner() : nullptr;
		return IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	}
}

bool FSRFacilityTemperatureSynchronizer::RefreshFacilityFromSurface(
	const UActorComponent* OwnerComponent,
	FSRFacilityNetworkRuntimeState& RuntimeState,
	FName OccupantId)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	const USRPlanetSurfaceGrid* SurfaceGrid = FindOwnerSurfaceGrid(OwnerComponent);
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo OriginCellInfo;
	if (!SurfaceGrid->GetCellInfoById(FacilityInstance->OriginCellId, OriginCellInfo))
	{
		return false;
	}

	FacilityInstance->TemperatureState = OriginCellInfo.TemperatureState;
	return true;
}

int32 FSRFacilityTemperatureSynchronizer::RefreshFacilitiesFromSurface(
	const UActorComponent* OwnerComponent,
	FSRFacilityNetworkRuntimeState& RuntimeState)
{
	const USRPlanetSurfaceGrid* SurfaceGrid = FindOwnerSurfaceGrid(OwnerComponent);
	if (!IsValid(SurfaceGrid) || RuntimeState.FacilityInstancesByOccupantId.IsEmpty())
	{
		return 0;
	}

	int32 ChangedTemperatureCount = 0;
	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		FSRPlanetSurfaceGridCellInfo OriginCellInfo;
		if (!SurfaceGrid->GetCellInfoById(FacilityInstance.OriginCellId, OriginCellInfo))
		{
			continue;
		}

		if (FacilityInstance.TemperatureState != OriginCellInfo.TemperatureState)
		{
			FacilityInstance.TemperatureState = OriginCellInfo.TemperatureState;
			++ChangedTemperatureCount;
		}
	}

	return ChangedTemperatureCount;
}
