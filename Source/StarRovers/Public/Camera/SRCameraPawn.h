#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "SRCameraPawn.generated.h"

class UCameraComponent;
class ADirectionalLight;
class UInputAction;
class UInputMappingContext;
class USceneComponent;
class USpringArmComponent;
class USRCelestialBodyRegistrySubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FSRFocusedActorChangedSignature, AActor*);

UCLASS(Blueprintable)
class STARROVERS_API ASRCameraPawn : public APawn
{
    GENERATED_BODY()

public:
    ASRCameraPawn();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Focus")
    void FocusActor(AActor* NewFocusActor);

    void FocusActorWithTransition(AActor* NewFocusActor, bool bUseArcTransition);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Focus")
    void ClearFocusActor();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Focus")
    AActor* GetFocusedActor() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Camera")
    float GetMaxZoomDistance() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Camera")
    float GetScreenSpaceThicknessReferenceZoomDistance() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Camera")
    float GetScreenSpaceThicknessReferenceFieldOfView() const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Focus")
    void SnapToFocusTarget();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Focus")
    void ResetFocus();

    FSRFocusedActorChangedSignature& OnFocusedActorChanged();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SceneRoot"))
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SpringArm"))
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Camera"))
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "DefaultMappingContext"))
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "DragHoldAction"))
    TObjectPtr<UInputAction> DragHoldAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "FocusSurfaceDragHoldAction"))
    TObjectPtr<UInputAction> FocusSurfaceDragHoldAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "DragDeltaAction"))
    TObjectPtr<UInputAction> DragDeltaAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "ZoomAction"))
    TObjectPtr<UInputAction> ZoomAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "FocusSurfaceAction"))
    TObjectPtr<UInputAction> FocusSurfaceAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "ResetFocusAction"))
    TObjectPtr<UInputAction> ResetFocusAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera", meta = (DisplayName = "ZoomSpeed", ClampMin = "0.0"))
    float ZoomSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|AdaptiveInput", meta = (DisplayName = "ZoomInputScaleMultiplier"))
    float ZoomInputScaleMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|AdaptiveInput", meta = (DisplayName = "LeftDragInputScaleMultiplier"))
    float LeftDragInputScaleMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|AdaptiveInput", meta = (DisplayName = "RightDragInputScaleMultiplier"))
    float RightDragInputScaleMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|AdaptiveInput", meta = (DisplayName = "RightDragInputScaleMax", ClampMin = "0.0"))
    float RightDragInputScaleMax;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera", meta = (DisplayName = "CameraSurfacePadding", ClampMin = "0.0"))
    float CameraSurfacePadding;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "UseObliqueView"))
    bool UseObliqueView;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "NearViewRotation"))
    FRotator NearViewRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "FarViewRotation"))
    FRotator FarViewRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "ObliqueViewStart", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float ObliqueViewStart;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "ObliqueViewEnd", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float ObliqueViewEnd;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView|Focused", meta = (DisplayName = "UseFocusedObliqueViewAltitudeRange"))
    bool UseFocusedObliqueViewAltitudeRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView|Focused", meta = (DisplayName = "FocusedObliqueViewNearAltitudeMultiplier", ClampMin = "0.0", UIMin = "0.0"))
    float FocusedObliqueViewNearAltitudeMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView|Focused", meta = (DisplayName = "FocusedObliqueViewFarAltitudeMultiplier", ClampMin = "0.0", UIMin = "0.0"))
    float FocusedObliqueViewFarAltitudeMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView|Focused", meta = (DisplayName = "FocusedObliqueViewBaseRotation"))
    FRotator FocusedObliqueViewBaseRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView|Focused", meta = (DisplayName = "FocusedObliqueViewMaxRotation"))
    FRotator FocusedObliqueViewMaxRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Focus", meta = (DisplayName = "FocusFollowSmoothTime", ClampMin = "0.01"))
    float FocusFollowSmoothTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Focus|Transition", meta = (DisplayName = "FocusArcTransitionDuration", ClampMin = "0.10"))
    float FocusArcTransitionDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Focus|Transition", meta = (DisplayName = "FocusArcHeightMultiplier", ClampMin = "0.0"))
    float FocusArcHeightMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Focus|Transition", meta = (DisplayName = "FocusArcMinHeight", ClampMin = "0.0"))
    float FocusArcMinHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Focus|Transition", meta = (DisplayName = "FocusArcZoomOutDistanceMultiplier", ClampMin = "0.0"))
    float FocusArcZoomOutDistanceMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "SurfaceRotateSensitivity", ClampMin = "0.0"))
    float SurfaceRotateSensitivity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FocusSurfaceSpeed", ClampMin = "0.0"))
    float FocusSurfaceSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FocusSurfaceInputAcceleration", ClampMin = "0.0"))
    float FocusSurfaceInputAcceleration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FocusSurfaceInputDeceleration", ClampMin = "0.0"))
    float FocusSurfaceInputDeceleration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FocusSurfaceInertiaDamping", ClampMin = "0.0"))
    float FocusSurfaceInertiaDamping;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FocusSurfaceMinInertiaSpeed", ClampMin = "0.0"))
    float FocusSurfaceMinInertiaSpeed;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera", meta = (DisplayName = "bIsDragging"))
    bool bIsDragging;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera", meta = (DisplayName = "bMappingContextApplied"))
    bool bMappingContextApplied;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "bIsFocusSurfaceActive"))
    bool bIsFocusSurfaceActive;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "bIsDraggingFocusSurface"))
    bool bIsDraggingFocusSurface;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera", meta = (DisplayName = "bHasDragStartMousePosition"))
    bool bHasDragStartMousePosition;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusedActor"))
    TObjectPtr<AActor> FocusedActor;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusDragOffset"))
    FVector FocusDragOffset;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusTrackingDelta"))
    FVector FocusTrackingDelta;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusTrackingDeltaVelocity"))
    FVector FocusTrackingDeltaVelocity;

private:
    void ApplyMappingContext();
    void ConfigureSpringArmCollision();
    void BroadcastFocusedActorChangedIfNeeded(AActor* PreviousFocusedActor);
    void BeginFocusArcTransition(float FinalZoomDistance);
    void StopFocusArcTransition();
    bool UpdateFocusArcTransition(float DeltaSeconds, FVector& OutNewLocation);
    void HandleDragHoldStarted();
    void HandleDragHoldCompleted();
    void HandleFocusSurfaceDragHoldStarted();
    void HandleFocusSurfaceDragHoldCompleted();
    void HandleDragDelta(const FInputActionValue& Value);
    void HandleZoom(const FInputActionValue& Value);
    void HandleFocusSurface(const FInputActionValue& Value);
    void HandleFocusSurfaceCompleted();
    void HandleResetFocus();
    bool HasExitedFocusedActorGravityField() const;
    bool GetMouseScreenPosition(FVector2D& OutMouseScreenPosition) const;
    FVector ConvertScreenDragToDragOffset(const FVector2D& StartScreenPosition, const FVector2D& CurrentScreenPosition) const;
    float GetScreenSpaceInputScale(float CurrentZoomDistance) const;
    float GetZoomSpeed() const;
    float GetMinimumZoomDistance() const;
    float ClampZoomDistance(float ZoomDistance) const;
    float GetSpaceSphereRadius() const;
    bool ResolveSpaceBoundary(FVector& OutCenter, float& OutRadius) const;
    FVector ClampPivotLocationInsideSpace(const FVector& CandidateLocation) const;
    float ClampZoomDistanceAgainstSpace(float ZoomDistance, const FVector& CandidatePawnLocation) const;
    bool ResolveCelestialCameraAvoidanceSphere(const AActor* Actor, FVector& OutCenter, float& OutRadius) const;
    FVector GetCameraDirectionFromPivot() const;
    float ClampZoomDistanceAgainstCelestialBodies(float ZoomDistance, const FVector& CandidatePawnLocation) const;
    bool ResolveFocusedObliqueViewZoomRange(float& OutNearZoomDistance, float& OutFarZoomDistance) const;
    float GetFocusedObliqueViewBlendAlpha(float ZoomDistance) const;
    float GetObliqueViewBlendAlpha(float ZoomDistance) const;
    FRotator GetViewRotationForZoom(float ZoomDistance) const;
    void ApplyZoomDrivenViewRotation(float ZoomDistance);
    bool ShouldAllowFocusSurface() const;
    bool TryComputeFocusSurfaceGridAlignmentDelta(const FQuat& ViewQuat, float ZoomDistance, FVector& OutAxis, float& OutAngleRadians) const;
    bool TryStartFocusSurfaceGridAlignment();
    void UpdateFocusSurface(float DeltaSeconds);
    void UpdateFocusSurfaceRotation(float DeltaSeconds);
    void ApplyFocusSurfaceRigAlignmentLocation();
    void ApplyFocusSurfaceDelta(const FVector2D& DegreesDelta);
    void RefreshScreenSpaceThicknessReferenceView();
    void UpdateDynamicMeshVisibility();
    bool ApplyCelestialBodyMeshVisibility(AActor*& OutDirectionalLightTarget);
    bool ShouldUseDynamicMesh(const AActor* BodyActor, float& OutScreenSizeRatio) const;
    void ConfigureDirectionalLight(AActor* LightingTarget);
    ADirectionalLight* FindDirectionalLightActor() const;
    USRCelestialBodyRegistrySubsystem* FindCelestialRegistry() const;
    bool ShouldDragFocusedSurface() const;
    void HandleFocusSurfaceDrag(const FVector2D& DragDelta);
    void ClearFocusSurfaceMotion();
    FVector GetFocusLocation() const;

    FSRFocusedActorChangedSignature FocusedActorChangedEvent;
    FVector DragTargetLocation;
    float ZoomDistanceTarget;
    FVector FocusArcTransitionStartLocation;
    float FocusArcTransitionElapsed;
    float FocusArcTransitionStartZoomDistance;
    float FocusArcTransitionFinalZoomDistance;
    float FocusArcTransitionPeakZoomDistance;
    float ScreenSpaceThicknessReferenceZoomDistance;
    float ScreenSpaceThicknessReferenceFieldOfView;
    FVector2D DragStartMouseScreenPosition;
    FVector DragStartFocusDragOffset;
    FVector DragStartTargetLocation;
    FVector2D FocusSurfaceInput;
    FVector2D FocusSurfaceAcceleratedInput;
    FVector2D FocusSurfaceAngularVelocity;
    FQuat FocusSurfaceRotation;
    FQuat FocusSurfaceTargetRotation;
    FQuat FocusSurfaceRigAlignmentStartRotation;
    FVector FocusSurfaceRigAlignmentStartOffset;
    FVector FocusSurfaceRigAlignmentAxis;
    FVector FocusSurfaceRotationSmoothVelocity;
    float FocusSurfaceRigAlignmentCurrentAngleRadians;
    float FocusSurfaceRigAlignmentTargetAngleRadians;
    FVector LastDynamicMeshVisibilityCameraLocation;
    FRotator LastDynamicMeshVisibilityCameraRotation;
    TWeakObjectPtr<AActor> LastDynamicMeshVisibilityFocusedActor;
    float LastDynamicMeshVisibilityZoomDistance;
    double LastDynamicMeshVisibilityUpdateTime;
    bool bHasDynamicMeshVisibilityState;
    bool bIsResettingFocusSurfaceRotation;
    bool bIsAligningFocusSurfaceRig;
    bool bIsFocusArcTransitionActive;
    bool bPendingFocusSurfaceGridAutoAlignment;
};
