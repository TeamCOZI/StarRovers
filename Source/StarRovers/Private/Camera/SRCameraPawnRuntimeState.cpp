#include "Camera/SRCameraPawnRuntimeState.h"

void FSRCameraFocusArcTransitionState::Reset()
{
    bActive = false;
    Elapsed = 0.0f;
    StartLocation = FVector::ZeroVector;
    StartZoomDistance = 0.0f;
    FinalZoomDistance = 0.0f;
    PeakZoomDistance = 0.0f;
}

void FSRCameraFocusSurfaceRuntimeState::ResetRotation()
{
    Rotation = FQuat::Identity;
    TargetRotation = FQuat::Identity;
    ResetRigAlignment();
}

void FSRCameraFocusSurfaceRuntimeState::ResetMotion()
{
    Input = FVector2D::ZeroVector;
    AcceleratedInput = FVector2D::ZeroVector;
    AngularVelocity = FVector2D::ZeroVector;
    bPendingGridAutoAlignment = false;
}

void FSRCameraFocusSurfaceRuntimeState::ResetRigAlignment()
{
    RigAlignmentStartRotation = FQuat::Identity;
    RigAlignmentStartOffset = FVector::ZeroVector;
    RigAlignmentAxis = FVector::UpVector;
    RotationSmoothVelocity = FVector::ZeroVector;
    RigAlignmentCurrentAngleRadians = 0.0f;
    RigAlignmentTargetAngleRadians = 0.0f;
    bIsResettingRotation = false;
    bIsAligningRig = false;
}

bool FSRCameraDynamicMeshVisibilityState::ShouldRefresh(
    const FVector& CurrentCameraLocation,
    const FRotator& CurrentCameraRotation,
    float CurrentZoomDistance,
    AActor* CurrentFocusedActor,
    double CurrentTime,
    double MinRefreshIntervalSeconds,
    float MinCameraMoveDistance,
    float MinZoomDelta,
    float MinRotationDeltaDegrees) const
{
    if (!bHasState)
    {
        return true;
    }

    const bool bFocusChanged = FocusedActor.Get() != CurrentFocusedActor;
    const bool bMovedEnough = FVector::DistSquared(CurrentCameraLocation, CameraLocation) >= FMath::Square(MinCameraMoveDistance);
    const bool bZoomedEnough = FMath::Abs(CurrentZoomDistance - ZoomDistance) >= MinZoomDelta;
    const bool bRotatedEnough = !CurrentCameraRotation.Equals(CameraRotation, MinRotationDeltaDegrees);
    const bool bIntervalElapsed = (CurrentTime - UpdateTime) >= MinRefreshIntervalSeconds;
    return bFocusChanged || bMovedEnough || bZoomedEnough || bRotatedEnough || bIntervalElapsed;
}

void FSRCameraDynamicMeshVisibilityState::Store(
    const FVector& CurrentCameraLocation,
    const FRotator& CurrentCameraRotation,
    float CurrentZoomDistance,
    AActor* CurrentFocusedActor,
    double CurrentTime)
{
    CameraLocation = CurrentCameraLocation;
    CameraRotation = CurrentCameraRotation;
    FocusedActor = CurrentFocusedActor;
    ZoomDistance = CurrentZoomDistance;
    UpdateTime = CurrentTime;
    bHasState = true;
}

void FSRCameraDynamicMeshVisibilityState::Reset()
{
    CameraLocation = FVector::ZeroVector;
    CameraRotation = FRotator::ZeroRotator;
    FocusedActor = nullptr;
    DynamicMeshBody = nullptr;
    ZoomDistance = 0.0f;
    UpdateTime = -BIG_NUMBER;
    AppliedCelestialBodyCount = 0;
    bHasAppliedMeshVisibility = false;
    bHasState = false;
}
