#include "SRFacilityCellTemperatureEffectApplier.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "SRFacilityEffectConditionEvaluator.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	void AddTemperatureTargetIfValid(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 Distance,
		TArray<FSRPlanetSurfaceGridCellId>& InOutQueue,
		TMap<FSRPlanetSurfaceGridCellId, int32>& InOutDistances)
	{
		if (!IsValid(SurfaceGrid) || InOutDistances.Contains(CellId))
		{
			return;
		}

		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			return;
		}

		InOutDistances.Add(CellId, Distance);
		InOutQueue.Add(CellId);
	}

	void GatherCellsInRange(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& OriginCellId,
		int32 TileRange,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		OutCellIds.Reset();
		if (!IsValid(SurfaceGrid))
		{
			return;
		}

		TArray<FSRPlanetSurfaceGridCellId> Queue;
		TMap<FSRPlanetSurfaceGridCellId, int32> Distances;
		AddTemperatureTargetIfValid(SurfaceGrid, OriginCellId, 0, Queue, Distances);

		const int32 SafeTileRange = FMath::Max(0, TileRange);
		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			const FSRPlanetSurfaceGridCellId CurrentCellId = Queue[QueueIndex];
			const int32 CurrentDistance = Distances.FindRef(CurrentCellId);
			OutCellIds.Add(CurrentCellId);
			if (CurrentDistance >= SafeTileRange)
			{
				continue;
			}

			FSRPlanetSurfaceGridCellNeighbors Neighbors;
			if (!SurfaceGrid->GetCellNeighbors(CurrentCellId, Neighbors))
			{
				continue;
			}

			const int32 NextDistance = CurrentDistance + 1;
			AddTemperatureTargetIfValid(SurfaceGrid, Neighbors.NegativeU, NextDistance, Queue, Distances);
			AddTemperatureTargetIfValid(SurfaceGrid, Neighbors.PositiveU, NextDistance, Queue, Distances);
			AddTemperatureTargetIfValid(SurfaceGrid, Neighbors.NegativeV, NextDistance, Queue, Distances);
			AddTemperatureTargetIfValid(SurfaceGrid, Neighbors.PositiveV, NextDistance, Queue, Distances);
		}
	}

	ESRFacilityTemperatureState ResolveEffectiveTemperatureState(
		ESRFacilityTemperatureState TemperatureState,
		bool bInvertHeat)
	{
		if (!bInvertHeat)
		{
			return TemperatureState;
		}

		switch (TemperatureState)
		{
		case ESRFacilityTemperatureState::Frozen:
			return ESRFacilityTemperatureState::Overheated;
		case ESRFacilityTemperatureState::Cold:
			return ESRFacilityTemperatureState::Hot;
		case ESRFacilityTemperatureState::Hot:
			return ESRFacilityTemperatureState::Cold;
		case ESRFacilityTemperatureState::Overheated:
			return ESRFacilityTemperatureState::Frozen;
		case ESRFacilityTemperatureState::Normal:
		default:
			return ESRFacilityTemperatureState::Normal;
		}
	}
}

int32 FSRFacilityCellTemperatureEffectApplier::ApplyEffects(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance,
	const FSRResourceInstance* ConditionResource,
	const FSRResourceInstance* BaselineResource)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return 0;
	}

	const AActor* Owner = IsValid(OwnerComponent) ? OwnerComponent->GetOwner() : nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	if (!IsValid(SurfaceGrid))
	{
		return 0;
	}

	int32 AppliedEffectCount = 0;
	bool bInvertHeat = false;
	ESRFacilityTemperatureState ConditionTemperatureState = FacilityInstance.TemperatureState;
	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		const ESRFacilityTemperatureState EffectiveTemperatureState = ResolveEffectiveTemperatureState(
			ConditionTemperatureState,
			bInvertHeat);
		const StarRovers::FacilityEffects::FSRFacilityEffectConditionContext ConditionContext =
		{
			ConditionResource,
			BaselineResource,
			EffectiveTemperatureState
		};
		if (!StarRovers::FacilityEffects::DoEffectConditionsPass(EffectSpec, ConditionContext))
		{
			continue;
		}

		if (EffectSpec.EffectKind == ESRFacilityEffectKind::InvertHeat)
		{
			bInvertHeat = !bInvertHeat;
			continue;
		}
		if (EffectSpec.EffectKind == ESRFacilityEffectKind::OverrideProcessTemperature)
		{
			bInvertHeat = false;
			ConditionTemperatureState = EffectSpec.ProcessTemperatureState;
			continue;
		}

		if (EffectSpec.EffectKind != ESRFacilityEffectKind::AdjustCellTemperature)
		{
			continue;
		}

		TArray<FSRPlanetSurfaceGridCellId> TargetCellIds;
		GatherCellsInRange(SurfaceGrid, FacilityInstance.OriginCellId, EffectSpec.TileRange, TargetCellIds);
		for (const FSRPlanetSurfaceGridCellId& TargetCellId : TargetCellIds)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(TargetCellId, CellInfo))
			{
				continue;
			}

			const double TemperatureDelta = bInvertHeat ? -EffectSpec.Value : EffectSpec.Value;
			const float NewSurfaceTemperature = FMath::Clamp(
				CellInfo.SurfaceTemperature + static_cast<float>(TemperatureDelta),
				0.0f,
				1.0f);
			if (SurfaceGrid->SetCellSurfaceTemperature(TargetCellId, NewSurfaceTemperature))
			{
				++AppliedEffectCount;
			}
		}
	}

	return AppliedEffectCount;
}
