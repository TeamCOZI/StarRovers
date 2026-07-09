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
	UpdateFocusSurfaceRotation(DeltaSeconds);

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
	FSRCameraZoomUpdateController::Update(
		SpringArm,
		ZoomDistanceTarget,
		NewLocation,
		DeltaSeconds,
		PivotUpdate.bUpdatedFocusArcTransition,
		[this](float ZoomDistance)
		{
			return ClampZoomDistance(ZoomDistance);
		},
		[this](float ZoomDistance, const FVector& PivotLocation)
		{
			return ClampZoomDistanceAgainstSpace(ZoomDistance, PivotLocation);
		},
		[this](float ZoomDistance, const FVector& PivotLocation)
		{
			return ClampZoomDistanceAgainstCelestialBodies(ZoomDistance, PivotLocation);
		},
		[this](float ZoomDistance)
		{
			ApplyZoomDrivenViewRotation(ZoomDistance);
		});

	SetActorLocation(NewLocation);
	UpdateComponentTransforms();
	if (SpringArm)
	{
		SpringArm->UpdateComponentToWorld();
	}
	if (Camera)
	{
		Camera->UpdateComponentToWorld();
	}
	if (!bIsFocusSurfaceActive && FocusSurface.bPendingGridAutoAlignment)
	{
		FocusSurface.bPendingGridAutoAlignment = false;
		TryStartFocusSurfaceGridAlignment();
	}
	UpdateDynamicMeshVisibility();
}
