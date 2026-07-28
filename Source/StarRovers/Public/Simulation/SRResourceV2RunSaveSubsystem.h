#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkSaveData.h"
#include "Celestial/SRStar.h"
#include "GameFramework/SaveGame.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Simulation/SRRunMilestoneSubsystem.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureInstanceSaveData.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRResourceV2RunSaveSubsystem.generated.h"

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceV2BodySaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FName BodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FName ActorName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	bool bHasStructureManager = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FSRStructureInstanceManagerSaveData StructureManager;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	bool bHasFacilityNetwork = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FSRFacilityNetworkSaveData FacilityNetwork;
};

/** One authoritative Resource V2 Run checkpoint. Telemetry is derived on load. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceV2RunSaveData
{
	GENERATED_BODY()

	static constexpr int32 InitialVersion = 1;
	static constexpr int32 FiniteResourceEconomyVersion = 2;
	static constexpr int32 CurrentVersion = FiniteResourceEconomyVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	ESRResourceRulesetVersion SourceRulesetVersion =
		ESRResourceRulesetVersion::ResourceV2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FName PrimaryStarBodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	TArray<FSRResourceV2BodySaveData> Bodies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FSRSpaceLogisticsSaveData SpaceLogistics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FSRAugmentSaveData Augments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FSRTimeControlSaveData TimeControl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FSRRunMilestoneSaveData RunMilestones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Save")
	FSRStellarRuntimeSaveData PrimaryStar;

	bool IsSupportedVersion() const
	{
		return Version >= InitialVersion && Version <= CurrentVersion;
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceV2RunRestoreReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	bool bRollbackSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	int32 RestoredBodyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	int32 RestoredStructureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	int32 RestoredDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	int32 MigratedLegacyPlaceholderCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	int32 MigratedLegacyInfiniteToFiniteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	bool bTelemetryRebuilt = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Save")
	FString FailureReason;
};

/** The SaveGame stores a checked binary DTO so existing nested save structs work unchanged. */
UCLASS()
class STARROVERS_API USRResourceV2RunSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentPayloadFormatVersion = 1;

	UPROPERTY(SaveGame)
	int32 PayloadFormatVersion = CurrentPayloadFormatVersion;

	UPROPERTY(SaveGame)
	uint32 PayloadChecksum = 0;

	UPROPERTY(SaveGame)
	TArray<uint8> Payload;
};

class STARROVERS_API FSRResourceV2RunSaveCodec final
{
public:
	static bool Encode(
		const FSRResourceV2RunSaveData& SaveData,
		TArray<uint8>& OutPayload,
		uint32& OutChecksum,
		FString& OutFailureReason);

	static bool Decode(
		TConstArrayView<uint8> Payload,
		uint32 ExpectedChecksum,
		FSRResourceV2RunSaveData& OutSaveData,
		FString& OutFailureReason);
};

/**
 * World-level save coordinator. Restore is synchronous and rollback-backed so
 * no child system can leave a partially loaded automation network.
 */
UCLASS()
class STARROVERS_API USRResourceV2RunSaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Save")
	bool CaptureRunState(
		FSRResourceV2RunSaveData& OutSaveData,
		FString& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Save")
	bool RestoreRunState(
		const FSRResourceV2RunSaveData& SaveData,
		FSRResourceV2RunRestoreReport& OutReport);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Save")
	bool SaveRunToSlot(
		const FString& SlotName,
		int32 UserIndex,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Run Save")
	bool LoadRunFromSlot(
		const FString& SlotName,
		int32 UserIndex,
		FSRResourceV2RunRestoreReport& OutReport);

private:
	bool RestoreRunStateInternal(
		const FSRResourceV2RunSaveData& SaveData,
		FSRResourceV2RunRestoreReport& OutReport);
};
