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
    void ResetFocusedCameraView();

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "DragDeltaAction"))
    TObjectPtr<UInputAction> DragDeltaAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "ZoomAction"))
    TObjectPtr<UInputAction> ZoomAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "FocusedSurfaceLookAction"))
    TObjectPtr<UInputAction> FocusedSurfaceLookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "ResetFocusCameraAction"))
    TObjectPtr<UInputAction> ResetFocusCameraAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera", meta = (DisplayName = "ZoomSpeed", ClampMin = "0.0"))
    float ZoomSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera", meta = (DisplayName = "CameraSurfacePadding", ClampMin = "0.0"))
    float CameraSurfacePadding;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "UseObliqueView"))
    bool bUseObliqueView;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "NearViewRotation"))
    FRotator NearViewRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "FarViewRotation"))
    FRotator FarViewRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "ObliqueViewStart", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float ObliqueViewStart;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|ObliqueView", meta = (DisplayName = "ObliqueViewEnd", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float ObliqueViewEnd;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Focus", meta = (DisplayName = "FocusFollowSmoothTime", ClampMin = "0.01"))
    float FocusFollowSmoothTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "SurfaceRotateSensitivity", ClampMin = "0.0"))
    float SurfaceRotateSensitivity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FocusedSurfaceLookSpeed", ClampMin = "0.0"))
    float FocusedSurfaceLookSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (DisplayName = "FocusedSurfaceLookMaxPitch", ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0"))
    float FocusedSurfaceLookMaxPitch;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera", meta = (DisplayName = "bIsDragging"))
    bool bIsDragging;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera", meta = (DisplayName = "bMappingContextApplied"))
    bool bMappingContextApplied;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "bIsRotatingFocusedBody"))
    bool bIsRotatingFocusedBody;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "bIsFocusedSurfaceLookActive"))
    bool bIsFocusedSurfaceLookActive;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera", meta = (DisplayName = "bHasDragStartMousePosition"))
    bool bHasDragStartMousePosition;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusedActor"))
    TObjectPtr<AActor> FocusedActor;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusDragOffset"))
    FVector FocusDragOffset;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera", meta = (DisplayName = "FixedPlaneX"))
    float FixedPlaneX;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusTrackingDelta"))
    FVector FocusTrackingDelta;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusTrackingDeltaVelocity"))
    FVector FocusTrackingDeltaVelocity;

private:
    void ApplyMappingContext();
    void BroadcastFocusedActorChangedIfNeeded(AActor* PreviousFocusedActor);
    void HandleDragHoldStarted();
    void HandleDragHoldCompleted();
    void HandleDragDelta(const FInputActionValue& Value);
    void HandleZoom(const FInputActionValue& Value);
    void HandleFocusedSurfaceLook(const FInputActionValue& Value);
    void HandleFocusedSurfaceLookCompleted();
    void HandleResetFocusedCameraView();
    bool HasExitedFocusedActorGravityField() const;
    bool GetMouseScreenPosition(FVector2D& OutMouseScreenPosition) const;
    FVector ConvertScreenDeltaToDragOffset(const FVector2D& ScreenDelta) const;
    float GetZoomSpeed() const;
    float GetMinimumZoomDistance() const;
    float ClampZoomDistance(float ZoomDistance) const;
    float GetSpaceSphereRadius() const;
    bool ResolveSpaceBoundary(FVector& OutCenter, float& OutRadius) const;
    bool ResolveCelestialCameraAvoidanceSphere(const AActor* Actor, FVector& OutCenter, float& OutRadius) const;
    FVector GetCameraDirectionFromPivot() const;
    float ClampZoomDistanceAgainstCelestialBodies(float ZoomDistance, const FVector& CandidatePawnLocation) const;
    float GetObliqueViewBlendAlpha(float ZoomDistance) const;
    FRotator GetViewRotationForZoom(float ZoomDistance) const;
    void ApplyZoomDrivenViewRotation(float ZoomDistance);
    bool ShouldAllowFocusedSurfaceLook() const;
    void UpdateFocusedSurfaceLook(float DeltaSeconds);
    void RefreshScreenSpaceThicknessReferenceView();
    void UpdateDynamicMeshVisibility();
    bool ApplyCelestialBodyMeshVisibility(AActor*& OutDirectionalLightTarget);
    bool ShouldUseDynamicMesh(const AActor* BodyActor, float& OutScreenSizeRatio) const;
    void ConfigureDirectionalLight(AActor* LightingTarget);
    ADirectionalLight* FindDirectionalLightActor() const;
    USRCelestialBodyRegistrySubsystem* FindCelestialRegistry() const;
    bool ShouldRotateFocusedBody() const;
    void HandleFocusedBodyRotation(const FVector2D& DragDelta);
    FVector GetFocusLocation() const;

    FSRFocusedActorChangedSignature FocusedActorChangedEvent;
    FVector DragTargetLocation;
    float ZoomDistanceTarget;
    float ScreenSpaceThicknessReferenceZoomDistance;
    float ScreenSpaceThicknessReferenceFieldOfView;
    FVector2D DragStartMouseScreenPosition;
    FVector DragStartFocusDragOffset;
    FVector DragStartTargetLocation;
    FVector2D FocusedSurfaceLookInput;
    FRotator FocusedSurfaceLookOffset;
};
