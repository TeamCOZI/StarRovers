#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	void AppendNeighborCellIds(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutNeighborCellIds)
	{
		if (!IsValid(SurfaceGrid))
		{
			return;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(CellId, Neighbors))
		{
			return;
		}

		OutNeighborCellIds.Add(Neighbors.NegativeU);
		OutNeighborCellIds.Add(Neighbors.PositiveU);
		OutNeighborCellIds.Add(Neighbors.NegativeV);
		OutNeighborCellIds.Add(Neighbors.PositiveV);
	}
}

bool USRStructureInstanceManagerComponent::GetResourceDepositInstance(
	FName OccupantId,
	FSRResourceDepositInstance& OutResourceDeposit) const
{
	if (const FSRResourceDepositInstance* ResourceDeposit = ResourceDepositsByOccupantId.Find(OccupantId))
	{
		OutResourceDeposit = *ResourceDeposit;
		return true;
	}

	OutResourceDeposit = FSRResourceDepositInstance();
	return false;
}

bool USRStructureInstanceManagerComponent::FindAdjacentResourceDeposit(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& SourceFootprintCellIds,
	FSRResourceDepositInstance& OutResourceDeposit) const
{
	OutResourceDeposit = FSRResourceDepositInstance();
	if (!IsValid(SurfaceGrid) || SourceFootprintCellIds.IsEmpty() || ResourceDepositsByOccupantId.IsEmpty())
	{
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> NeighborCellIds;
	for (const FSRPlanetSurfaceGridCellId& FootprintCellId : SourceFootprintCellIds)
	{
		AppendNeighborCellIds(SurfaceGrid, FootprintCellId, NeighborCellIds);
	}

	TSet<FName> VisitedOccupantIds;
	for (const FSRPlanetSurfaceGridCellId& NeighborCellId : NeighborCellIds)
	{
		FSRPlanetSurfaceGridCellInfo NeighborCellInfo;
		if (!SurfaceGrid->GetCellInfoById(NeighborCellId, NeighborCellInfo)
			|| !NeighborCellInfo.bOccupied
			|| NeighborCellInfo.OccupantId.IsNone()
			|| VisitedOccupantIds.Contains(NeighborCellInfo.OccupantId))
		{
			continue;
		}

		VisitedOccupantIds.Add(NeighborCellInfo.OccupantId);
		const FSRResourceDepositInstance* ResourceDeposit = ResourceDepositsByOccupantId.Find(NeighborCellInfo.OccupantId);
		if (!ResourceDeposit
			|| ResourceDeposit->RemainingAmount <= 0
			|| !IsValid(ResourceDeposit->ResourceDataAsset.Get()))
		{
			continue;
		}

		OutResourceDeposit = *ResourceDeposit;
		return true;
	}

	return false;
}

bool USRStructureInstanceManagerComponent::TryHarvestResourceDeposit(
	USRPlanetSurfaceGrid* SurfaceGrid,
	FName DepositOccupantId,
	FSRResourceInstance& OutResourceInstance,
	FSRResourceDepositInstance& OutUpdatedResourceDeposit)
{
	OutResourceInstance = FSRResourceInstance();
	OutUpdatedResourceDeposit = FSRResourceDepositInstance();
	if (!IsValid(SurfaceGrid) || DepositOccupantId.IsNone())
	{
		return false;
	}

	FSRResourceDepositInstance* ResourceDeposit = ResourceDepositsByOccupantId.Find(DepositOccupantId);
	if (!ResourceDeposit
		|| ResourceDeposit->RemainingAmount <= 0
		|| !IsValid(ResourceDeposit->ResourceDataAsset.Get()))
	{
		return false;
	}

	OutResourceInstance = ResourceDeposit->ResourceDataAsset->BuildDefaultInstance();
	OutResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutResourceInstance.StackCount = 1;

	ResourceDeposit->RemainingAmount = FMath::Max(0, ResourceDeposit->RemainingAmount - 1);
	OutUpdatedResourceDeposit = *ResourceDeposit;
	if (ResourceDeposit->RemainingAmount <= 0)
	{
		RemoveStructureByOccupantId(SurfaceGrid, DepositOccupantId);
	}

	return !OutResourceInstance.ResourceId.IsNone();
}

void USRStructureInstanceManagerComponent::RegisterResourceDeposit(
	const FSRPlacedStructureInstance& PlacedStructure,
	const FSRStructureData& StructureData)
{
	ResourceDepositsByOccupantId.Remove(PlacedStructure.OccupantId);
	if (PlacedStructure.OccupantId.IsNone()
		|| !StructureData.bIsResourceDeposit
		|| !IsValid(StructureData.DepositResourceDataAsset.Get())
		|| StructureData.DepositTotalAmount <= 0)
	{
		return;
	}

	FSRResourceDepositInstance ResourceDeposit;
	ResourceDeposit.OccupantId = PlacedStructure.OccupantId;
	ResourceDeposit.StructureId = PlacedStructure.StructureId;
	ResourceDeposit.ResourceDataAsset = StructureData.DepositResourceDataAsset;
	ResourceDeposit.ResourceId = StructureData.DepositResourceDataAsset->ResourceId;
	ResourceDeposit.TotalAmount = FMath::Max(0, StructureData.DepositTotalAmount);
	ResourceDeposit.RemainingAmount = ResourceDeposit.TotalAmount;
	ResourceDepositsByOccupantId.Add(ResourceDeposit.OccupantId, ResourceDeposit);
}
