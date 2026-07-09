#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SRSpaceshipActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
class USphereComponent;

UCLASS(Blueprintable)
class STARROVERS_API ASRSpaceshipActor : public AActor
{
	GENERATED_BODY()

public:
	ASRSpaceshipActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics")
	void SetRouteId(FName NewRouteId);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Space Logistics")
	FName GetRouteId() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics")
	void UpdateRouteVisual(const FVector& WorldLocation, const FVector& TargetWorldLocation, float TravelProgressRatio);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics")
	void UpdateRouteVisualWithDirection(
		const FVector& WorldLocation,
		const FVector& TargetWorldLocation,
		const FVector& TravelDirection,
		float TravelProgressRatio);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Space Logistics|Flight")
	float GetInitialSpeedUnitsPerSecond() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Space Logistics|Flight")
	float GetLaunchAccelerationUnitsPerSecondSquared() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SpaceshipMesh"))
	TObjectPtr<UStaticMeshComponent> SpaceshipMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "FocusCollision"))
	TObjectPtr<USphereComponent> FocusCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "TrailAnchor"))
	TObjectPtr<USceneComponent> TrailAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "TrailNiagaraComponent"))
	TObjectPtr<UNiagaraComponent> TrailNiagaraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Visual", meta = (DisplayName = "VisualScale"))
	FVector VisualScale = FVector(2.5f, 0.8f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Visual", meta = (DisplayName = "VisualForwardLocalAxis"))
	FVector VisualForwardLocalAxis = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Visual", meta = (DisplayName = "OrientVisualFromTrailAnchor"))
	bool bOrientVisualFromTrailAnchor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Flight", meta = (DisplayName = "InitialSpeedUnitsPerSecond", ClampMin = "100.0"))
	float InitialSpeedUnitsPerSecond = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Flight", meta = (DisplayName = "LaunchAccelerationUnitsPerSecondSquared", ClampMin = "1.0"))
	float LaunchAccelerationUnitsPerSecondSquared = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Focus", meta = (DisplayName = "FocusTraceRadius", ClampMin = "1.0"))
	float FocusTraceRadius = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Trail", meta = (DisplayName = "TrailEnabled"))
	bool bTrailEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Trail", meta = (DisplayName = "TrailNiagaraSystem"))
	TObjectPtr<UNiagaraSystem> TrailNiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Trail", meta = (DisplayName = "TrailMaterial"))
	TObjectPtr<UMaterialInterface> TrailMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Trail", meta = (DisplayName = "TrailColor"))
	FLinearColor TrailColor = FLinearColor(0.25f, 0.95f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Trail", meta = (DisplayName = "TrailWidth", ClampMin = "0.0"))
	float TrailWidth = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Trail", meta = (DisplayName = "TrailLifetime", ClampMin = "0.01"))
	float TrailLifetime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Trail", meta = (DisplayName = "TrailSpawnRate", ClampMin = "0.0"))
	float TrailSpawnRate = 70.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "RouteId"))
	FName RouteId = NAME_None;

private:
	void ConfigureTrail();
	void ApplyTrailUserParameters();
	void ResetTrailState();
	void ResetVisualMotionState();
	FVector ResolveEffectiveTravelDirection(
		const FVector& WorldLocation,
		const FVector& TargetWorldLocation,
		const FVector& FallbackTravelDirection);
	FVector ResolveVisualForwardLocalAxis() const;
	FQuat ResolveVisualRotation(const FVector& TravelDirection) const;
	FVector ResolveTrailSourceWorldLocation(const FVector& FallbackWorldLocation) const;
	void UpdateTrailParameters(const FVector& WorldLocation, float TravelProgressRatio);

	UPROPERTY(Transient)
	FVector LastVisualWorldLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasLastVisualWorldLocation = false;

	UPROPERTY(Transient)
	FVector LastTrailWorldLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	double LastTrailUpdateTimeSeconds = 0.0;

	UPROPERTY(Transient)
	bool bHasLastTrailWorldLocation = false;
};
