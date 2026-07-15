#include "SRFacilityCellTemperatureEffectApplier.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "SRFacilityEffectConditionEvaluator.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	USRPlanetSurfaceGrid* ResolveSurfaceGrid(const UActorComponent* OwnerComponent)
	{
		const AActor* Owner = IsValid(OwnerComponent) ? OwnerComponent->GetOwner() : nullptr;
		return IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	}

	bool GatherCellsInTileRange(
		const USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CenterCellId,
		int32 TileRange,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		OutCellIds.Reset();
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		return SurfaceGrid->GetInteractionGridPatchCellIdsWithSize(
			CenterCellId,
			FMath::Max(1, TileRange),
			OutCellIds);
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

	int32 TemperatureStateToStep(ESRFacilityTemperatureState TemperatureState)
	{
		switch (TemperatureState)
		{
		case ESRFacilityTemperatureState::Frozen:
			return 0;
		case ESRFacilityTemperatureState::Cold:
			return 1;
		case ESRFacilityTemperatureState::Hot:
			return 3;
		case ESRFacilityTemperatureState::Overheated:
			return 4;
		case ESRFacilityTemperatureState::Normal:
		default:
			return 2;
		}
	}

	ESRFacilityTemperatureState StepToTemperatureState(int32 TemperatureStep)
	{
		switch (FMath::Clamp(TemperatureStep, 0, 4))
		{
		case 0:
			return ESRFacilityTemperatureState::Frozen;
		case 1:
			return ESRFacilityTemperatureState::Cold;
		case 3:
			return ESRFacilityTemperatureState::Hot;
		case 4:
			return ESRFacilityTemperatureState::Overheated;
		case 2:
		default:
			return ESRFacilityTemperatureState::Normal;
		}
	}

	ESRFacilityTemperatureState AdjustTemperatureStateByStep(
		ESRFacilityTemperatureState TemperatureState,
		int32 StepDelta,
		int32& OutAppliedStepDelta)
	{
		const int32 OldStep = TemperatureStateToStep(TemperatureState);
		const int32 NewStep = FMath::Clamp(OldStep + StepDelta, 0, 4);
		OutAppliedStepDelta = NewStep - OldStep;
		return StepToTemperatureState(NewStep);
	}
}

int32 FSRFacilityCellTemperatureEffectApplier::ApplyInstallationEffects(
	const UActorComponent* OwnerComponent,
	FSRFacilityInstance& FacilityInstance)
{
	FacilityInstance.CellTemperatureAdjustments.Reset();

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return 0;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = ResolveSurfaceGrid(OwnerComponent);
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
			nullptr,
			nullptr,
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
		if (!GatherCellsInTileRange(SurfaceGrid, FacilityInstance.OriginCellId, EffectSpec.TileRange, TargetCellIds))
		{
			continue;
		}

		for (const FSRPlanetSurfaceGridCellId& TargetCellId : TargetCellIds)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(TargetCellId, CellInfo))
			{
				continue;
			}

			const int32 RequestedStepDelta = bInvertHeat
				? -EffectSpec.TemperatureStepDelta
				: EffectSpec.TemperatureStepDelta;
			int32 AppliedStepDelta = 0;
			const ESRFacilityTemperatureState NewTemperatureState = AdjustTemperatureStateByStep(
				CellInfo.TemperatureState,
				RequestedStepDelta,
				AppliedStepDelta);
			if (AppliedStepDelta != 0
				&& SurfaceGrid->SetCellTemperatureState(TargetCellId, NewTemperatureState))
			{
				FSRFacilityCellTemperatureAdjustment& Adjustment = FacilityInstance.CellTemperatureAdjustments.AddDefaulted_GetRef();
				Adjustment.CellId = TargetCellId;
				Adjustment.TemperatureStepDelta = AppliedStepDelta;
				++AppliedEffectCount;
			}
		}
	}

	return AppliedEffectCount;
}

int32 FSRFacilityCellTemperatureEffectApplier::RemoveInstallationEffects(
	const UActorComponent* OwnerComponent,
	FSRFacilityInstance& FacilityInstance)
{
	if (FacilityInstance.CellTemperatureAdjustments.IsEmpty())
	{
		return 0;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = ResolveSurfaceGrid(OwnerComponent);
	if (!IsValid(SurfaceGrid))
	{
		FacilityInstance.CellTemperatureAdjustments.Reset();
		return 0;
	}

	int32 RemovedEffectCount = 0;
	for (int32 AdjustmentIndex = FacilityInstance.CellTemperatureAdjustments.Num() - 1; AdjustmentIndex >= 0; --AdjustmentIndex)
	{
		const FSRFacilityCellTemperatureAdjustment& Adjustment = FacilityInstance.CellTemperatureAdjustments[AdjustmentIndex];
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(Adjustment.CellId, CellInfo))
		{
			continue;
		}

		int32 RestoredStepDelta = 0;
		const ESRFacilityTemperatureState RestoredTemperatureState = AdjustTemperatureStateByStep(
			CellInfo.TemperatureState,
			-Adjustment.TemperatureStepDelta,
			RestoredStepDelta);
		if (RestoredStepDelta != 0
			&& SurfaceGrid->SetCellTemperatureState(Adjustment.CellId, RestoredTemperatureState))
		{
			++RemovedEffectCount;
		}
	}

	FacilityInstance.CellTemperatureAdjustments.Reset();
	return RemovedEffectCount;
}
