#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr int32 InfiniteResourceDepositAmount = MAX_int32;

	bool IsResourceDepositMineable(const FSRResourceDepositInstance& ResourceDeposit)
	{
		return ResourceDeposit.CanHarvestResource();
	}

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

bool FSRResourceDepositInstance::IsPatternSourceValid() const
{
	return IsValid(ResourceDataAsset.Get())
		&& !ResourceId.IsNone()
		&& !SourcePatternId.IsNone()
		&& SourcePattern.IsCanonical()
		&& !SourcePattern.IsEmpty();
}

bool FSRResourceDepositInstance::CanHarvestResource() const
{
	return IsPatternSourceValid() && RemainingAmount > 0;
}

FSRResourceInstance FSRResourceDepositInstance::BuildResourceInstance(FName ResourceInstanceId) const
{
	if (!IsPatternSourceValid())
	{
		return FSRResourceInstance();
	}

	FSRResourceInstance Result = ResourceDataAsset->BuildInstanceFromPattern(
		SourcePattern,
		SourcePatternSeed,
		SourcePatternId);
	Result.ResourceInstanceId = ResourceInstanceId;
	Result.StackCount = 1;
	return Result;
}

bool FSRResourceDepositInstance::TryHarvestResource(
	FName ResourceInstanceId,
	FSRResourceInstance& OutResourceInstance)
{
	OutResourceInstance = FSRResourceInstance();
	if (!CanHarvestResource())
	{
		return false;
	}

	OutResourceInstance = BuildResourceInstance(ResourceInstanceId);
	if (OutResourceInstance.ResourceId.IsNone())
	{
		return false;
	}

	if (RemainingAmount != InfiniteResourceDepositAmount)
	{
		RemainingAmount = FMath::Max(0, RemainingAmount - 1);
	}
	return true;
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
			|| !IsResourceDepositMineable(*ResourceDeposit))
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
		|| !IsResourceDepositMineable(*ResourceDeposit))
	{
		return false;
	}

	if (!ResourceDeposit->TryHarvestResource(
		FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)),
		OutResourceInstance))
	{
		return false;
	}

	OutUpdatedResourceDeposit = *ResourceDeposit;
	return true;
}

void USRStructureInstanceManagerComponent::RegisterResourceDeposit(
	const FSRPlacedStructureInstance& PlacedStructure,
	const FSRStructureData& StructureData)
{
	ResourceDepositsByOccupantId.Remove(PlacedStructure.OccupantId);
	if (PlacedStructure.OccupantId.IsNone()
		|| !StructureData.bIsResourceDeposit
		|| !IsValid(StructureData.DepositResourceDataAsset.Get()))
	{
		return;
	}

	FSRResourceDepositInstance ResourceDeposit;
	ResourceDeposit.OccupantId = PlacedStructure.OccupantId;
	ResourceDeposit.StructureId = PlacedStructure.StructureId;
	ResourceDeposit.ResourceDataAsset = StructureData.DepositResourceDataAsset;
	ResourceDeposit.ResourceId = StructureData.DepositResourceDataAsset->ResourceId;
	const FString OwnerIdentity = IsValid(GetOwner())
		? GetOwner()->GetPathName()
		: FString(TEXT("NoOwner"));
	ResourceDeposit.SourcePatternId = FName(*FString::Printf(
		TEXT("%s|%s"),
		*OwnerIdentity,
		*PlacedStructure.OccupantId.ToString()));
	ResourceDeposit.SourcePatternSeed = StarRovers::ResourcePatterns::MakeStableSourcePatternSeed(
		ResourceDeposit.SourcePatternId,
		ResourceDeposit.ResourceId,
		StructureData.DepositResourceDataAsset->SourcePatternSeedSalt);
	StructureData.DepositResourceDataAsset->TryGenerateSourcePattern(
		ResourceDeposit.SourcePatternSeed,
		ResourceDeposit.SourcePattern);
	ResourceDeposit.TotalAmount = StructureData.DepositTotalAmount > 0
		? StructureData.DepositTotalAmount
		: InfiniteResourceDepositAmount;
	ResourceDeposit.RemainingAmount = ResourceDeposit.TotalAmount;
	ResourceDepositsByOccupantId.Add(ResourceDeposit.OccupantId, ResourceDeposit);
}
