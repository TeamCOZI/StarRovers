#include "Camera/SRCameraPawn.h"

#include "SRCameraPawnInternal.h"
#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"

float ASRCameraPawn::GetScreenSpaceThicknessReferenceZoomDistance() const
{
	if (ScreenSpaceThicknessReferenceZoomDistance > KINDA_SMALL_NUMBER)
	{
		return ScreenSpaceThicknessReferenceZoomDistance;
	}

	return FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
}

float ASRCameraPawn::GetScreenSpaceThicknessReferenceFieldOfView() const
{
	if (ScreenSpaceThicknessReferenceFieldOfView > KINDA_SMALL_NUMBER)
	{
		return ScreenSpaceThicknessReferenceFieldOfView;
	}

	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera to resolve screen-space thickness FOV."));
		return 0.0f;
	}

	return FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
}

void ASRCameraPawn::RefreshScreenSpaceThicknessReferenceView()
{
	ScreenSpaceThicknessReferenceZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera to refresh screen-space thickness reference FOV."));
		ScreenSpaceThicknessReferenceFieldOfView = 0.0f;
		return;
	}
	ScreenSpaceThicknessReferenceFieldOfView = FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
}

void ASRCameraPawn::UpdateDynamicMeshVisibility()
{
	if (!Camera)
	{
		return;
	}

	constexpr double MinRefreshIntervalSeconds = 0.10;
	constexpr float MinCameraMoveDistance = 25.0f;
	constexpr float MinZoomDelta = 25.0f;
	constexpr float MinRotationDeltaDegrees = 0.25f;

	const UWorld* World = GetWorld();
	const double CurrentTime = World ? World->GetTimeSeconds() : 0.0;
	const FVector CameraLocation = Camera->GetComponentLocation();
	const FRotator CameraRotation = Camera->GetComponentRotation();
	const float CurrentZoomDistance = SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget;
	AActor* CurrentFocusedActor = FocusedActor.Get();

	if (!DynamicMeshVisibility.ShouldRefresh(
		CameraLocation,
		CameraRotation,
		CurrentZoomDistance,
		CurrentFocusedActor,
		CurrentTime,
		MinRefreshIntervalSeconds,
		MinCameraMoveDistance,
		MinZoomDelta,
		MinRotationDeltaDegrees))
	{
		return;
	}

	AActor* DirectionalLightTarget = nullptr;
	if (ApplyCelestialBodyMeshVisibility(DirectionalLightTarget))
	{
		ConfigureDirectionalLight(DirectionalLightTarget);
	}
	else
	{
		ConfigureDirectionalLight(nullptr);
	}

	DynamicMeshVisibility.Store(
		CameraLocation,
		CameraRotation,
		CurrentZoomDistance,
		CurrentFocusedActor,
		CurrentTime);
}

bool ASRCameraPawn::ApplyCelestialBodyMeshVisibility(AActor*& OutDirectionalLightTarget)
{
	OutDirectionalLightTarget = nullptr;

	USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry();
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
	for (AActor* BodyActor : CelestialBodies)
	{
		ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
		if (!IsValid(CelestialBody))
		{
			continue;
		}

		ValidCelestialBodies.Add(CelestialBody);
		float ScreenSizeRatio = 0.0f;
		if (!ShouldUseDynamicMesh(BodyActor, ScreenSizeRatio) || USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			continue;
		}

		if (BodyActor == FocusedActor.Get())
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
		!DynamicMeshVisibility.bHasAppliedMeshVisibility
		|| DynamicMeshVisibility.AppliedCelestialBodyCount != ValidCelestialBodies.Num();
	ASRCelestialBody* PreviousDynamicBody = Cast<ASRCelestialBody>(DynamicMeshVisibility.DynamicMeshBody.Get());
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

	DynamicMeshVisibility.DynamicMeshBody = DynamicBody;
	DynamicMeshVisibility.AppliedCelestialBodyCount = ValidCelestialBodies.Num();
	DynamicMeshVisibility.bHasAppliedMeshVisibility = true;

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

bool ASRCameraPawn::ShouldUseDynamicMesh(const AActor* BodyActor, float& OutScreenSizeRatio) const
{
	constexpr float NonFocusedDynamicMeshScreenSizeThreshold = 0.15f;

	OutScreenSizeRatio = 0.0f;
	const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
	if (!IsValid(CelestialBody) || !Camera)
	{
		return false;
	}

	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector CameraForward = Camera->GetForwardVector().GetSafeNormal();
	const FVector CameraToBody = BodyActor->GetActorLocation() - CameraLocation;
	const float Depth = FVector::DotProduct(CameraToBody, CameraForward);
	if (Depth <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float BodyRadius = StarRovers::Camera::ComputeScaledBodyRadius(BodyActor);
	if (BodyRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float SafeFieldOfViewDegrees = FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
	const float TanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(SafeFieldOfViewDegrees * 0.5f));
	if (TanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return false;
	}

	float AspectRatio = 16.0f / 9.0f;
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		if (ViewportWidth > 0 && ViewportHeight > 0)
		{
			AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
		}
	}

	const float HalfFrustumHeight = Depth * TanHalfFieldOfView;
	const float HalfFrustumWidth = HalfFrustumHeight * FMath::Max(AspectRatio, UE_SMALL_NUMBER);
	const float HorizontalOffset = FMath::Abs(FVector::DotProduct(CameraToBody, Camera->GetRightVector()));
	const float VerticalOffset = FMath::Abs(FVector::DotProduct(CameraToBody, Camera->GetUpVector()));
	if (HorizontalOffset > HalfFrustumWidth + BodyRadius || VerticalOffset > HalfFrustumHeight + BodyRadius)
	{
		return false;
	}

	OutScreenSizeRatio = USRCelestialBodyRuntimeLibrary::GetScreenScale(
		BodyActor,
		CameraLocation,
		CameraForward,
		Camera->FieldOfView);
	if (BodyActor == FocusedActor.Get())
	{
		return true;
	}

	return OutScreenSizeRatio >= NonFocusedDynamicMeshScreenSizeThreshold;
}

void ASRCameraPawn::ConfigureDirectionalLight(AActor* LightingTarget)
{
	ADirectionalLight* DirectionalLightActor = FindDirectionalLightActor();
	UDirectionalLightComponent* DirectionalLightComponent = IsValid(DirectionalLightActor)
		? DirectionalLightActor->FindComponentByClass<UDirectionalLightComponent>()
		: nullptr;
	if (!IsValid(DirectionalLightComponent))
	{
		return;
	}

	USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry();
	const AActor* PrimaryStarActor = CelestialRegistry ? CelestialRegistry->GetPrimaryStarActor() : nullptr;
	const bool bCanUseDirectionalLight = IsValid(PrimaryStarActor) && IsValid(LightingTarget);
	if (DirectionalLightComponent->IsVisible() != bCanUseDirectionalLight)
	{
		DirectionalLightComponent->SetVisibility(bCanUseDirectionalLight);
	}
	if (!bCanUseDirectionalLight)
	{
		return;
	}

	const FVector StarToTargetDirection = (LightingTarget->GetActorLocation() - PrimaryStarActor->GetActorLocation()).GetSafeNormal();
	if (StarToTargetDirection.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return;
	}

	const FRotator DesiredLightRotation = StarToTargetDirection.Rotation();
	if (!DirectionalLightActor->GetActorRotation().Equals(DesiredLightRotation, 0.01f))
	{
		DirectionalLightActor->SetActorRotation(DesiredLightRotation);
	}
}

ADirectionalLight* ASRCameraPawn::FindDirectionalLightActor() const
{
	if (ADirectionalLight* CachedLight = CachedDirectionalLightActor.Get())
	{
		if (IsValid(CachedLight))
		{
			return CachedLight;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		ADirectionalLight* CandidateLight = *It;
		if (IsValid(CandidateLight))
		{
			CachedDirectionalLightActor = CandidateLight;
			return CandidateLight;
		}
	}

	return nullptr;
}

USRCelestialBodyRegistrySubsystem* ASRCameraPawn::FindCelestialRegistry() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>() : nullptr;
}
