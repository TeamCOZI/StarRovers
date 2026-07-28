#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Simulation/SRSimulationSettings.h"
#include "SRResourceSystemValidationActor.generated.h"

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceSystemValidationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	bool bPassed = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 CheckCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 FailureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	ESRResourceRulesetVersion ActiveRuleset = ESRResourceRulesetVersion::Legacy;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 LogisticsSaveVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 RunSaveVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 StructureSaveVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 RunSavePayloadBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 CelestialBodyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	bool bHasPrimaryStar = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 FacilityNetworkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 RegisteredFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 AuthoredResourceV2ResourceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 AuthoredResourceV2FacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 AuthoredResourceV2StructureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 AuthoredResourceV2DepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 BuildableResourceV2StructureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 RuntimeResourceV2DepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	int32 RuntimeResourceV2DepositTypeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	TArray<FString> FailureMessages;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource System|Validation")
	FString Summary;
};

UCLASS(Blueprintable)
class STARROVERS_API ASRResourceSystemValidationActor : public AActor
{
	GENERATED_BODY()

public:
	ASRResourceSystemValidationActor();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "StarRovers|Resource System|Validation")
	bool RunValidation();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Resource System|Validation")
	FSRResourceSystemValidationReport GetLastValidationReport() const;

	static bool ValidateWorld(
		UWorld* World,
		bool bRequirePrimaryStar,
		bool bRequireFacilityNetwork,
		FSRResourceSystemValidationReport& OutReport);

	static void LogReport(const FSRResourceSystemValidationReport& Report);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource System|Validation")
	bool bRunOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource System|Validation")
	bool bRequirePrimaryStar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource System|Validation")
	bool bRequireFacilityNetwork = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "StarRovers|Resource System|Validation")
	FSRResourceSystemValidationReport LastValidationReport;
};
