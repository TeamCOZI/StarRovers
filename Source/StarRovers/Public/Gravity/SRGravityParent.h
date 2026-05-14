#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SRGravityParent.generated.h"

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

class ULineBatchComponent;

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRGravityParent : public UActorComponent
{
	GENERATED_BODY()

public:
	USRGravityParent();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Gravity")
	void RecomputeDerivedValues();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity")
	float GetMass() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity")
	float GetGravityStrength() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity")
	float GetGravityRadius() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity")
	FVector GetGravityAccelerationAtWorldLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity|Visual")
	bool ShouldShowGravityLine() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity|Visual")
	FLinearColor GetGravityLineColor() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity|Visual")
	float GetGravityLineOpacity() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity|Visual")
	int32 GetGravityLineSegments() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity|Visual")
	float GetGravityLineThickness() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Gravity|Visual")
	void RefreshGravityLine();

	static void GetRegisteredSourcesForWorld(const UWorld* World, TArray<USRGravityParent*>& OutSources);

protected:
	UPROPERTY()
	float Mass;

	UPROPERTY()
	float GravityRatio;

	UPROPERTY()
	float GravityRadiusRatio;

	UPROPERTY()
	float Gravity;

	UPROPERTY()
	float GravityRadius;

	UPROPERTY()
	bool bShowGravityLine;

	UPROPERTY()
	FLinearColor GravityLineColor;

	UPROPERTY()
	float GravityLineOpacity;

	UPROPERTY()
	int32 GravityLineSegments;

	UPROPERTY()
	float GravityLineThickness;

private:
	void EnsureGravityLineBatch();
	void ReleaseGravityLineBatch();

	static TArray<TWeakObjectPtr<USRGravityParent>> RegisteredSources;

	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> GravityLineBatch;
};
