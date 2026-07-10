#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRAugmentSubsystem.generated.h"

class USRStructureDataAsset;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment", meta = (DisplayName = "Rarity"))
	ESRFacilityRarity Rarity = ESRFacilityRarity::Basic;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSRAugmentChoicesReadySignature, const TArray<FSRAugmentChoice>&, Choices, int32, CycleIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRAugmentChoiceSelectedSignature, const FSRAugmentChoice&, Choice);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSRAugmentUnlockedStructuresChangedSignature);

UCLASS(BlueprintType)
class STARROVERS_API USRAugmentSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Augment")
	FSRAugmentChoicesReadySignature OnAugmentChoicesReady;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Augment")
	FSRAugmentChoiceSelectedSignature OnAugmentChoiceSelected;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Augment")
	FSRAugmentUnlockedStructuresChangedSignature OnUnlockedStructuresChanged;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	void RegisterStructureDataAssets(const TArray<USRStructureDataAsset*>& StructureDataAssets);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	void GenerateAugmentChoices(int32 CycleIndex);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	bool SelectAugmentChoiceByIndex(int32 ChoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	bool SelectAugmentChoiceByStructureId(FName StructureId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	bool UnlockStructure(USRStructureDataAsset* StructureDataAsset);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	bool IsStructureUnlocked(const USRStructureDataAsset* StructureDataAsset) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	bool IsStructureUnlockedById(FName StructureId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	bool IsAugmentChoicePending() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	TArray<FSRAugmentChoice> GetCurrentAugmentChoices() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	int32 GetCurrentAugmentChoiceCycleIndex() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	float GetHighTechEffectiveChancePercent() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	float GetHighTechBonusChancePercent() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "AugmentIntervalCycles", ClampMin = "1"))
	int32 AugmentIntervalCycles = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "ChoicesPerOffer", ClampMin = "1"))
	int32 ChoicesPerOffer = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Rarity", meta = (DisplayName = "BasicChancePercent", ClampMin = "0.0"))
	float BasicChancePercent = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Rarity", meta = (DisplayName = "AdvancedChancePercent", ClampMin = "0.0"))
	float AdvancedChancePercent = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Rarity", meta = (DisplayName = "HighTechBaseChancePercent", ClampMin = "0.0", ClampMax = "100.0"))
	float HighTechBaseChancePercent = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Rarity", meta = (DisplayName = "HighTechPityIncreasePercent", ClampMin = "0.0"))
	float HighTechPityIncreasePercent = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Rarity", meta = (DisplayName = "HighTechPityCapPercent", ClampMin = "0.0", ClampMax = "100.0"))
	float HighTechPityCapPercent = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "bPauseSimulationDuringChoice"))
	bool bPauseSimulationDuringChoice = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "AugmentRandomSeed"))
	int32 AugmentRandomSeed = 47219;

private:
	UFUNCTION()
	void HandleGameCycleAdvanced(int32 CurrentCycleIndex);

	void BindTimeControlSubsystem();
	void UnbindTimeControlSubsystem();
	TArray<USRStructureDataAsset*> GetEligibleAugmentCandidates() const;
	bool IsStructureUnlockControlled(const USRStructureDataAsset* StructureDataAsset) const;
	bool IsDebugUnlockableFacility(const USRStructureDataAsset* StructureDataAsset) const;
	bool IsAugmentCandidate(const USRStructureDataAsset* StructureDataAsset) const;
	bool DrawCandidateIndex(const TArray<USRStructureDataAsset*>& Candidates, FRandomStream& RandomStream, int32& OutCandidateIndex) const;
	FSRAugmentChoice BuildAugmentChoice(USRStructureDataAsset* StructureDataAsset) const;
	void UpdateHighTechBonusAfterOffer(const TArray<USRStructureDataAsset*>& InitialCandidates, const TArray<FSRAugmentChoice>& GeneratedChoices);
	void ClearPendingAugmentChoice();
	void ResumeSimulationAfterChoiceIfNeeded();

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRStructureDataAsset>> RegisteredStructureDataAssets;

	UPROPERTY(Transient)
	TSet<FName> UnlockedStructureIds;

	UPROPERTY(Transient)
	TArray<FSRAugmentChoice> CurrentChoices;

	UPROPERTY(Transient)
	int32 CurrentAugmentChoiceCycleIndex = 0;

	UPROPERTY(Transient)
	float HighTechBonusChancePercent = 0.0f;

	UPROPERTY(Transient)
	bool bPausedSimulationForCurrentChoice = false;

	bool bDebugUnlockAllFacilitiesWithoutAugments = false;
};
