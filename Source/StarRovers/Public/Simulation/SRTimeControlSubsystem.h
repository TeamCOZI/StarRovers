#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRTimeControlSubsystem.generated.h"

namespace StarRovers::Save::TimeControl
{
	inline constexpr int32 CurrentVersion = 1;
	inline bool IsSupportedVersion(int32 Version) { return Version == CurrentVersion; }
}

USTRUCT(BlueprintType)
struct STARROVERS_API FSRTimeControlSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Time")
	int32 Version = StarRovers::Save::TimeControl::CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Time")
	float TimeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Time")
	float SecondsPerPeriod = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Time")
	float CycleProgressSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Time")
	int32 CurrentCycleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Time")
	bool bSimulationPaused = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRGameCycleAdvancedSignature, int32, CurrentCycleIndex);

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

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Time")
	void ExportSaveData(FSRTimeControlSaveData& OutSaveData) const;

	bool CanImportSaveData(const FSRTimeControlSaveData& SaveData, FString& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Time")
	bool ImportSaveData(const FSRTimeControlSaveData& SaveData);

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
