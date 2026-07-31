#pragma once

#include "CoreMinimal.h"
#include "Save/SRRunSaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRRunSaveSubsystem.generated.h"

class AActor;
class ASRSolarSystemGenerator;

UCLASS(BlueprintType)
class STARROVERS_API USRRunSaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Run")
	bool CaptureRunData(FSRRunSaveData& OutRunData);

	bool CanRestoreRunData(const FSRRunSaveData& RunData, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Run")
	bool RestoreRunData(const FSRRunSaveData& RunData);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Run")
	bool SaveRunToSlot(const FString& SlotName = TEXT("StarRoversRun"), int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Run")
	bool LoadRunFromSlot(const FString& SlotName = TEXT("StarRoversRun"), int32 UserIndex = 0);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Save|Run")
	FString GetLastSaveError() const;

private:
	bool ApplyRunDataUnchecked(const FSRRunSaveData& RunData, FString& OutFailureReason);
	ASRSolarSystemGenerator* FindRuntimeGenerator(FName PreferredActorName, FString& OutFailureReason) const;
	bool RegenerateTopologyForRunData(const FSRRunSaveData& RunData, FString& OutFailureReason);
	bool GatherCelestialBodies(TArray<AActor*>& OutBodies, FString& OutFailureReason);
	FSRRunCelestialBodyKey BuildBodyKey(const AActor* BodyActor) const;
	AActor* ResolveSavedBody(
		const FSRRunCelestialBodyKey& BodyKey,
		const TArray<AActor*>& CurrentBodies,
		FString& OutFailureReason) const;
	bool ValidateSpaceLogisticsAgainstRun(
		const FSRRunSaveData& RunData,
		FString& OutFailureReason) const;

	UPROPERTY(Transient)
	FString LastSaveError;
};
