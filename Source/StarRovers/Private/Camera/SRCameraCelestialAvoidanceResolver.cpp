#include "SRCameraCelestialAvoidanceResolver.h"

#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

float FSRCameraCelestialAvoidanceResolver::ComputeScaledBodyRadius(const AActor* Actor)
{
	if (const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(Actor))
	{
		const FSRCelestialBodyData BodyData = CelestialBody->GetData();
		if (IsValid(BodyData.StaticMesh.Get()))
		{
			return BodyData.StaticMesh->GetBounds().SphereRadius * FMath::Max(0.0f, BodyData.Scale);
		}

		return IsValid(BodyData.DynamicMeshBaseDataAsset.Get())
			? BodyData.DynamicMeshBaseDataAsset->GetSafeBaseRadius() * FMath::Max(0.0f, BodyData.Scale)
			: 0.0f;
	}

	return 0.0f;
}

bool FSRCameraCelestialAvoidanceResolver::ResolveAvoidanceSphere(
	const AActor* Actor,
	float CameraSurfacePadding,
	FVector& OutCenter,
	float& OutRadius)
{
	OutCenter = FVector::ZeroVector;
	OutRadius = 0.0f;

	if (!USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(Actor))
	{
		return false;
	}

	if (USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(Actor))
	{
		return false;
	}

	OutRadius = ComputeScaledBodyRadius(Actor);
	if (OutRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutCenter = Actor->GetActorLocation();

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents(PrimitiveComponents);
	float BestRadius = 0.0f;
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent)
			|| !PrimitiveComponent->IsVisible()
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.GravityLine"))
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.GravityLineRoot"))
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.GravityLineSegment"))
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.RotationAxisLine"))
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.RotationAxisLineRoot")))
		{
			continue;
		}

		const float CandidateRadius = PrimitiveComponent->Bounds.SphereRadius;
		if (CandidateRadius > BestRadius)
		{
			BestRadius = CandidateRadius;
			OutCenter = PrimitiveComponent->Bounds.Origin;
		}
	}

	OutRadius += FMath::Max(0.0f, CameraSurfacePadding);
	return true;
}

float FSRCameraCelestialAvoidanceResolver::ClampZoomDistanceAgainstBodies(
	float ZoomDistance,
	const FVector& CandidatePawnLocation,
	const FVector& CameraDirection,
	const TArray<AActor*>& CelestialBodies,
	const AActor* ExcludedActor,
	float CameraSurfacePadding)
{
	float AdjustedZoomDistance = FMath::Max(0.0f, ZoomDistance);

	constexpr int32 MaxAvoidancePasses = 3;
	for (int32 PassIndex = 0; PassIndex < MaxAvoidancePasses; ++PassIndex)
	{
		bool bAdjustedThisPass = false;
		for (const AActor* CandidateBody : CelestialBodies)
		{
			if (!IsValid(CandidateBody) || CandidateBody == ExcludedActor)
			{
				continue;
			}

			FVector BodyCenter = FVector::ZeroVector;
			float BodyRadius = 0.0f;
			if (!ResolveAvoidanceSphere(CandidateBody, CameraSurfacePadding, BodyCenter, BodyRadius))
			{
				continue;
			}

			const FVector PivotToBody = CandidatePawnLocation - BodyCenter;
			const float B = FVector::DotProduct(PivotToBody, CameraDirection);
			const float C = PivotToBody.SizeSquared() - FMath::Square(BodyRadius);
			const float Discriminant = FMath::Square(B) - C;
			if (Discriminant < 0.0f)
			{
				continue;
			}

			const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
			const float EntryDistance = -B - SqrtDiscriminant;
			const float ExitDistance = -B + SqrtDiscriminant;
			if (ExitDistance < 0.0f)
			{
				continue;
			}

			constexpr float BoundaryTolerance = 0.1f;
			const float SafeEntryDistance = FMath::Max(0.0f, EntryDistance);
			const bool bPivotIsInsideBody = EntryDistance <= 0.0f;
			const bool bCameraWouldBeInsideBody = bPivotIsInsideBody
				? AdjustedZoomDistance < ExitDistance - BoundaryTolerance
				: AdjustedZoomDistance > EntryDistance + BoundaryTolerance && AdjustedZoomDistance < ExitDistance - BoundaryTolerance;
			if (!bCameraWouldBeInsideBody)
			{
				continue;
			}

			AdjustedZoomDistance = EntryDistance > 0.0f ? SafeEntryDistance : ExitDistance;
			bAdjustedThisPass = true;
		}

		if (!bAdjustedThisPass)
		{
			break;
		}
	}

	return AdjustedZoomDistance;
}
