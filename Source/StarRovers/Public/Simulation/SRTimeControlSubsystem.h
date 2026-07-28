#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRTimeControlSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRGameCycleAdvancedSignature, int32, CurrentCycleIndex);

USTRUCT(BlueprintType)
struct STARROVERS_API FSRTimeControlSaveData
{
	GENERATED_BODY()

	static constexpr int32 InitialVersion = 1;
	static constexpr int32 CurrentVersion = InitialVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Simulation|Save")
	int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Simulation|Save")
	float TimeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Simulation|Save")
	float SecondsPerPeriod = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Simulation|Save")
	float CycleProgressSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Simulation|Save")
	int32 CurrentCycleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Simulation|Save")
	bool bSimulationPaused = false;

	bool IsSupportedVersion() const
	{
		return Version >= InitialVersion && Version <= CurrentVersion;
	}
};

UCLASS()
class STARROVERS_API USRTimeControlSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Simulation")
	FSRGameCycleAdvancedSignature OnGameCycleAdvanced;

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

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation")
	void AdvanceGameCycles(int32 CycleCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation")
	void ResetGameCycle(int32 NewCurrentCycleIndex = 0);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	float GetTimeScale() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	float GetEffectiveTimeScale() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	float GetSecondsPerPeriod() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	bool IsSimulationPaused() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	int32 GetCurrentCycleIndex() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	float GetCycleProgressSeconds() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Simulation")
	float GetCycleProgressRatio() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation|Save")
	void ExportSaveData(FSRTimeControlSaveData& OutSaveData) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Simulation|Save")
	bool ImportSaveData(const FSRTimeControlSaveData& SaveData, FString& OutFailureReason);

private:
	UPROPERTY(Transient)
	float TimeScale = 1.0f;

	UPROPERTY(Transient)
	float SecondsPerPeriod = 20.0f;

	UPROPERTY(Transient)
	float CycleProgressSeconds = 0.0f;

	UPROPERTY(Transient)
	int32 CurrentCycleIndex = 0;

	UPROPERTY(Transient)
	bool bSimulationPaused = false;
};
