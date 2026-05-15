#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "SRCameraPawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USceneComponent;
class USpringArmComponent;

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

    FSRFocusedActorChangedSignature& OnFocusedActorChanged();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> DragHoldAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> DragDeltaAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> ZoomAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera", meta = (ClampMin = "0.0"))
    float ZoomSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera", meta = (ClampMin = "0.0"))
    float CameraSurfacePadding;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|Oblique View", meta = (DisplayName = "Use Oblique View"))
    bool bUseObliqueView;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|Oblique View")
    FRotator NearViewRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|Oblique View")
    FRotator FarViewRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|Oblique View", meta = (DisplayName = "Oblique View Start", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float ObliqueViewStart;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Camera|Oblique View", meta = (DisplayName = "Oblique View End", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float ObliqueViewEnd;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Focus", meta = (ClampMin = "0.01"))
    float FocusFollowSmoothTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Surface", meta = (ClampMin = "0.0"))
    float SurfaceRotateSensitivity;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera")
    bool bIsDragging;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera")
    bool bMappingContextApplied;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface")
    bool bIsRotatingFocusedBody;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera")
    bool bHasDragStartMousePosition;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
    TObjectPtr<AActor> FocusedActor;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
    FVector FocusDragOffset;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Camera")
    float FixedPlaneX;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
    FVector FocusTrackingDelta;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
    FVector FocusTrackingDeltaVelocity;

private:
    void ApplyMappingContext();
    void BroadcastFocusedActorChangedIfNeeded(AActor* PreviousFocusedActor);
    void HandleDragHoldStarted();
    void HandleDragHoldCompleted();
    void HandleDragDelta(const FInputActionValue& Value);
    void HandleZoom(const FInputActionValue& Value);
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
    void RefreshScreenSpaceThicknessReferenceView();
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
};
