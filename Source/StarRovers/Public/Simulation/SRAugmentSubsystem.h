#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Simulation/SRAugmentPackageContent.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRAugmentSubsystem.generated.h"

class USRStructureDataAsset;
struct FSRFacilityInstance;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment")
	ESRAugmentChoiceKind ChoiceKind = ESRAugmentChoiceKind::LegacyStructure;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FName PackageId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FName StrategyId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	ESRAugmentPackageRoleV2 PackageRole = ESRAugmentPackageRoleV2::Enabler;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	ESRAugmentOfferRoleV2 OfferRole = ESRAugmentOfferRoleV2::Legacy;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FText GrantSummary;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Augment V2")
	FText ExampleLinePreview;

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

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentChoiceSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	ESRAugmentChoiceKind ChoiceKind = ESRAugmentChoiceKind::ResourceV2Package;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	FName PackageId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	FName StructureId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	ESRAugmentOfferRoleV2 OfferRole = ESRAugmentOfferRoleV2::Legacy;
};

/** Selected progress, pity, pending choice, and diversity memory for one Run. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRAugmentSaveData
{
	GENERATED_BODY()

	static constexpr int32 InitialVersion = 1;
	static constexpr int32 CurrentVersion = InitialVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	TArray<FName> UnlockedStructureIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	TArray<FName> SelectedPackageIdsV2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	TArray<FName> PreviousOfferPackageIdsV2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	FName ActiveMacroDoctrineIdV2 = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	TArray<FSRAugmentChoiceSaveData> CurrentChoices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	int32 CurrentChoiceCycleIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	float HighTechBonusChancePercent = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment|Save")
	bool bPausedSimulationForCurrentChoice = false;

	bool IsSupportedVersion() const
	{
		return Version >= InitialVersion && Version <= CurrentVersion;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSRAugmentChoicesReadySignature, const TArray<FSRAugmentChoice>&, Choices, int32, CycleIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRAugmentChoiceSelectedSignature, const FSRAugmentChoice&, Choice);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSRAugmentUnlockedStructuresChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSRAugmentPackagesChangedSignature);

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

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Augment V2")
	FSRAugmentPackagesChangedSignature OnAugmentPackagesChanged;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	void RegisterStructureDataAssets(const TArray<USRStructureDataAsset*>& StructureDataAssets);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	void GenerateAugmentChoices(int32 CycleIndex);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	bool SelectAugmentChoiceByIndex(int32 ChoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	bool SelectAugmentChoiceByStructureId(FName StructureId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment V2")
	bool SelectAugmentChoiceByPackageId(FName PackageId);

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

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment V2")
	bool UnlockAugmentPackageV2(FName PackageId);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	bool IsAugmentPackageUnlockedV2(FName PackageId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	TArray<FName> GetSelectedAugmentPackageIdsV2() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	FName GetActiveMacroDoctrineIdV2() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	bool IsProcessTagRecipeUnlockedV2(FName TagId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	bool IsFuelImprintRecipeUnlockedV2(FName ImprintId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	bool IsFacilityContentUnlockedV2(FName FacilityContentId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	bool IsLogisticsModuleUnlockedV2(FName ModuleId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	bool IsRouteProfileUnlockedV2(FName ProfileId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment V2")
	FSRAugmentBuildContextV2 BuildResourceV2OfferContext() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment|Save")
	void ExportSaveData(FSRAugmentSaveData& OutSaveData) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment|Save")
	bool ImportSaveData(const FSRAugmentSaveData& SaveData, FString& OutFailureReason);

	bool IsFacilityRecipeUnlockedV2(
		const FSRFacilityInstance& FacilityInstance,
		FString& OutFailureReason) const;

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
	bool IsResourceV2RulesetActive() const;
	void GenerateResourceV2AugmentChoices(int32 CycleIndex);
	void PublishCurrentChoices(int32 CycleIndex);
	void UpdateResourceV2HighTechBonusAfterOffer(
		const TArray<FSRAugmentPackageDefinitionV2>& EligibleDefinitions,
		const TArray<FSRAugmentChoice>& GeneratedChoices);
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
	TSet<FName> SelectedAugmentPackageIdsV2;

	// Retained after a choice is made so the next offer prefers cards that were
	// not just rejected. This is a soft diversity memory, not persistent progress.
	UPROPERTY(Transient)
	TArray<FName> PreviousAugmentOfferPackageIdsV2;

	UPROPERTY(Transient)
	FName ActiveMacroDoctrineIdV2 = NAME_None;

	UPROPERTY(Transient)
	TArray<FSRAugmentChoice> CurrentChoices;

	UPROPERTY(Transient)
	int32 CurrentAugmentChoiceCycleIndex = 0;

	UPROPERTY(Transient)
	float HighTechBonusChancePercent = 0.0f;

	UPROPERTY(Transient)
	bool bPausedSimulationForCurrentChoice = false;

	bool bDebugUnlockAllFacilitiesWithoutAugments = false;
	bool bDebugUnlockAllAugmentPackagesV2 = false;
};
