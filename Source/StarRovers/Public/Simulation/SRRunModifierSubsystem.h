#pragma once

#include "CoreMinimal.h"
#include "Simulation/SRRunModifierDataAssets.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRRunModifierSubsystem.generated.h"

USTRUCT(BlueprintType)
struct STARROVERS_API FSRActiveTrialState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Trial")
	FName TrialId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Trial")
	int32 StartCycleIndex = 0;

	// The Trial is active for cycles [StartCycleIndex, EndCycleIndexExclusive).
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Trial")
	int32 EndCycleIndexExclusive = 1;
};

namespace StarRovers::Save::RunModifiers
{
	inline constexpr int32 CurrentVersion = 1;
	inline bool IsSupportedVersion(int32 Version) { return Version == CurrentVersion; }
}

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunModifierAugmentStackSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run Modifier")
	FName AugmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run Modifier")
	int32 StackCount = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunModifierSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run Modifier")
	int32 Version = StarRovers::Save::RunModifiers::CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run Modifier")
	TArray<FName> UnlockedTechnologyIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run Modifier")
	TArray<FSRRunModifierAugmentStackSaveData> AugmentStacks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run Modifier")
	TArray<FSRActiveTrialState> ActiveTrials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run Modifier")
	int32 ContextRevision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "StarRovers|Save|Run Modifier")
	int32 NextContextRevision = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRRunModifierContextChangedSignature, const FSRRunModifierContext&, Context);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRTechnologyUnlockedSignature, FName, TechnologyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSRTrialStateChangedSignature, const FSRActiveTrialState&, TrialState);

UCLASS(BlueprintType)
class STARROVERS_API USRRunModifierSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Run Modifier")
	FSRRunModifierContextChangedSignature OnRunModifierContextChanged;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Technology")
	FSRTechnologyUnlockedSignature OnTechnologyUnlocked;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Trial")
	FSRTrialStateChangedSignature OnTrialActivated;

	UPROPERTY(BlueprintAssignable, Category = "StarRovers|Trial")
	FSRTrialStateChangedSignature OnTrialExpired;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Modifier|Registry")
	void RegisterTechnologyDataAssets(const TArray<USRTechnologyDataAsset*>& DataAssets);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Modifier|Registry")
	void RegisterAugmentDataAssets(const TArray<USRRunAugmentDataAsset*>& DataAssets);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Modifier|Registry")
	void RegisterTrialDataAssets(const TArray<USRTrialDataAsset*>& DataAssets);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Technology")
	bool UnlockTechnology(FName TechnologyId);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Technology")
	bool IsTechnologyUnlocked(FName TechnologyId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Technology")
	bool IsStructureUnlockedByTechnology(FName StructureId) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	bool ApplyAugment(FName AugmentId);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	int32 GetAugmentStackCount(FName AugmentId) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	int32 GetTotalAugmentStackCount() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Trial")
	bool ActivateTrial(FName TrialId, int32 StartCycleIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Trial")
	bool DeactivateTrial(FName TrialId);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Trial")
	TArray<FSRActiveTrialState> GetActiveTrials() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Run Modifier")
	FSRRunModifierContext GetRunModifierContext() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Run Modifier")
	FSRResolvedRunModifiers ResolveModifiers(const FSRRunModifierQuery& Query) const;

	TArray<USRRunAugmentDataAsset*> GetRegisteredAugmentDataAssets() const;
	const USRRunAugmentDataAsset* FindAugmentDataAsset(FName AugmentId) const;

	static FSRRunModifierContext GetContextForObject(const UObject* WorldContextObject);
	static FSRResolvedRunModifiers ResolveForObject(
		const UObject* WorldContextObject,
		const FSRRunModifierQuery& Query = FSRRunModifierQuery());

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Run Modifier")
	void ExportSaveData(FSRRunModifierSaveData& OutSaveData) const;

	bool CanImportSaveData(const FSRRunModifierSaveData& SaveData, FString& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Run Modifier")
	bool ImportSaveData(const FSRRunModifierSaveData& SaveData);

private:
	UFUNCTION()
	void HandleGameCycleAdvanced(int32 CurrentCycleIndex);

	void RegisterConfiguredDataAssets();
	void UnlockConfiguredDefaultTechnologies();
	bool ValidateEffects(FName SourceId, ESRRunModifierSourceKind SourceKind, const TArray<FSRRunModifierEffect>& Effects) const;
	void RebuildContext();
	void RebuildTechnologyStructureUnlocks();
	void BindTimeControlSubsystem();
	void UnbindTimeControlSubsystem();
	bool BuildStateFromSaveData(
		const FSRRunModifierSaveData& SaveData,
		TSet<FName>& OutUnlockedTechnologyIds,
		TMap<FName, int32>& OutAugmentStacksById,
		TMap<FName, FSRActiveTrialState>& OutActiveTrialsById,
		FSRRunModifierContext& OutContext,
		FString& OutFailureReason) const;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USRTechnologyDataAsset>> TechnologiesById;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USRRunAugmentDataAsset>> AugmentsById;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USRTrialDataAsset>> TrialsById;

	UPROPERTY(Transient)
	TSet<FName> UnlockedTechnologyIds;

	UPROPERTY(Transient)
	TSet<FName> TechnologyUnlockedStructureIds;

	UPROPERTY(Transient)
	TMap<FName, int32> AugmentStacksById;

	UPROPERTY(Transient)
	TMap<FName, FSRActiveTrialState> ActiveTrialsById;

	UPROPERTY(Transient)
	FSRRunModifierContext CurrentContext;

	int32 NextContextRevision = 1;
};
