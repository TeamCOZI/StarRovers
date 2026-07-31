#include "SRFacilityMiningTargetResolver.h"

#include "Automation/SRFacilityRuntimeData.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool ResolveMiningSystems(
		const UActorComponent* OwnerComponent,
		USRPlanetSurfaceGrid*& OutSurfaceGrid,
		USRStructureInstanceManagerComponent*& OutStructureInstanceManager)
	{
		const AActor* Owner = IsValid(OwnerComponent) ? OwnerComponent->GetOwner() : nullptr;
		OutSurfaceGrid = IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
		OutStructureInstanceManager = IsValid(Owner)
			? Owner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		return IsValid(OutSurfaceGrid) && IsValid(OutStructureInstanceManager);
	}
}

bool FSRFacilityMiningTargetResolver::FindTargetDeposit(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance,
	FSRResourceDepositInstance& OutResourceDeposit)
{
	OutResourceDeposit = FSRResourceDepositInstance();

	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	USRStructureInstanceManagerComponent* StructureInstanceManager = nullptr;
	if (!ResolveMiningSystems(OwnerComponent, SurfaceGrid, StructureInstanceManager))
	{
		return false;
	}

	if (!FacilityInstance.MiningTargetDepositOccupantId.IsNone()
		&& StructureInstanceManager->GetResourceDepositInstance(FacilityInstance.MiningTargetDepositOccupantId, OutResourceDeposit)
		&& OutResourceDeposit.CanHarvestResource())
	{
		return true;
	}

	return StructureInstanceManager->FindAdjacentResourceDeposit(
		SurfaceGrid,
		FacilityInstance.FootprintCellIds,
		OutResourceDeposit);
}

bool FSRFacilityMiningTargetResolver::TryHarvestTargetDeposit(
	const UActorComponent* OwnerComponent,
	const FSRResourceDepositInstance& ResourceDeposit,
	FSRResourceInstance& OutMinedResource,
	FSRResourceDepositInstance& OutUpdatedResourceDeposit)
{
	OutMinedResource = FSRResourceInstance();
	OutUpdatedResourceDeposit = FSRResourceDepositInstance();

	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	USRStructureInstanceManagerComponent* StructureInstanceManager = nullptr;
	if (!ResolveMiningSystems(OwnerComponent, SurfaceGrid, StructureInstanceManager))
	{
		return false;
	}

	return StructureInstanceManager->TryHarvestResourceDeposit(
		SurfaceGrid,
		ResourceDeposit.OccupantId,
		OutMinedResource,
		OutUpdatedResourceDeposit);
}
