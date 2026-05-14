#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRTimeControlSubsystem.generated.h"

UCLASS()
class STARROVERS_API USRTimeControlSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation")
	void PauseSimulation();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation")
	void ResumeSimulation();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation")
	void SetSimulationPaused(bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation")
	void SetTimeScale(float NewTimeScale);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation")
	void SetSimulationSpeedPreset(float NewTimeScale);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation")
	void SetSecondsPerPeriod(float NewSecondsPerPeriod);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	float GetTimeScale() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	float GetEffectiveTimeScale() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	float GetSecondsPerPeriod() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	bool IsSimulationPaused() const;

private:
	UPROPERTY(Transient)
	float TimeScale = 1.0f;

	UPROPERTY(Transient)
	float SecondsPerPeriod = 20.0f;

	UPROPERTY(Transient)
	bool bSimulationPaused = false;
};
