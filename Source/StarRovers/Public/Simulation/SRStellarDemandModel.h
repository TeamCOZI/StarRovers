#pragma once

#include "CoreMinimal.h"
#include "SRStellarDemandModel.generated.h"

UENUM(BlueprintType)
enum class ESRStellarDemandPhaseV2 : uint8
{
	Grace UMETA(DisplayName = "Ignition Grace"),
	Expansion UMETA(DisplayName = "Expansion Ramp"),
	Plateau UMETA(DisplayName = "Stabilization Plateau"),
};

/** Readable, capped demand curve used by the shipped finite Run. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarDemandCurveV2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Pressure", meta = (ClampMin = "0.0"))
	double InitialDemandPerSecond = 50.0;

	/** Completed cycles that retain the initial demand before the ramp starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Pressure", meta = (ClampMin = "0"))
	int32 GraceCycleCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Pressure", meta = (ClampMin = "0.0"))
	double DemandIncreasePerCycle = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Pressure", meta = (ClampMin = "0.0"))
	double MaximumDemandPerSecond = 100.0;
};

/** Survival-buffer rules kept separate from cumulative Run objective fuel. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarPressureRulesV2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Pressure", meta = (ClampMin = "1.0"))
	double FuelReserveCapacity = 20000.0;

	/** One-time reserve granted when Main Sequence first enters Red Giant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Pressure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double RedGiantEmergencyReserveRatio = 1.0;
};

/** Shared demand math used by the live Star and deterministic balance runs. */
class STARROVERS_API FSRStellarDemandModel final
{
public:
	static FSRStellarDemandCurveV2 SanitizeCurveV2(
		const FSRStellarDemandCurveV2& Curve);
	static FSRStellarPressureRulesV2 SanitizePressureRulesV2(
		const FSRStellarPressureRulesV2& Rules);
	static double CalculateDemandForCycleV2(
		const FSRStellarDemandCurveV2& Curve,
		int32 CurrentCycleIndex);
	static ESRStellarDemandPhaseV2 ResolveDemandPhaseV2(
		const FSRStellarDemandCurveV2& Curve,
		int32 CurrentCycleIndex);
	static double CalculateNextCycleMultiplierV2(
		const FSRStellarDemandCurveV2& Curve,
		int32 CurrentCycleIndex);
	static double ClampFuelReserveV2(
		double StoredFuel,
		const FSRStellarPressureRulesV2& Rules);
	static double ResolveRedGiantEmergencyReserveV2(
		const FSRStellarPressureRulesV2& Rules);
	static float CalculateFuelPressureRatioV2(
		double StoredFuel,
		const FSRStellarPressureRulesV2& Rules);

	static double CalculateLegacyNextCycleDemand(
		double PreviousDemandPerSecond,
		int32 CurrentCycleIndex);
};
