#include "SRCameraDynamicMeshVisibilityController.h"

#include "SRCameraCelestialAvoidanceResolver.h"
#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"

bool FSRCameraDynamicMeshVisibilityController::Apply(
	USRCelestialBodyRegistrySubsystem* CelestialRegistry,
	const UCameraComponent* Camera,
	const APlayerController* PlayerController,
	AActor* FocusedActor,
	FSRCameraDynamicMeshVisibilityState& VisibilityState,
	AActor*& OutDirectionalLightTarget)
{
	OutDirectionalLightTarget = nullptr;
	if (!CelestialRegistry)
	{
		return false;
	}

	TArray<AActor*> CelestialBodies;
	CelestialRegistry->GetCelestialBodies(CelestialBodies);
	if (CelestialBodies.IsEmpty())
	{
		CelestialRegistry->RefreshCelestialBodies();
		CelestialRegistry->GetCelestialBodies(CelestialBodies);
		if (CelestialBodies.IsEmpty())
		{
			return false;
		}
	}

	TArray<ASRCelestialBody*> ValidCelestialBodies;
	ValidCelestialBodies.Reserve(CelestialBodies.Num());
	ASRCelestialBody* FocusedDynamicBody = nullptr;
	ASRCelestialBody* BestNonFocusedDynamicBody = nullptr;
	float BestDynamicMeshTargetScreenSizeRatio = 0.0f;
	const bool bHasCamera = Camera != nullptr;
	const FVector CameraLocation = bHasCamera ? Camera->GetComponentLocation() : FVector::ZeroVector;
	const FVector CameraForward = bHasCamera ? Camera->GetForwardVector().GetSafeNormal() : FVector::ForwardVector;
	const FVector CameraRight = bHasCamera ? Camera->GetRightVector() : FVector::RightVector;
	const FVector CameraUp = bHasCamera ? Camera->GetUpVector() : FVector::UpVector;
	const float CameraFieldOfViewDegrees = bHasCamera ? Camera->FieldOfView : 90.0f;
	const float SafeFieldOfViewDegrees = FMath::Clamp(CameraFieldOfViewDegrees, 5.0f, 170.0f);
	const float TanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(SafeFieldOfViewDegrees * 0.5f));
	const float AspectRatio = ResolveViewportAspectRatio(PlayerController);
	for (AActor* BodyActor : CelestialBodies)
	{
		ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
		if (!IsValid(CelestialBody))
		{
			continue;
		}

		ValidCelestialBodies.Add(CelestialBody);
		float ScreenSizeRatio = 0.0f;
		if (!ShouldUseDynamicMesh(
				BodyActor,
				bHasCamera,
				CameraLocation,
				CameraForward,
				CameraRight,
				CameraUp,
				CameraFieldOfViewDegrees,
				TanHalfFieldOfView,
				AspectRatio,
				FocusedActor,
				ScreenSizeRatio)
			|| USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			continue;
		}

		if (BodyActor == FocusedActor)
		{
			FocusedDynamicBody = CelestialBody;
			OutDirectionalLightTarget = BodyActor;
			BestDynamicMeshTargetScreenSizeRatio = TNumericLimits<float>::Max();
			continue;
		}

		if (!FocusedDynamicBody && ScreenSizeRatio > BestDynamicMeshTargetScreenSizeRatio)
		{
			BestNonFocusedDynamicBody = CelestialBody;
			OutDirectionalLightTarget = BodyActor;
			BestDynamicMeshTargetScreenSizeRatio = ScreenSizeRatio;
		}
	}

	ASRCelestialBody* DynamicBody = FocusedDynamicBody ? FocusedDynamicBody : BestNonFocusedDynamicBody;
	const bool bNeedsFullMeshApply =
		!VisibilityState.bHasAppliedMeshVisibility
		|| VisibilityState.AppliedCelestialBodyCount != ValidCelestialBodies.Num();
	ASRCelestialBody* PreviousDynamicBody = Cast<ASRCelestialBody>(VisibilityState.DynamicMeshBody.Get());
	if (bNeedsFullMeshApply)
	{
		if (IsValid(PreviousDynamicBody) && PreviousDynamicBody != DynamicBody && !ValidCelestialBodies.Contains(PreviousDynamicBody))
		{
			PreviousDynamicBody->SetCelestialBodyMesh(false);
		}
		for (ASRCelestialBody* CelestialBody : ValidCelestialBodies)
		{
			CelestialBody->SetCelestialBodyMesh(CelestialBody == DynamicBody);
		}
	}
	else if (PreviousDynamicBody != DynamicBody)
	{
		if (IsValid(PreviousDynamicBody))
		{
			PreviousDynamicBody->SetCelestialBodyMesh(false);
		}
		if (IsValid(DynamicBody))
		{
			DynamicBody->SetCelestialBodyMesh(true);
		}
	}

	VisibilityState.DynamicMeshBody = DynamicBody;
	VisibilityState.AppliedCelestialBodyCount = ValidCelestialBodies.Num();
	VisibilityState.bHasAppliedMeshVisibility = true;

	const bool bHasStaticMeshBody = ValidCelestialBodies.Num() > (DynamicBody ? 1 : 0);
	if (AActor* PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor())
	{
		if (UPointLightComponent* StarPointLight = PrimaryStarActor->FindComponentByClass<UPointLightComponent>())
		{
			const bool bShouldShowStarPointLight = bHasStaticMeshBody || !IsValid(OutDirectionalLightTarget);
			if (StarPointLight->IsVisible() != bShouldShowStarPointLight)
			{
				StarPointLight->SetVisibility(bShouldShowStarPointLight);
			}
		}
	}

	return true;
}

bool FSRCameraDynamicMeshVisibilityController::ShouldUseDynamicMesh(
	const AActor* BodyActor,
	bool bHasCamera,
	const FVector& CameraLocation,
	const FVector& CameraForward,
	const FVector& CameraRight,
	const FVector& CameraUp,
	float CameraFieldOfViewDegrees,
	float TanHalfFieldOfView,
	float AspectRatio,
	const AActor* FocusedActor,
	float& OutScreenSizeRatio)
{
	constexpr float NonFocusedDynamicMeshScreenSizeThreshold = 0.15f;

	OutScreenSizeRatio = 0.0f;
	const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
	if (!IsValid(CelestialBody) || !bHasCamera)
	{
		return false;
	}

	const FVector CameraToBody = BodyActor->GetActorLocation() - CameraLocation;
	const float Depth = FVector::DotProduct(CameraToBody, CameraForward);
	if (Depth <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float BodyRadius = FSRCameraCelestialAvoidanceResolver::ComputeScaledBodyRadius(BodyActor);
	if (BodyRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (TanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return false;
	}

	const float HalfFrustumHeight = Depth * TanHalfFieldOfView;
	const float HalfFrustumWidth = HalfFrustumHeight * FMath::Max(AspectRatio, UE_SMALL_NUMBER);
	const float HorizontalOffset = FMath::Abs(FVector::DotProduct(CameraToBody, CameraRight));
	const float VerticalOffset = FMath::Abs(FVector::DotProduct(CameraToBody, CameraUp));
	if (HorizontalOffset > HalfFrustumWidth + BodyRadius || VerticalOffset > HalfFrustumHeight + BodyRadius)
	{
		return false;
	}

	OutScreenSizeRatio = USRCelestialBodyRuntimeLibrary::GetScreenScale(
		BodyActor,
		CameraLocation,
		CameraForward,
		CameraFieldOfViewDegrees);
	if (BodyActor == FocusedActor)
	{
		return true;
	}

	return OutScreenSizeRatio >= NonFocusedDynamicMeshScreenSizeThreshold;
}

float FSRCameraDynamicMeshVisibilityController::ResolveViewportAspectRatio(const APlayerController* PlayerController)
{
	float AspectRatio = 16.0f / 9.0f;
	if (!PlayerController)
	{
		return AspectRatio;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth > 0 && ViewportHeight > 0)
	{
		AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
	}
	return AspectRatio;
}
