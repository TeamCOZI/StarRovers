#include "Camera/SRCameraPawn.h"

#include "SRCameraPawnInternal.h"
#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"

namespace
{
	constexpr double SpaceBoundaryFullScanIntervalSeconds = 1.0;

	bool IsSpaceBoundaryActor(const AActor* Candidate)
	{
		if (!IsValid(Candidate))
		{
			return false;
		}

		const FString CandidateName = Candidate->GetName();
		const FString CandidateClassName = Candidate->GetClass() ? Candidate->GetClass()->GetName() : FString();
		return CandidateName.Contains(TEXT("SpaceSphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Contains(TEXT("SpaceSkySphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Contains(TEXT("Space Sky Sphere"), ESearchCase::IgnoreCase)
			|| CandidateName.Equals(TEXT("BP_Space"), ESearchCase::IgnoreCase)
			|| CandidateName.StartsWith(TEXT("BP_Space_"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Contains(TEXT("SpaceSphere"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Contains(TEXT("SpaceSkySphere"), ESearchCase::IgnoreCase)
			|| CandidateClassName.Equals(TEXT("BP_Space_C"), ESearchCase::IgnoreCase);
	}

	bool TryResolveSpaceBoundaryActor(const AActor* Candidate, FVector& OutCenter, float& OutRadius)
	{
		OutCenter = FVector::ZeroVector;
		OutRadius = 0.0f;

		if (!IsSpaceBoundaryActor(Candidate))
		{
			return false;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Candidate);
		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsValid(PrimitiveComponent))
			{
				continue;
			}

			const float CandidateRadius = PrimitiveComponent->Bounds.SphereRadius;
			if (CandidateRadius > OutRadius)
			{
				OutCenter = PrimitiveComponent->Bounds.Origin;
				OutRadius = CandidateRadius;
			}
		}

		if (OutRadius <= KINDA_SMALL_NUMBER)
		{
			FVector ActorOrigin = FVector::ZeroVector;
			FVector ActorExtent = FVector::ZeroVector;
			Candidate->GetActorBounds(false, ActorOrigin, ActorExtent);
			const float ActorRadius = ActorExtent.Size();
			if (ActorRadius > OutRadius)
			{
				OutCenter = ActorOrigin;
				OutRadius = ActorRadius;
			}
		}

		return OutRadius > KINDA_SMALL_NUMBER;
	}
}
float ASRCameraPawn::GetMaxZoomDistance() const
{
	const float SpaceSphereRadius = GetSpaceSphereRadius();
	return SpaceSphereRadius > KINDA_SMALL_NUMBER ? SpaceSphereRadius : BIG_NUMBER;
}

float ASRCameraPawn::GetScreenSpaceInputScale(float CurrentZoomDistance) const
{
	const float ReferenceZoomDistance = FMath::Max(1.0f, GetScreenSpaceThicknessReferenceZoomDistance());
	const float SafeCurrentZoomDistance = FMath::Max(1.0f, CurrentZoomDistance);
	const float ReferenceFieldOfView = FMath::Clamp(GetScreenSpaceThicknessReferenceFieldOfView(), 5.0f, 170.0f);
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera to compute screen-space input scale."));
		return 1.0f;
	}

	const float CurrentFieldOfView = FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
	const float ReferenceTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(ReferenceFieldOfView * 0.5f));
	const float CurrentTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(CurrentFieldOfView * 0.5f));
	if (ReferenceTanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return 1.0f;
	}

	const float ScreenSpaceInputScale = (SafeCurrentZoomDistance * CurrentTanHalfFieldOfView)
		/ (ReferenceZoomDistance * ReferenceTanHalfFieldOfView);
	return FMath::Max(ScreenSpaceInputScale, UE_SMALL_NUMBER);
}

float ASRCameraPawn::GetZoomSpeed() const
{
	const float SafeBaseZoomSpeed = FMath::Max(0.0f, ZoomSpeed);
	if (SafeBaseZoomSpeed <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return SafeBaseZoomSpeed * GetScreenSpaceInputScale(
		ZoomDistanceTarget) * FMath::Max(0.0f, ZoomInputScaleMultiplier);
}

float ASRCameraPawn::GetMinimumZoomDistance() const
{
	if (!IsValid(FocusedActor))
	{
		return 0.0f;
	}

	FVector BodyCenter = FVector::ZeroVector;
	float BodyRadius = 0.0f;
	if (!ResolveCelestialCameraAvoidanceSphere(FocusedActor, BodyCenter, BodyRadius))
	{
		return 0.0f;
	}

	return BodyRadius;
}

float ASRCameraPawn::GetSpaceSphereRadius() const
{
	FVector SpaceCenter = FVector::ZeroVector;
	float SpaceRadius = 0.0f;
	return ResolveSpaceBoundary(SpaceCenter, SpaceRadius) ? SpaceRadius : 0.0f;
}

float ASRCameraPawn::ClampZoomDistance(float ZoomDistance) const
{
	const float MaximumZoomDistance = GetMaxZoomDistance();
	if (!FMath::IsFinite(MaximumZoomDistance))
	{
		return FMath::Max(0.0f, ZoomDistance);
	}

	const float MinimumZoomDistance = FMath::Max(0.0f, GetMinimumZoomDistance());
	const float SafeMaximumZoomDistance = FMath::Max(MinimumZoomDistance, MaximumZoomDistance);
	return FMath::Clamp(ZoomDistance, MinimumZoomDistance, SafeMaximumZoomDistance);
}

bool ASRCameraPawn::ResolveSpaceBoundary(FVector& OutCenter, float& OutRadius) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		bHasCachedSpaceBoundaryResult = false;
		bCachedSpaceBoundaryFound = false;
		CachedSpaceBoundaryActor = nullptr;
		CachedSpaceBoundaryCenter = FVector::ZeroVector;
		CachedSpaceBoundaryRadius = 0.0f;
		CachedSpaceBoundaryFrame = 0;
		CachedSpaceBoundaryFullScanTime = -BIG_NUMBER;
		return false;
	}

	OutCenter = FVector::ZeroVector;
	OutRadius = 0.0f;

	if (bHasCachedSpaceBoundaryResult && CachedSpaceBoundaryFrame == GFrameCounter)
	{
		if (!bCachedSpaceBoundaryFound)
		{
			return false;
		}

		OutCenter = CachedSpaceBoundaryCenter;
		OutRadius = CachedSpaceBoundaryRadius;
		return OutRadius > KINDA_SMALL_NUMBER;
	}

	if (bCachedSpaceBoundaryFound && IsValid(CachedSpaceBoundaryActor.Get()))
	{
		if (TryResolveSpaceBoundaryActor(CachedSpaceBoundaryActor.Get(), OutCenter, OutRadius))
		{
			CachedSpaceBoundaryCenter = OutCenter;
			CachedSpaceBoundaryRadius = OutRadius;
			CachedSpaceBoundaryFrame = GFrameCounter;
			bHasCachedSpaceBoundaryResult = true;
			return true;
		}
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (bHasCachedSpaceBoundaryResult
		&& !bCachedSpaceBoundaryFound
		&& CurrentTime - CachedSpaceBoundaryFullScanTime < SpaceBoundaryFullScanIntervalSeconds)
	{
		CachedSpaceBoundaryFrame = GFrameCounter;
		return false;
	}

	AActor* BestBoundaryActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == this)
		{
			continue;
		}

		FVector CandidateCenter = FVector::ZeroVector;
		float CandidateRadius = 0.0f;
		if (TryResolveSpaceBoundaryActor(Candidate, CandidateCenter, CandidateRadius)
			&& CandidateRadius > OutRadius)
		{
			OutCenter = CandidateCenter;
			OutRadius = CandidateRadius;
			BestBoundaryActor = const_cast<AActor*>(Candidate);
		}
	}

	CachedSpaceBoundaryActor = BestBoundaryActor;
	CachedSpaceBoundaryCenter = OutCenter;
	CachedSpaceBoundaryRadius = OutRadius;
	CachedSpaceBoundaryFullScanTime = CurrentTime;
	CachedSpaceBoundaryFrame = GFrameCounter;
	bCachedSpaceBoundaryFound = OutRadius > KINDA_SMALL_NUMBER;
	bHasCachedSpaceBoundaryResult = true;
	return bCachedSpaceBoundaryFound;
}

FVector ASRCameraPawn::ClampPivotLocationInsideSpace(const FVector& CandidateLocation) const
{
	FVector SpaceCenter = FVector::ZeroVector;
	float SpaceRadius = 0.0f;
	if (!ResolveSpaceBoundary(SpaceCenter, SpaceRadius))
	{
		return CandidateLocation;
	}

	const FVector SpaceToCandidate = CandidateLocation - SpaceCenter;
	const float SafeRadius = FMath::Max(0.0f, SpaceRadius);
	if (SpaceToCandidate.SizeSquared() <= FMath::Square(SafeRadius))
	{
		return CandidateLocation;
	}

	return SpaceCenter + (SpaceToCandidate.GetSafeNormal() * SafeRadius);
}

float ASRCameraPawn::ClampZoomDistanceAgainstSpace(float ZoomDistance, const FVector& CandidatePawnLocation) const
{
	FVector SpaceCenter = FVector::ZeroVector;
	float SpaceRadius = 0.0f;
	if (!ResolveSpaceBoundary(SpaceCenter, SpaceRadius))
	{
		return ZoomDistance;
	}

	const float SafeSpaceRadius = FMath::Max(0.0f, SpaceRadius - FMath::Max(0.0f, CameraSurfacePadding));
	if (SafeSpaceRadius <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector CameraDirection = GetCameraDirectionFromPivot();
	if (CameraDirection.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return ZoomDistance;
	}

	const FVector SpaceToPivot = CandidatePawnLocation - SpaceCenter;
	const float B = FVector::DotProduct(SpaceToPivot, CameraDirection);
	const float C = SpaceToPivot.SizeSquared() - FMath::Square(SafeSpaceRadius);
	const float Discriminant = FMath::Square(B) - C;
	if (Discriminant < 0.0f)
	{
		return 0.0f;
	}

	const float ExitDistance = -B + FMath::Sqrt(Discriminant);
	const float MaximumZoomDistance = FMath::Max(0.0f, ExitDistance);
	return FMath::Min(FMath::Max(0.0f, ZoomDistance), MaximumZoomDistance);
}

bool ASRCameraPawn::ResolveCelestialCameraAvoidanceSphere(const AActor* Actor, FVector& OutCenter, float& OutRadius) const
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

	OutRadius = StarRovers::Camera::ComputeScaledBodyRadius(Actor);
	if (OutRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutCenter = Actor->GetActorLocation();

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
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

FVector ASRCameraPawn::GetCameraDirectionFromPivot() const
{
	if (Camera)
	{
		const FVector CurrentCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
		if (CurrentCameraOffset.SizeSquared() > UE_SMALL_NUMBER)
		{
			return CurrentCameraOffset.GetSafeNormal();
		}
	}

	if (SpringArm)
	{
		return (-SpringArm->GetForwardVector()).GetSafeNormal();
	}

	return FVector::ForwardVector;
}

float ASRCameraPawn::ClampZoomDistanceAgainstCelestialBodies(float ZoomDistance, const FVector& CandidatePawnLocation) const
{
	const USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry();
	if (!CelestialRegistry)
	{
		return ZoomDistance;
	}

	TArray<AActor*> CelestialBodies;
	CelestialRegistry->GetCelestialBodies(CelestialBodies);
	if (CelestialBodies.IsEmpty())
	{
		return ZoomDistance;
	}

	const FVector CameraDirection = GetCameraDirectionFromPivot();
	if (CameraDirection.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return ZoomDistance;
	}

	float AdjustedZoomDistance = FMath::Max(0.0f, ZoomDistance);

	constexpr int32 MaxAvoidancePasses = 3;
	for (int32 PassIndex = 0; PassIndex < MaxAvoidancePasses; ++PassIndex)
	{
		bool bAdjustedThisPass = false;
		for (const AActor* CandidateBody : CelestialBodies)
		{
			if (!IsValid(CandidateBody) || CandidateBody == this)
			{
				continue;
			}

			FVector BodyCenter = FVector::ZeroVector;
			float BodyRadius = 0.0f;
			if (!ResolveCelestialCameraAvoidanceSphere(CandidateBody, BodyCenter, BodyRadius))
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

bool ASRCameraPawn::ResolveFocusedObliqueViewZoomRange(float& OutNearZoomDistance, float& OutFarZoomDistance) const
{
	OutNearZoomDistance = 0.0f;
	OutFarZoomDistance = 0.0f;
	if (!UseObliqueView || !UseFocusedObliqueViewAltitudeRange || !IsValid(FocusedActor))
	{
		return false;
	}

	FVector BodyCenter = FVector::ZeroVector;
	float AvoidanceRadius = 0.0f;
	if (!ResolveCelestialCameraAvoidanceSphere(FocusedActor, BodyCenter, AvoidanceRadius))
	{
		return false;
	}

	const float BodyRadius = StarRovers::Camera::ComputeScaledBodyRadius(FocusedActor);
	if (BodyRadius <= KINDA_SMALL_NUMBER || AvoidanceRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float NearAltitudeMultiplier = FMath::Max(0.0f, FocusedObliqueViewNearAltitudeMultiplier);
	const float FarAltitudeMultiplier = FMath::Max(
		NearAltitudeMultiplier + UE_SMALL_NUMBER,
		FocusedObliqueViewFarAltitudeMultiplier);

	OutNearZoomDistance = AvoidanceRadius + (BodyRadius * NearAltitudeMultiplier);
	OutFarZoomDistance = AvoidanceRadius + (BodyRadius * FarAltitudeMultiplier);
	return OutFarZoomDistance > OutNearZoomDistance + UE_SMALL_NUMBER;
}

float ASRCameraPawn::GetFocusedObliqueViewBlendAlpha(float ZoomDistance) const
{
	float NearZoomDistance = 0.0f;
	float FarZoomDistance = 0.0f;
	if (!ResolveFocusedObliqueViewZoomRange(NearZoomDistance, FarZoomDistance))
	{
		return -1.0f;
	}

	const float RawFarAlpha = (FMath::Max(0.0f, ZoomDistance) - NearZoomDistance)
		/ FMath::Max(FarZoomDistance - NearZoomDistance, UE_SMALL_NUMBER);
	const float ClampedFarAlpha = FMath::Clamp(RawFarAlpha, 0.0f, 1.0f);
	const float SmoothFarAlpha = ClampedFarAlpha * ClampedFarAlpha * (3.0f - (2.0f * ClampedFarAlpha));
	return 1.0f - SmoothFarAlpha;
}

float ASRCameraPawn::GetObliqueViewBlendAlpha(float ZoomDistance) const
{
	if (!UseObliqueView)
	{
		return 0.0f;
	}

	const float MinimumZoomDistance = 0.0f;
	const float MaximumZoomDistance = FMath::Max(1.0f, GetMaxZoomDistance());
	const float StartRatio = FMath::Clamp(ObliqueViewStart, 0.0f, 1.0f);
	const float EndRatio = FMath::Clamp(ObliqueViewEnd, StartRatio + UE_SMALL_NUMBER, 1.0f);
	const float StartZoomDistance = FMath::Lerp(MinimumZoomDistance, MaximumZoomDistance, StartRatio);
	const float EndZoomDistance = FMath::Lerp(MinimumZoomDistance, MaximumZoomDistance, EndRatio);
	const float RawAlpha = (FMath::Max(0.0f, ZoomDistance) - StartZoomDistance) / FMath::Max(EndZoomDistance - StartZoomDistance, UE_SMALL_NUMBER);
	const float ClampedAlpha = FMath::Clamp(RawAlpha, 0.0f, 1.0f);
	const float SmoothAlpha = ClampedAlpha * ClampedAlpha * (3.0f - (2.0f * ClampedAlpha));
	return SmoothAlpha;
}

FRotator ASRCameraPawn::GetViewRotationForZoom(float ZoomDistance) const
{
	const float FocusedBlendAlpha = GetFocusedObliqueViewBlendAlpha(ZoomDistance);
	if (FocusedBlendAlpha >= 0.0f)
	{
		return FRotator(
			FMath::Lerp(FocusedObliqueViewBaseRotation.Pitch, FocusedObliqueViewMaxRotation.Pitch, FocusedBlendAlpha),
			FMath::Lerp(FocusedObliqueViewBaseRotation.Yaw, FocusedObliqueViewMaxRotation.Yaw, FocusedBlendAlpha),
			FMath::Lerp(FocusedObliqueViewBaseRotation.Roll, FocusedObliqueViewMaxRotation.Roll, FocusedBlendAlpha)).GetNormalized();
	}

	const float BlendAlpha = GetObliqueViewBlendAlpha(ZoomDistance);
	return FRotator(
		FMath::Lerp(NearViewRotation.Pitch, FarViewRotation.Pitch, BlendAlpha),
		FMath::Lerp(NearViewRotation.Yaw, FarViewRotation.Yaw, BlendAlpha),
		FMath::Lerp(NearViewRotation.Roll, FarViewRotation.Roll, BlendAlpha)).GetNormalized();
}

void ASRCameraPawn::ApplyZoomDrivenViewRotation(float ZoomDistance)
{
	if (!Camera)
	{
		return;
	}

	ObliqueViewStart = FMath::Clamp(ObliqueViewStart, 0.0f, 1.0f);
	ObliqueViewEnd = FMath::Clamp(ObliqueViewEnd, ObliqueViewStart + UE_SMALL_NUMBER, 1.0f);
	const FRotator BaseViewRotation = GetViewRotationForZoom(ZoomDistance);
	if (ShouldAllowFocusSurface())
	{
		const FQuat SurfaceLookQuat = FocusSurface.Rotation.GetNormalized();
		if (SpringArm)
		{
			SpringArm->SetWorldRotation(SurfaceLookQuat.Rotator().GetNormalized());
		}
		Camera->SetRelativeRotation(BaseViewRotation);
		return;
	}

	if (SpringArm)
	{
		SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
	}
	Camera->SetRelativeRotation(BaseViewRotation);
}
