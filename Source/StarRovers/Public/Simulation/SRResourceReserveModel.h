#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "SRResourceReserveModel.generated.h"

UENUM(BlueprintType)
enum class ESRResourceReservePressure : uint8
{
	Unavailable UMETA(DisplayName = "Unavailable"),
	Healthy UMETA(DisplayName = "Healthy"),
	Low UMETA(DisplayName = "Low"),
	Critical UMETA(DisplayName = "Critical"),
	Depleted UMETA(DisplayName = "Depleted"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceReserveEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	ESRResourceClass ResourceClass = ESRResourceClass::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	ESRResourceFamily Family = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int32 DepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int32 ActiveDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int32 DepletedDepositCount = 0;

	/** Finite authored amount only. Infinite Legacy deposits are represented by flags. */
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 TotalAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 RemainingAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	bool bHasInfiniteDeposit = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	bool bHasInfiniteRemaining = false;
};

/** A deterministic aggregate. It observes deposits but never owns or consumes them. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceReserveSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	bool bHasDeposits = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int32 DepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int32 ActiveDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int32 DepletedDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int32 InfiniteDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 TotalFiniteAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 RemainingFiniteAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 RemainingCardAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 RemainingUtilityAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int32 CoveredReferenceCardTypeCount = 0;

	/** Maximum complete five-Card batches from current system reserves. */
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 PotentialFuelBatchCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 PotentialTotalFuelBatchCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	bool bPotentialFuelBatchesInfinite = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	FName LimitingReferenceCardId = NAME_None;

	/** Maximum Common Ore + Biomass recipe pairs still available. */
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	int64 PotentialIndustrialSupplyCycleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	bool bPotentialIndustrialSupplyCyclesInfinite = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	float RemainingRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	ESRResourceReservePressure Pressure = ESRResourceReservePressure::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource Reserve")
	TArray<FSRResourceReserveEntry> Entries;
};

class STARROVERS_API FSRResourceReserveModel final
{
public:
	static FSRResourceReserveSnapshot BuildSnapshot(
		const TArray<FSRResourceDepositInstance>& ResourceDeposits);
	static ESRResourceReservePressure ResolvePressure(bool bHasDeposits, float RemainingRatio);
	static FString BuildPressureLabel(ESRResourceReservePressure Pressure);
};
