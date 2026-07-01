#include "Celestial/SRCelestialBodyRuntimeLibrary.h"

#include "SRCelestialBodyRuntimeLibraryInternal.h"

#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Celestial/SRStar.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

using namespace StarRovers::CelestialBodyRuntime;

namespace StarRovers::CelestialBodyRuntime
{
	namespace ComponentTags
	{
		const FName GravityLine(TEXT("StarRovers.GravityLine"));
		const FName GravityLineRoot(TEXT("StarRovers.GravityLineRoot"));
		const FName GravityLineSegment(TEXT("StarRovers.GravityLineSegment"));
		const FName OrbitLine(TEXT("StarRovers.OrbitLine"));
		const FName OrbitLineRoot(TEXT("StarRovers.OrbitLineRoot"));
		const FName RotationAxisLine(TEXT("StarRovers.RotationAxisLine"));
		const FName RotationAxisLineRoot(TEXT("StarRovers.RotationAxisLineRoot"));
	}

	namespace
	{
		bool IsExcludedCelestialBodyComponent(const UPrimitiveComponent* PrimitiveComponent)
		{
			if (!IsValid(PrimitiveComponent))
			{
				return true;
			}

			return PrimitiveComponent->ComponentHasTag(ComponentTags::GravityLine)
				|| PrimitiveComponent->ComponentHasTag(ComponentTags::GravityLineRoot)
				|| PrimitiveComponent->ComponentHasTag(ComponentTags::GravityLineSegment)
				|| PrimitiveComponent->ComponentHasTag(ComponentTags::OrbitLine)
				|| PrimitiveComponent->ComponentHasTag(ComponentTags::OrbitLineRoot)
				|| PrimitiveComponent->ComponentHasTag(ComponentTags::RotationAxisLine)
				|| PrimitiveComponent->ComponentHasTag(ComponentTags::RotationAxisLineRoot);
		}

		float GetLargestPrimitiveRadius(const AActor* Actor)
		{
			if (!IsValid(Actor))
			{
				return 0.0f;
			}

			float LargestRadius = 0.0f;

			TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
			Actor->GetComponents(PrimitiveComponents);
			for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
			{
				if (!IsValid(PrimitiveComponent) || IsExcludedCelestialBodyComponent(PrimitiveComponent))
				{
					continue;
				}

				if (!PrimitiveComponent->IsVisible())
				{
					continue;
				}

				LargestRadius = FMath::Max(LargestRadius, PrimitiveComponent->Bounds.SphereRadius);
			}

			if (LargestRadius > KINDA_SMALL_NUMBER)
			{
				return LargestRadius;
			}

			if (const UStaticMeshComponent* RootStaticMeshComponent = Cast<UStaticMeshComponent>(Actor->GetRootComponent()))
			{
				if (!IsExcludedCelestialBodyComponent(RootStaticMeshComponent)
					&& IsValid(RootStaticMeshComponent->GetStaticMesh())
					&& RootStaticMeshComponent->Bounds.SphereRadius > KINDA_SMALL_NUMBER)
				{
					return RootStaticMeshComponent->Bounds.SphereRadius;
				}
			}

			return 0.0f;
		}
	}

	float GetScaledBodyRadius(const AActor* Actor)
	{
		if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
		{
			const FSRCelestialBodyData BodyData = ProceduralBody->GetData();
			if (IsValid(BodyData.StaticMesh.Get()))
			{
				return BodyData.StaticMesh->GetBounds().SphereRadius * FMath::Max(0.0f, BodyData.Scale);
			}
			return IsValid(BodyData.DynamicMeshBaseDataAsset.Get())
				? BodyData.DynamicMeshBaseDataAsset->GetSafeBaseRadius() * FMath::Max(0.0f, BodyData.Scale)
				: 0.0f;
		}

		return GetLargestPrimitiveRadius(Actor);
	}
}

float USRCelestialBodyRuntimeLibrary::GetCelestialFocusZoomDistance(const AActor* Actor, float CameraFieldOfViewDegrees, float FramingPadding)
{
	if (!IsValid(Actor))
	{
		LogMissingCelestialData(Actor, TEXT("actor"));
		return 0.0f;
	}

	const float SafeFieldOfViewDegrees = FMath::Clamp(CameraFieldOfViewDegrees, 5.0f, 170.0f);
	float FocusZoomMultiplier = 0.0f;
	if (!TryGetFloatPropertyValue(Actor, PropertyNames::FocusZoomMultiplier, FocusZoomMultiplier))
	{
		LogMissingCelestialData(Actor, TEXT("FocusZoomMultiplier"));
		return 0.0f;
	}
	FocusZoomMultiplier = FMath::Max(0.0f, FocusZoomMultiplier);

	const float ScaledBodyRadius = GetScaledBodyRadius(Actor);
	if (ScaledBodyRadius > KINDA_SMALL_NUMBER)
	{
		const float HalfFieldOfViewRadians = FMath::DegreesToRadians(SafeFieldOfViewDegrees * 0.5f);
		const float FramedDistance = ScaledBodyRadius / FMath::Tan(HalfFieldOfViewRadians);
		return FMath::Max(0.0f, FramedDistance * FocusZoomMultiplier);
	}

	LogMissingCelestialData(Actor, TEXT("ScaledBodyRadius"));
	return 0.0f;
}

FText USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(const AActor* Actor)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		const FText VariableName = ProceduralBody->GetData().VariableName;
		if (!VariableName.IsEmpty())
		{
			return VariableName;
		}
	}

	FText VariableName;
	if (TryGetTextLikePropertyValue(Actor, PropertyNames::VariableName, VariableName) && !VariableName.IsEmpty())
	{
		return VariableName;
	}

	LogMissingCelestialData(Actor, TEXT("VariableName"));
	return FText::GetEmpty();
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(const AActor* Actor)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		return ProceduralBody->GetData().bCanConstruct;
	}

	bool bCanConstruct = false;
	TryGetBoolPropertyValue(Actor, PropertyNames::CanConstruct, bCanConstruct);
	return bCanConstruct;
}

float USRCelestialBodyRuntimeLibrary::GetScreenScale(
	const AActor* Actor,
	const FVector& CameraLocation,
	const FVector& CameraForward,
	float CameraFieldOfViewDegrees)
{
	if (!IsValid(Actor))
	{
		LogMissingCelestialData(Actor, TEXT("actor"));
		return 0.0f;
	}

	const FVector SafeCameraForward = CameraForward.GetSafeNormal();
	if (SafeCameraForward.IsNearlyZero())
	{
		return 0.0f;
	}

	const FVector CameraToBody = Actor->GetActorLocation() - CameraLocation;
	const float Depth = FVector::DotProduct(CameraToBody, SafeCameraForward);
	if (Depth <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float ScaledBodyRadius = GetScaledBodyRadius(Actor);
	if (ScaledBodyRadius <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float SafeFieldOfViewDegrees = FMath::Clamp(CameraFieldOfViewDegrees, 5.0f, 170.0f);
	const float TanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(SafeFieldOfViewDegrees * 0.5f));
	if (TanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(ScaledBodyRadius / (Depth * TanHalfFieldOfView), 0.0f, BIG_NUMBER);
}

USRPlanetSurfaceGrid* USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(const AActor* Actor)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		return ProceduralBody->GetSurfaceGrid();
	}

	return IsValid(Actor)
		? Actor->FindComponentByClass<USRPlanetSurfaceGrid>()
		: nullptr;
}

FSRCelestialBodyFocusInfo USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(const AActor* Actor)
{
	FSRCelestialBodyFocusInfo FocusInfo;
	if (!IsCelestialBodyActor(Actor))
	{
		return FocusInfo;
	}

	FocusInfo.Actor = const_cast<AActor*>(Actor);
	FocusInfo.VariableName = GetCelestialVariableName(Actor);
	FocusInfo.bCanConstruct = GetCelestialCanConstruct(Actor);
	if (const ASRStar* Star = Cast<ASRStar>(Actor))
	{
		const FSRStellarFuelState FuelState = Star->GetStellarFuelState();
		FocusInfo.bHasStarFuelInfo = true;
		FocusInfo.StarFuelInfo.bIsValid = true;
		FocusInfo.StarFuelInfo.StoredFuel = FuelState.StoredFuel;
		FocusInfo.StarFuelInfo.RequiredFuelPerCycle = FuelState.RequiredFuelPerCycle;
		FocusInfo.StarFuelInfo.RequirementGrowthPerCycle = FuelState.RequirementGrowthPerCycle;
		FocusInfo.StarFuelInfo.RedGiantPressure = FuelState.RedGiantPressure;
		FocusInfo.StarFuelInfo.RedGiantPressurePerMissingFuel = FuelState.RedGiantPressurePerMissingFuel;
		FocusInfo.StarFuelInfo.LastSettledCycleIndex = FuelState.LastSettledCycleIndex;
		FocusInfo.StarFuelInfo.LastCycleFuelConsumed = FuelState.LastCycleFuelConsumed;
		FocusInfo.StarFuelInfo.LastCycleFuelDeficit = FuelState.LastCycleFuelDeficit;
		FocusInfo.StarFuelInfo.bLastCycleMetRequirement = FuelState.bLastCycleMetRequirement;
	}
	if (USRPlanetSurfaceGrid* SurfaceGrid = FindPlanetSurfaceGrid(Actor))
	{
		FocusInfo.bHasSurfaceGrid = true;
		FocusInfo.bHasHoveredSurfaceCell = SurfaceGrid->GetHoveredCellInfo(FocusInfo.HoveredSurfaceCellInfo);
		if (FocusInfo.bHasHoveredSurfaceCell)
		{
			SurfaceGrid->GetInteractionGridPatchCellIds(
				FocusInfo.HoveredSurfaceCellInfo.CellId,
				FocusInfo.HoveredSurfaceGridPatchCellIds);
		}
	}
	FocusInfo.bIsValid = true;
	return FocusInfo;
}
