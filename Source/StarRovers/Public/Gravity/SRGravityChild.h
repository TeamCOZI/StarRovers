#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SRGravityChild.generated.h"

class UPrimitiveComponent;

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRGravityChild : public UActorComponent
{
	GENERATED_BODY()

public:
	USRGravityChild();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Gravity")
	void SetGravityEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity")
	bool IsGravityEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Gravity")
	void ResetVelocity();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity")
	FVector GetLinearVelocity() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity")
	FVector GetCurrentGravityAcceleration() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Gravity")
	AActor* GetCurrentPrimaryGravityActor() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Gravity")
	bool bGravityEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Gravity")
	bool bUsePhysicsIfAvailable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Gravity")
	bool bUseStrongestSourceOnly;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Gravity", meta = (ClampMin = "0.0"))
	float ManualLinearDamping;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Gravity")
	FVector LinearVelocity;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Gravity")
	FVector CurrentGravityAcceleration;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Gravity")
	TObjectPtr<AActor> CurrentPrimaryGravityActor;

private:
	void UpdateTickState();
	UPrimitiveComponent* FindSimulatingPrimitive() const;
};
