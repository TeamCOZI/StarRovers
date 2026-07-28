#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Automation/SRResourceInstanceOperations.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr int32 InfiniteResourceDepositAmount = MAX_int32;

	bool IsResourceDepositMineable(const FSRResourceDepositInstance& ResourceDeposit)
	{
		return IsValid(ResourceDeposit.ResourceDataAsset.Get())
			&& FSRResourceDepositAmountModel::CanHarvest(ResourceDeposit.RemainingAmount);
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

	bool AreOccupantIdSetsEqual(const TSet<FName>& Left, const TSet<FName>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (const FName OccupantId : Left)
		{
			if (!Right.Contains(OccupantId))
			{
				return false;
			}
		}
		return true;
	}

	void AppendOccupantIds(TSet<FName>& OutOccupantIds, const TSet<FName>& OccupantIds)
	{
		for (const FName OccupantId : OccupantIds)
		{
			OutOccupantIds.Add(OccupantId);
		}
	}
}

int32 FSRResourceDepositAmountModel::ResolveInitialAmount(int32 AuthoredTotalAmount)
{
	// Zero remains an explicit Legacy-infinite contract. Resource V2 deposits
	// author a positive amount and therefore use finite depletion.
	return AuthoredTotalAmount > 0
		? AuthoredTotalAmount
		: InfiniteResourceDepositAmount;
}

bool FSRResourceDepositAmountModel::IsInfinite(int32 Amount)
{
	return Amount >= InfiniteResourceDepositAmount;
}

bool FSRResourceDepositAmountModel::CanHarvest(int32 RemainingAmount)
{
	return RemainingAmount > 0;
}

bool FSRResourceDepositAmountModel::TryConsumeOne(
	int32 TotalAmount,
	int32& InOutRemainingAmount)
{
	if (!CanHarvest(InOutRemainingAmount))
	{
		return false;
	}
	if (!IsInfinite(TotalAmount) && !IsInfinite(InOutRemainingAmount))
	{
		--InOutRemainingAmount;
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

void USRStructureInstanceManagerComponent::GetResourceDepositInstances(
	TArray<FSRResourceDepositInstance>& OutResourceDeposits) const
{
	OutResourceDeposits.Reset();
	ResourceDepositsByOccupantId.GenerateValueArray(OutResourceDeposits);
	OutResourceDeposits.Sort(
		[](const FSRResourceDepositInstance& Left, const FSRResourceDepositInstance& Right)
		{
			return Left.OccupantId.LexicalLess(Right.OccupantId);
		});
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

void USRStructureInstanceManagerComponent::SetMiningResourceDepositHighlights(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& MinerFootprintCellIds)
{
	TSet<FName> NewHighlightedDepositOccupantIds;
	NewHighlightedDepositOccupantIds.Reserve(ResourceDepositsByOccupantId.Num());
	for (const TPair<FName, FSRResourceDepositInstance>& DepositPair : ResourceDepositsByOccupantId)
	{
		if (PlacedStructuresByOccupantId.Contains(DepositPair.Key)
			&& IsResourceDepositMineable(DepositPair.Value))
		{
			NewHighlightedDepositOccupantIds.Add(DepositPair.Key);
		}
	}

	FName NewTargetDepositOccupantId = NAME_None;
	FSRResourceDepositInstance AdjacentDeposit;
	if (FindAdjacentResourceDeposit(SurfaceGrid, MinerFootprintCellIds, AdjacentDeposit))
	{
		NewTargetDepositOccupantId = AdjacentDeposit.OccupantId;
	}

	if (AreOccupantIdSetsEqual(MiningHighlightedResourceDepositOccupantIds, NewHighlightedDepositOccupantIds)
		&& MiningTargetResourceDepositOccupantId == NewTargetDepositOccupantId)
	{
		return;
	}

	TSet<FName> AffectedOccupantIds;
	AppendOccupantIds(AffectedOccupantIds, MiningHighlightedResourceDepositOccupantIds);
	AppendOccupantIds(AffectedOccupantIds, NewHighlightedDepositOccupantIds);
	MiningHighlightedResourceDepositOccupantIds = MoveTemp(NewHighlightedDepositOccupantIds);
	MiningTargetResourceDepositOccupantId = NewTargetDepositOccupantId;
	RefreshVisualInstancesForOccupants(AffectedOccupantIds);

	for (const FName OccupantId : AffectedOccupantIds)
	{
		if (const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId))
		{
			RefreshStructureNameLabel(SurfaceGrid, *PlacedStructure);
		}
	}
}

void USRStructureInstanceManagerComponent::ClearMiningResourceDepositHighlights(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (MiningHighlightedResourceDepositOccupantIds.IsEmpty()
		&& MiningTargetResourceDepositOccupantId.IsNone())
	{
		return;
	}

	if (!IsValid(SurfaceGrid))
	{
		if (AActor* OwnerActor = GetOwner())
		{
			SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
		}
	}

	TSet<FName> AffectedOccupantIds = MoveTemp(MiningHighlightedResourceDepositOccupantIds);
	MiningHighlightedResourceDepositOccupantIds.Reset();
	MiningTargetResourceDepositOccupantId = NAME_None;
	RefreshVisualInstancesForOccupants(AffectedOccupantIds);

	for (const FName OccupantId : AffectedOccupantIds)
	{
		if (const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId))
		{
			RefreshStructureNameLabel(SurfaceGrid, *PlacedStructure);
		}
	}
}

bool USRStructureInstanceManagerComponent::IsMiningResourceDepositHighlighted(FName OccupantId) const
{
	return !OccupantId.IsNone() && MiningHighlightedResourceDepositOccupantIds.Contains(OccupantId);
}

bool USRStructureInstanceManagerComponent::IsMiningResourceDepositTarget(FName OccupantId) const
{
	return !OccupantId.IsNone() && MiningTargetResourceDepositOccupantId == OccupantId;
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

	OutResourceInstance = ResourceDeposit->ResourceDataAsset->BuildDefaultInstance();
	OutResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	OutResourceInstance.StackCount = 1;
	StarRovers::Resources::InitializeResourceOrigin(
		OutResourceInstance,
		StarRovers::Resources::ResolveCelestialBodyResourceId(GetOwner()));
	if (!FSRResourceDepositAmountModel::TryConsumeOne(
		ResourceDeposit->TotalAmount,
		ResourceDeposit->RemainingAmount))
	{
		OutResourceInstance = FSRResourceInstance();
		return false;
	}

	OutUpdatedResourceDeposit = *ResourceDeposit;
	return !OutResourceInstance.ResourceId.IsNone();
}

bool USRStructureInstanceManagerComponent::TryConfigureResourceDepositAmount(
	FName DepositOccupantId,
	int32 TotalAmount,
	int32 RemainingAmount,
	FSRResourceDepositInstance& OutUpdatedResourceDeposit)
{
	OutUpdatedResourceDeposit = FSRResourceDepositInstance();
	FSRResourceDepositInstance* ResourceDeposit =
		ResourceDepositsByOccupantId.Find(DepositOccupantId);
	if (!ResourceDeposit || !IsValid(ResourceDeposit->ResourceDataAsset.Get()))
	{
		return false;
	}

	const int32 SafeTotalAmount = FMath::Max(1, TotalAmount);
	ResourceDeposit->TotalAmount = SafeTotalAmount;
	ResourceDeposit->RemainingAmount = FMath::Clamp(
		RemainingAmount,
		0,
		SafeTotalAmount);
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
	ResourceDeposit.TotalAmount =
		FSRResourceDepositAmountModel::ResolveInitialAmount(
			StructureData.DepositTotalAmount);
	ResourceDeposit.RemainingAmount = ResourceDeposit.TotalAmount;
	ResourceDepositsByOccupantId.Add(ResourceDeposit.OccupantId, ResourceDeposit);
}
