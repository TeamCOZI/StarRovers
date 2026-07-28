#include "Simulation/SRStellarDemandModel.h"

namespace
{
	constexpr double MaximumStellarBalanceValue = 1.0e15;

	double SanitizeDemandNonNegative(double Value)
	{
		return FMath::IsFinite(Value)
			? FMath::Clamp(Value, 0.0, MaximumStellarBalanceValue)
			: 0.0;
	}
}

FSRStellarDemandCurveV2 FSRStellarDemandModel::SanitizeCurveV2(
	const FSRStellarDemandCurveV2& Curve)
{
	FSRStellarDemandCurveV2 Result = Curve;
	Result.InitialDemandPerSecond = SanitizeDemandNonNegative(Curve.InitialDemandPerSecond);
	Result.GraceCycleCount = FMath::Max(0, Curve.GraceCycleCount);
	Result.DemandIncreasePerCycle = SanitizeDemandNonNegative(Curve.DemandIncreasePerCycle);
	Result.MaximumDemandPerSecond = FMath::Max(
		Result.InitialDemandPerSecond,
		SanitizeDemandNonNegative(Curve.MaximumDemandPerSecond));
	return Result;
}

FSRStellarPressureRulesV2 FSRStellarDemandModel::SanitizePressureRulesV2(
	const FSRStellarPressureRulesV2& Rules)
{
	FSRStellarPressureRulesV2 Result = Rules;
	Result.FuelReserveCapacity = FMath::Clamp(
		SanitizeDemandNonNegative(Rules.FuelReserveCapacity),
		1.0,
		MaximumStellarBalanceValue);
	Result.RedGiantEmergencyReserveRatio = FMath::Clamp(
		FMath::IsFinite(Rules.RedGiantEmergencyReserveRatio)
			? Rules.RedGiantEmergencyReserveRatio
			: 0.0,
		0.0,
		1.0);
	return Result;
}

double FSRStellarDemandModel::CalculateDemandForCycleV2(
	const FSRStellarDemandCurveV2& Curve,
	int32 CurrentCycleIndex)
{
	const FSRStellarDemandCurveV2 SafeCurve = SanitizeCurveV2(Curve);
	const int32 RampCycleCount = FMath::Max(
		0,
		FMath::Max(0, CurrentCycleIndex) - SafeCurve.GraceCycleCount);
	const double Demand = SafeCurve.InitialDemandPerSecond
		+ static_cast<double>(RampCycleCount) * SafeCurve.DemandIncreasePerCycle;
	return FMath::Clamp(
		FMath::IsFinite(Demand) ? Demand : SafeCurve.MaximumDemandPerSecond,
		SafeCurve.InitialDemandPerSecond,
		SafeCurve.MaximumDemandPerSecond);
}

ESRStellarDemandPhaseV2 FSRStellarDemandModel::ResolveDemandPhaseV2(
	const FSRStellarDemandCurveV2& Curve,
	int32 CurrentCycleIndex)
{
	const FSRStellarDemandCurveV2 SafeCurve = SanitizeCurveV2(Curve);
	const int32 SafeCycleIndex = FMath::Max(0, CurrentCycleIndex);
	if (SafeCycleIndex <= SafeCurve.GraceCycleCount)
	{
		return ESRStellarDemandPhaseV2::Grace;
	}
	if (CalculateDemandForCycleV2(SafeCurve, SafeCycleIndex)
		+ UE_DOUBLE_SMALL_NUMBER < SafeCurve.MaximumDemandPerSecond)
	{
		return ESRStellarDemandPhaseV2::Expansion;
	}
	return ESRStellarDemandPhaseV2::Plateau;
}

double FSRStellarDemandModel::CalculateNextCycleMultiplierV2(
	const FSRStellarDemandCurveV2& Curve,
	int32 CurrentCycleIndex)
{
	const double CurrentDemand = CalculateDemandForCycleV2(Curve, CurrentCycleIndex);
	const int32 NextCycleIndex = CurrentCycleIndex < TNumericLimits<int32>::Max()
		? CurrentCycleIndex + 1
		: CurrentCycleIndex;
	const double NextDemand = CalculateDemandForCycleV2(Curve, NextCycleIndex);
	return CurrentDemand > UE_DOUBLE_SMALL_NUMBER
		? FMath::Max(0.0, NextDemand / CurrentDemand)
		: 1.0;
}

double FSRStellarDemandModel::ClampFuelReserveV2(
	double StoredFuel,
	const FSRStellarPressureRulesV2& Rules)
{
	const FSRStellarPressureRulesV2 SafeRules = SanitizePressureRulesV2(Rules);
	return FMath::Clamp(
		SanitizeDemandNonNegative(StoredFuel),
		0.0,
		SafeRules.FuelReserveCapacity);
}

double FSRStellarDemandModel::ResolveRedGiantEmergencyReserveV2(
	const FSRStellarPressureRulesV2& Rules)
{
	const FSRStellarPressureRulesV2 SafeRules = SanitizePressureRulesV2(Rules);
	return SafeRules.FuelReserveCapacity * SafeRules.RedGiantEmergencyReserveRatio;
}

float FSRStellarDemandModel::CalculateFuelPressureRatioV2(
	double StoredFuel,
	const FSRStellarPressureRulesV2& Rules)
{
	const FSRStellarPressureRulesV2 SafeRules = SanitizePressureRulesV2(Rules);
	const double SafeStoredFuel = ClampFuelReserveV2(StoredFuel, SafeRules);
	return FMath::Clamp(
		static_cast<float>(1.0 - SafeStoredFuel / SafeRules.FuelReserveCapacity),
		0.0f,
		1.0f);
}

double FSRStellarDemandModel::CalculateLegacyNextCycleDemand(
	double PreviousDemandPerSecond,
	int32 CurrentCycleIndex)
{
	const double SafePreviousDemand = FMath::IsFinite(PreviousDemandPerSecond)
		? FMath::Max(0.0, PreviousDemandPerSecond)
		: 0.0;
	const double GrowthPercent = 200.0 + static_cast<double>(FMath::Max(0, CurrentCycleIndex));
	const double NextDemand = SafePreviousDemand * (GrowthPercent / 100.0);
	return FMath::IsFinite(NextDemand)
		? FMath::Max(0.0, NextDemand)
		: TNumericLimits<double>::Max();
}
