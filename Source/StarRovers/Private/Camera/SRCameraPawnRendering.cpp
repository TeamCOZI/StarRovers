#include "Camera/SRCameraPawn.h"

#include "Utility/SRLog.h"
#include "SRCameraDynamicMeshVisibilityController.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
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
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRCameraPawn requires Camera to resolve screen-space thickness FOV."));
		return 0.0f;
	}

	return FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
}

void ASRCameraPawn::RefreshScreenSpaceThicknessReferenceView()
{
	ScreenSpaceThicknessReferenceZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	if (!Camera)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRCameraPawn requires Camera to refresh screen-space thickness reference FOV."));
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
	if (FSRCameraDynamicMeshVisibilityController::Apply(
		FindCelestialRegistry(),
		Camera,
		Cast<APlayerController>(GetController()),
		FocusedActor.Get(),
		DynamicMeshVisibility,
		DirectionalLightTarget))
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
