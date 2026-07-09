#include "SRFacilityCellTemperatureEffectApplier.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

int32 FSRFacilityCellTemperatureEffectApplier::ApplyEffects(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance)
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
	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		double TemperatureDelta = 0.0;
		switch (EffectSpec.EffectKind)
		{
		case ESRFacilityEffectKind::AddCellTemperature:
			TemperatureDelta = EffectSpec.Value;
			break;
		case ESRFacilityEffectKind::SubtractCellTemperature:
			TemperatureDelta = -EffectSpec.Value;
			break;
		default:
			continue;
		}

		FSRPlanetSurfaceGridCellInfo OriginCellInfo;
		if (!SurfaceGrid->GetCellInfoById(FacilityInstance.OriginCellId, OriginCellInfo))
		{
			continue;
		}

		const float NewSurfaceTemperature = FMath::Clamp(
			OriginCellInfo.SurfaceTemperature + static_cast<float>(TemperatureDelta),
			0.0f,
			1.0f);
		if (SurfaceGrid->SetCellSurfaceTemperature(FacilityInstance.OriginCellId, NewSurfaceTemperature))
		{
			++AppliedEffectCount;
		}
	}

	return AppliedEffectCount;
}
