#include "Visual/SRLineThicknessUtils.h"

#include "Camera/SRCameraPawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

float FSRCameraInfo::ComputeDepthToWorldLocation(const FVector& WorldLocation) const
{
	if (!bIsValid)
	{
		return 0.0f;
	}

	return FVector::DotProduct(WorldLocation - ViewLocation, ViewForward);
}

bool FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(
	const UWorld* World,
	FSRCameraInfo& OutCameraInfo)
{
	OutCameraInfo = FSRCameraInfo();

	if (!World)
	{
		return false;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager))
	{
		return false;
	}

	const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
	const FVector ViewForward = CameraManager->GetCameraRotation().Vector();
	const float FieldOfViewDegrees = FMath::Clamp(CameraManager->GetFOVAngle(), 5.0f, 170.0f);
	const float TanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(FieldOfViewDegrees * 0.5f));
	if (ViewForward.IsNearlyZero() || TanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return false;
	}

	OutCameraInfo.bIsValid = true;
	OutCameraInfo.ViewLocation = CameraManager->GetCameraLocation();
	OutCameraInfo.ViewForward = ViewForward.GetSafeNormal();
	OutCameraInfo.TanHalfVerticalFieldOfView = TanHalfFieldOfView;
	return true;
}

void FSRLineThicknessUtils::ResolveReferenceView(
	const UWorld* World,
	float& OutReferenceViewDepth,
	float& OutReferenceFieldOfViewDegrees)
{
	OutReferenceViewDepth = DefaultReferenceViewDepth;
	OutReferenceFieldOfViewDegrees = DefaultReferenceFieldOfViewDegrees;

	if (!World)
	{
		return;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(PlayerController->GetPawn());
	if (!IsValid(CameraPawn))
	{
		return;
	}

	OutReferenceViewDepth = CameraPawn->GetScreenSpaceThicknessReferenceZoomDistance();
	OutReferenceFieldOfViewDegrees = CameraPawn->GetScreenSpaceThicknessReferenceFieldOfView();
}

float FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
	const FSRCameraInfo& CameraInfo,
	const FVector& WorldLocation,
	float ReferenceWorldThickness,
	float ReferenceViewDepth,
	float ReferenceFieldOfViewDegrees)
{
	const float SafeReferenceWorldThickness = FMath::Max(0.0f, ReferenceWorldThickness);
	if (!CameraInfo.bIsValid || SafeReferenceWorldThickness <= KINDA_SMALL_NUMBER)
	{
		return SafeReferenceWorldThickness;
	}

	const float SafeReferenceViewDepth = FMath::Max(1.0f, ReferenceViewDepth);
	const float SafeReferenceFieldOfView = FMath::Clamp(ReferenceFieldOfViewDegrees, 5.0f, 170.0f);
	const float ReferenceTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(SafeReferenceFieldOfView * 0.5f));
	if (ReferenceTanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return SafeReferenceWorldThickness;
	}

	const float CurrentViewDepth = FMath::Max(FMath::Abs(CameraInfo.ComputeDepthToWorldLocation(WorldLocation)), 1.0f);
	const float ThicknessScale = (CurrentViewDepth * CameraInfo.TanHalfVerticalFieldOfView)
		/ (SafeReferenceViewDepth * ReferenceTanHalfFieldOfView);

	return SafeReferenceWorldThickness * ThicknessScale;
}
