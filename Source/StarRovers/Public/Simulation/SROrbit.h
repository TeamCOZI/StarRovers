#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SROrbit.generated.h"

class USRTimeControlSubsystem;
class ULineBatchComponent;

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USROrbit : public UActorComponent
{
	GENERATED_BODY()

public:
	USROrbit();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Orbit")
	void ConfigureOrbit(AActor* NewParentBody, float NewOrbitRadius, float NewOrbitPeriodInPeriods, float NewStartingPhaseDegrees);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Orbit|Visual")
	void ConfigureOrbitLineVisual(bool bNewShowOrbitLine, FLinearColor NewOrbitLineColor, float NewOrbitLineOpacity, int32 NewOrbitLineSegments, float NewOrbitLineThickness);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Orbit")
	void ResetOrbitSimulation();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit|Visual")
	bool ShouldShowOrbitLine() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit|Visual")
	FLinearColor GetOrbitLineColor() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit|Visual")
	float GetOrbitLineOpacity() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit|Visual")
	int32 GetOrbitLineSegments() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit|Visual")
	float GetOrbitLineThickness() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Orbit|Visual")
	void RefreshOrbitLineVisual();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	AActor* GetParentBody() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	float GetOrbitRadius() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	float GetOrbitPeriodInPeriods() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	float GetOrbitPeriodSeconds() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	float GetStartingPhaseDegrees() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	bool HasOrbit() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	FVector ComputeOrbitCenterLocation() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	FVector ComputeOrbitLocationAtCurrentPhase() const;

private:
	void EnsureOrbitLineBatcher();
	void ReleaseOrbitLineBatcher();
	USRTimeControlSubsystem* FindTimeControlSubsystem() const;
	float ResolveSecondsPerPeriod() const;
	void RefreshDerivedState();
	void UpdateTickDependency();
	float ResolveSimulationDeltaSeconds(float DeltaSeconds) const;
	float ComputeOrbitAngleRadians() const;
	FVector ComputeOrbitLocationAtAngle(float AngleRadians) const;

	UPROPERTY()
	TObjectPtr<AActor> ParentBody = nullptr;

	UPROPERTY()
	float OrbitRadius = 0.0f;

	UPROPERTY()
	float OrbitPeriodInPeriods = 0.0f;

	UPROPERTY()
	float StartingPhaseDegrees = 0.0f;

	UPROPERTY()
	float OrbitPeriodSeconds = 0.0f;

	UPROPERTY()
	bool bShowOrbitLine = true;

	UPROPERTY()
	FLinearColor OrbitLineColor = FLinearColor(0.2f, 0.75f, 1.0f, 1.0f);

	UPROPERTY()
	float OrbitLineOpacity = 0.85f;

	UPROPERTY()
	int32 OrbitLineSegments = 96;

	UPROPERTY()
	float OrbitLineThickness = 20.0f;

	UPROPERTY(Transient)
	FVector OrbitAnchorLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float OrbitElapsedTimeSeconds = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<AActor> OrbitTickDependencyActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> OrbitLineBatcher;

	mutable TWeakObjectPtr<USRTimeControlSubsystem> CachedTimeControlSubsystem;
};
