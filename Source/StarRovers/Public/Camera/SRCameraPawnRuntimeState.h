#pragma once

#include "CoreMinimal.h"

class AActor;

struct FSRCameraFocusArcTransitionState
{
    FVector StartLocation = FVector::ZeroVector;
    float Elapsed = 0.0f;
    float StartZoomDistance = 0.0f;
    float FinalZoomDistance = 0.0f;
    float PeakZoomDistance = 0.0f;
    bool bActive = false;

    void Reset();
};

struct FSRCameraFocusSurfaceRuntimeState
{
    FVector2D Input = FVector2D::ZeroVector;
    FVector2D AcceleratedInput = FVector2D::ZeroVector;
    FVector2D AngularVelocity = FVector2D::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    FQuat TargetRotation = FQuat::Identity;
    FQuat RigAlignmentStartRotation = FQuat::Identity;
    FVector RigAlignmentStartOffset = FVector::ZeroVector;
    FVector RigAlignmentAxis = FVector::UpVector;
    FVector RotationSmoothVelocity = FVector::ZeroVector;
    float RigAlignmentCurrentAngleRadians = 0.0f;
    float RigAlignmentTargetAngleRadians = 0.0f;
    bool bIsResettingRotation = false;
    bool bIsAligningRig = false;
    bool bPendingGridAutoAlignment = false;

    void ResetRotation();
    void ResetMotion();
    void ResetRigAlignment();
};

struct FSRCameraDynamicMeshVisibilityState
{
    FVector CameraLocation = FVector::ZeroVector;
    FRotator CameraRotation = FRotator::ZeroRotator;
    TWeakObjectPtr<AActor> FocusedActor;
    TWeakObjectPtr<AActor> DynamicMeshBody;
    float ZoomDistance = 0.0f;
    double UpdateTime = -BIG_NUMBER;
    int32 AppliedCelestialBodyCount = 0;
    bool bHasAppliedMeshVisibility = false;
    bool bHasState = false;

    bool ShouldRefresh(
        const FVector& CurrentCameraLocation,
        const FRotator& CurrentCameraRotation,
        float CurrentZoomDistance,
        AActor* CurrentFocusedActor,
        double CurrentTime,
        double MinRefreshIntervalSeconds,
        float MinCameraMoveDistance,
        float MinZoomDelta,
        float MinRotationDeltaDegrees) const;
    void Store(
        const FVector& CurrentCameraLocation,
        const FRotator& CurrentCameraRotation,
        float CurrentZoomDistance,
        AActor* CurrentFocusedActor,
        double CurrentTime);
    void Reset();
};

struct FSRCameraSpaceBoundaryCacheState
{
    TWeakObjectPtr<AActor> Actor;
    FVector Center = FVector::ZeroVector;
    float Radius = 0.0f;
    double FullScanTime = -BIG_NUMBER;
    uint64 Frame = 0;
    bool bHasResult = false;
    bool bFound = false;

    void Reset();
};
