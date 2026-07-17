#include "Camera/SRCameraPawn.h"

#include "SRCameraPivotUpdateController.h"
#include "SRCameraZoomUpdateController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

void ASRCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyMappingContext();
	UpdateFocusSurface(DeltaSeconds);

	if (FocusedActor)
	{
		if (IsValid(FocusedActor))
		{
			if (HasExitedFocusedActorGravityField())
			{
				ClearFocusActor();
			}
		}
		else
		{
			ClearFocusActor();
		}
	}

	const bool bHasFocusedActor = IsValid(FocusedActor);
	const bool bUpdateCenterTargetAfterPivot = bHasFocusSurfaceCenterTarget;
	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	if (!bUpdateCenterTargetAfterPivot)
	{
		UpdateFocusSurfaceRotation(DeltaSeconds, GetActorLocation(), CurrentZoomDistance);
	}

	FSRCameraPivotUpdateSettings PivotUpdateSettings;
	PivotUpdateSettings.CurrentLocation = GetActorLocation();
	PivotUpdateSettings.FocusLocation = bHasFocusedActor ? GetFocusLocation() : DragTargetLocation;
	PivotUpdateSettings.DeltaSeconds = DeltaSeconds;
	PivotUpdateSettings.FocusFollowSmoothTime = FocusFollowSmoothTime;
	PivotUpdateSettings.FocusArcTransitionDuration = FocusArcTransitionDuration;
	PivotUpdateSettings.FocusArcMinHeight = FocusArcMinHeight;
	PivotUpdateSettings.FocusArcHeightMultiplier = FocusArcHeightMultiplier;
	PivotUpdateSettings.bIsDragging = bIsDragging;
	PivotUpdateSettings.bHasFocusedActor = bHasFocusedActor;
	const FSRCameraPivotUpdateResult PivotUpdate = FSRCameraPivotUpdateController::Update(
		DragTargetLocation,
		FocusDragOffset,
		FocusTrackingDelta,
		FocusTrackingDeltaVelocity,
		FocusArcTransition,
		ZoomDistanceTarget,
		PivotUpdateSettings,
		[this](const FVector& CandidateLocation)
		{
			return ClampPivotLocationInsideSpace(CandidateLocation);
		},
		[this](float ZoomDistance)
		{
			return ClampZoomDistance(ZoomDistance);
		});
	const FVector NewLocation = PivotUpdate.NewLocation;
	if (bUpdateCenterTargetAfterPivot)
	{
		UpdateFocusSurfaceRotation(DeltaSeconds, NewLocation, CurrentZoomDistance);
	}

	const FVector CameraDirectionFromPivot = GetCameraDirectionFromPivot(FocusSurface.Rotation);
	const float NewZoomDistance = FSRCameraZoomUpdateController::Update(
		CurrentZoomDistance,
		ZoomDistanceTarget,
		NewLocation,
		DeltaSeconds,
		PivotUpdate.bUpdatedFocusArcTransition,
		[this](float ZoomDistance)
		{
			return ClampZoomDistance(ZoomDistance);
		},
		[this, CameraDirectionFromPivot](float ZoomDistance, const FVector& PivotLocation)
		{
			return ClampZoomDistanceAgainstSpace(ZoomDistance, PivotLocation, CameraDirectionFromPivot);
		},
		[this, CameraDirectionFromPivot](float ZoomDistance, const FVector& PivotLocation)
		{
			return ClampZoomDistanceAgainstCelestialBodies(ZoomDistance, PivotLocation, CameraDirectionFromPivot);
		});

	ApplyCameraFrame(NewLocation, NewZoomDistance);
	if (!bIsFocusSurfaceActive && FocusSurface.bPendingGridAutoAlignment)
	{
		FocusSurface.bPendingGridAutoAlignment = false;
		TryStartFocusSurfaceGridAlignment();
	}
	UpdateDynamicMeshVisibility();
}
