#pragma once

#include "CoreMinimal.h"
#include "Simulation/SRRunBalanceSimulation.h"
#include "SRFiniteResourceEconomy.generated.h"

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFiniteResourceEconomyContract
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	int32 RequiredCardTypeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	int32 BatchesPerCardDepositSet = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	int32 RawUnitsPerUtilityDeposit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	double FabricationCycleSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	double BasicFuelEnergyPerBatch = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	double OptimizedFuelEnergyPerBatch = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	double BasicFuelPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Finite Resource Economy")
	double OptimizedFuelPerSecond = 0.0;
};

/** Derives finite Line throughput from the same Card, Facility, and Fabricator catalogs used by play. */
class STARROVERS_API FSRFiniteResourceEconomyModel final
{
public:
	static FSRFiniteResourceEconomyContract BuildReferenceContract();
	static bool BuildReferenceSupplyStage(
		bool bOptimized,
		int32 CardDepositSetCount,
		double StartTimeSeconds,
		double TransitDelaySeconds,
		FSRRunBalanceSupplyStage& OutStage,
		FString& OutFailureReason);
};
