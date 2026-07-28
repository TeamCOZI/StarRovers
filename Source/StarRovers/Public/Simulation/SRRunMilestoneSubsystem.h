#pragma once

#include "Automation/SRResourceDataAsset.h"
#include "CoreMinimal.h"
#include "Simulation/SRSystemScan.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRRunMilestoneSubsystem.generated.h"

class AActor;
class USRFacilityNetworkComponent;

UENUM(BlueprintType)
enum class ESRFirstFuelMilestone : uint8
{
	PlaceExtractor UMETA(DisplayName = "Place Extractor"),
	ExtractFirstCard UMETA(DisplayName = "Extract First Card"),
	PlaceFamilyProcessor UMETA(DisplayName = "Place Family Processor"),
	ProcessFirstCard UMETA(DisplayName = "Process First Card"),
	PlaceStellarFuelFabricator UMETA(DisplayName = "Place Stellar Fuel Fabricator"),
	FabricateFirstStellarFuel UMETA(DisplayName = "Fabricate First Stellar Fuel"),
	PlaceHub UMETA(DisplayName = "Place Hub"),
	LaunchFirstStellarFuel UMETA(DisplayName = "Launch First Stellar Fuel"),
	DeliverFirstStellarFuel UMETA(DisplayName = "Deliver First Stellar Fuel"),
	Complete UMETA(DisplayName = "Complete"),
};

/** Monotonic facts for the first complete extraction-to-Star vertical slice. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRFirstFuelMilestoneFacts
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bExtractorPlaced = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bFirstCardExtracted = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bFamilyProcessorPlaced = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bFirstCardProcessed = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bStellarFuelFabricatorPlaced = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bFirstStellarFuelFabricated = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bHubPlaced = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bFirstStellarFuelLaunched = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bFirstStellarFuelDelivered = false;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunMilestoneSaveData
{
	GENERATED_BODY()

	static constexpr int32 InitialVersion = 1;
	static constexpr int32 CurrentVersion = InitialVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	FSRFirstFuelMilestoneFacts Facts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	ESRResourceFamily FirstResourceFamily = ESRResourceFamily::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	bool bEmergencyRecoveryAttempted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	bool bEmergencyRecoveryApplied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	FName EmergencyRecoveryBodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	FName EmergencyRecoveryDepositOccupantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	FName EmergencyRecoveryResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Milestone|Save")
	int32 EmergencyRecoveryGrantedCardAmount = 0;

	bool IsSupportedVersion() const
	{
		return Version >= InitialVersion && Version <= CurrentVersion;
	}
};

/** Read-only UI view. Actor targets are navigation hints, never ownership. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRFirstFuelMilestoneSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	bool bIsTracking = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	ESRFirstFuelMilestone CurrentMilestone = ESRFirstFuelMilestone::PlaceExtractor;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	int32 CompletedMilestoneCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	int32 TotalMilestoneCount = 9;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	FSRFirstFuelMilestoneFacts Facts;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	ESRResourceFamily FirstResourceFamily = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	TObjectPtr<AActor> RecommendedBodyActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	TObjectPtr<AActor> TargetFacilityBodyActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	FName TargetFacilityOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	TObjectPtr<AActor> PrimaryStarActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	FSRSystemScanSnapshot InitialSystemScan;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Milestone")
	FSRInitialProgressRecoverySnapshot InitialProgressRecovery;

	bool IsComplete() const
	{
		return CurrentMilestone == ESRFirstFuelMilestone::Complete;
	}
};

/** Pure ordering and implication rules shared by runtime and tests. */
class STARROVERS_API FSRFirstFuelMilestoneModel final
{
public:
	static constexpr int32 TotalMilestoneCount = 9;

	static void ApplyConsistency(FSRFirstFuelMilestoneFacts& Facts);
	static ESRFirstFuelMilestone ResolveCurrentMilestone(const FSRFirstFuelMilestoneFacts& Facts);
	static int32 ResolveCompletedMilestoneCount(const FSRFirstFuelMilestoneFacts& Facts);
};

/**
 * World-owned, monotonic Run progress. It observes real Facility outputs and
 * also rescans authoritative state so loading and late UI creation are safe.
 */
UCLASS()
class STARROVERS_API USRRunMilestoneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Milestone")
	void RefreshFromWorld();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Run Milestone")
	FSRFirstFuelMilestoneSnapshot GetFirstFuelMilestoneSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Run Milestone")
	FSRFirstFuelMilestoneFacts GetFirstFuelMilestoneFacts() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|System Scan")
	FSRSystemScanSnapshot GetInitialSystemScanSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|System Scan")
	bool IsInitialSystemScanRecommendationActive() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|System Scan")
	bool TryActivateEmergencyProspectingRecovery();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Milestone")
	void ResetFirstFuelMilestone();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Milestone|Save")
	void ExportSaveData(FSRRunMilestoneSaveData& OutSaveData) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Milestone|Save")
	bool ImportSaveData(const FSRRunMilestoneSaveData& SaveData, FString& OutFailureReason);

private:
	struct FFacilityTarget
	{
		TWeakObjectPtr<AActor> BodyActor;
		FName OccupantId = NAME_None;

		bool IsValid() const
		{
			return BodyActor.IsValid() && !OccupantId.IsNone();
		}
	};

	void BindFacilityNetwork(USRFacilityNetworkComponent* FacilityNetwork);
	void HandleResourceProduced(
		USRFacilityNetworkComponent* FacilityNetwork,
		FName OccupantId,
		const FSRResourceInstance& ResourceInstance);
	void ObserveResource(const FSRResourceInstance& ResourceInstance);
	void ObserveFacility(AActor* BodyActor, USRFacilityNetworkComponent* FacilityNetwork, FName OccupantId);
	void SetTargetIfUnset(FFacilityTarget& Target, AActor* BodyActor, FName OccupantId);
	void ValidateFacilityTargets();
	bool IsFacilityTargetPresent(const FFacilityTarget& Target) const;
	bool IsExtractorTargetOperational(const FFacilityTarget& Target) const;
	bool IsFamilyProcessorTargetCompatible(const FFacilityTarget& Target) const;
	bool IsInitialSystemScanRecommendationStillViable() const;
	void TryBuildInitialSystemScan();

	UPROPERTY(Transient)
	FSRFirstFuelMilestoneFacts ObservedFacts;

	ESRResourceFamily FirstResourceFamily = ESRResourceFamily::None;
	TWeakObjectPtr<AActor> RecommendedDepositBody;
	TWeakObjectPtr<AActor> FallbackConstructibleBody;
	TWeakObjectPtr<AActor> FirstAutomationBody;
	TWeakObjectPtr<AActor> PrimaryStarActor;
	FSRSystemScanSnapshot InitialSystemScan;
	FSRInitialProgressRecoverySnapshot InitialProgressRecovery;
	FFacilityTarget ExtractorTarget;
	FFacilityTarget FamilyProcessorTarget;
	FFacilityTarget StellarFuelFabricatorTarget;
	FFacilityTarget HubTarget;
	TSet<TWeakObjectPtr<USRFacilityNetworkComponent>> BoundFacilityNetworks;
};
