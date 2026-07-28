#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Simulation/SRStellarDemandModel.h"
#include "Simulation/SRStellarRunContract.h"
#include "SRSimulationSettings.generated.h"

UENUM(BlueprintType)
enum class ESRResourceRulesetVersion : uint8
{
	Legacy UMETA(DisplayName = "Legacy"),
	ResourceV2 UMETA(DisplayName = "Resource V2"),
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Star Rovers Simulation"))
class STARROVERS_API USRSimulationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Resource System|Migration", meta = (
		DisplayName = "Resource Ruleset Version",
		ToolTip = "Selects the active automation resource rules. Resource V2 is the shipped project default; Legacy remains available for save migration and regression testing."))
	ESRResourceRulesetVersion ResourceRulesetVersion = ESRResourceRulesetVersion::ResourceV2;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Time Control", meta = (DisplayName = "Seconds Per Period", ClampMin = "0.01", UIMin = "0.01"))
	float SecondsPerPeriod = 20.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Time Control", meta = (
		DisplayName = "Pause Simulation On Run Start",
		ToolTip = "Starts a newly loaded run in planning mode so the player can inspect the system before stellar fuel begins draining."))
	bool bPauseSimulationOnRunStart = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Pressure", meta = (
		DisplayName = "Use Stellar Pressure Curve V2",
		ToolTip = "Uses a capped linear demand ramp and a bounded survival reserve instead of the legacy exponential curve."))
	bool bUseStellarPressureCurveV2 = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Pressure", meta = (
		DisplayName = "Stellar Fuel Reserve Capacity",
		ClampMin = "1.0",
		UIMin = "1.0",
		ToolTip = "Survival buffer capacity for each stellar evolution stage. Fuel beyond this reserve still advances the stabilization objective."))
	double StellarFuelReserveCapacityV2 = 20000.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Pressure", meta = (
		DisplayName = "Initial Stellar Demand Per Second",
		ClampMin = "0.0",
		UIMin = "0.0"))
	double StellarInitialDemandPerSecondV2 = 50.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Pressure", meta = (
		DisplayName = "Demand Grace Cycle Count",
		ClampMin = "0",
		UIMin = "0",
		ToolTip = "Completed cycles that keep the initial demand before the linear ramp begins."))
	int32 StellarDemandGraceCycleCountV2 = 2;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Pressure", meta = (
		DisplayName = "Demand Increase Per Cycle",
		ClampMin = "0.0",
		UIMin = "0.0"))
	double StellarDemandIncreasePerCycleV2 = 5.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Pressure", meta = (
		DisplayName = "Maximum Stellar Demand Per Second",
		ClampMin = "0.0",
		UIMin = "0.0"))
	double StellarMaximumDemandPerSecondV2 = 100.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Pressure", meta = (
		DisplayName = "Red Giant Emergency Reserve Ratio",
		ClampMin = "0.0",
		ClampMax = "1.0",
		UIMin = "0.0",
		UIMax = "1.0",
		ToolTip = "One-time fraction of the reserve restored when Main Sequence enters Red Giant. The evolution stage never cycles backward."))
	double StellarRedGiantEmergencyReserveRatioV2 = 1.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Stabilization", meta = (
		DisplayName = "Enable Finite Stellar Victory",
		ToolTip = "Allows cumulative delivered fuel plus a sustained recent-income window to complete the Run."))
	bool bFiniteStellarVictoryEnabledV2 = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Stabilization", meta = (
		DisplayName = "Emergency Delivery Target",
		ClampMin = "0.0",
		UIMin = "0.0"))
	double StellarEmergencyDeliveryTargetV2 = 5000.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Stabilization", meta = (
		DisplayName = "Sustained Supply Delivery Target",
		ClampMin = "0.0",
		UIMin = "0.0"))
	double StellarSustainedSupplyDeliveryTargetV2 = 25000.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Stabilization", meta = (
		DisplayName = "Victory Delivery Target",
		ClampMin = "0.0",
		UIMin = "0.0"))
	double StellarVictoryDeliveryTargetV2 = 100000.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Stabilization", meta = (
		DisplayName = "Victory Required Income Per Second",
		ClampMin = "0.0",
		UIMin = "0.0"))
	double StellarVictoryRequiredIncomePerSecondV2 = 100.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Stabilization", meta = (
		DisplayName = "Victory Required Sustain Seconds",
		ClampMin = "0.0",
		UIMin = "0.0"))
	double StellarVictoryRequiredSustainSecondsV2 = 30.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Stellar Stabilization", meta = (
		DisplayName = "Target Run Duration Seconds",
		ClampMin = "0.0",
		UIMin = "0.0",
		ToolTip = "Balancing and telemetry target. It does not create an artificial minimum victory time."))
	double TargetRunDurationSecondsV2 = 1800.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Telemetry", meta = (
		DisplayName = "Enable Run Telemetry",
		ToolTip = "Keeps a bounded in-memory record of authoritative Run, Facility, Capacity, and logistics metrics. It never writes player data to disk."))
	bool bEnableRunTelemetryV2 = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Telemetry", meta = (
		DisplayName = "Run Telemetry Sample Interval Seconds",
		ClampMin = "1.0",
		UIMin = "1.0"))
	double RunTelemetrySampleIntervalSecondsV2 = 5.0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Telemetry", meta = (
		DisplayName = "Run Telemetry Max Samples",
		ClampMin = "16",
		UIMin = "16"))
	int32 RunTelemetryMaxSamplesV2 = 720;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Run Contract|Telemetry", meta = (
		DisplayName = "Log Run Telemetry On Completion"))
	bool bLogRunTelemetryOnCompletionV2 = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Resource System|Operational Capacity", meta = (
		DisplayName = "Base Operational Capacity",
		ClampMin = "1",
		UIMin = "1"))
	int32 BaseOperationalCapacityV2 = 30;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Resource System|Initial Progress", meta = (
		DisplayName = "Enable Emergency Prospecting Recovery",
		ToolTip = "Offers one bounded emergency deposit only when the initial System Scan has no mineable Card candidate. The option expires after the first Card is produced."))
	bool bEnableEmergencyProspectingRecoveryV2 = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Resource System|Initial Progress", meta = (
		DisplayName = "Emergency Prospecting Card Amount",
		ClampMin = "5",
		UIMin = "5",
		ToolTip = "Finite output assigned to the one-time emergency deposit. It is a bootstrap safety net, not a renewable production source."))
	int32 EmergencyProspectingCardAmountV2 = 25;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Resource System|Operational Capacity", meta = (
		DisplayName = "Service Core Capacity",
		ClampMin = "0",
		UIMin = "0"))
	int32 ServiceCoreOperationalCapacityV2 = 18;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Resource System|Refinement Resistance", meta = (
		DisplayName = "Refinement Resistance Energy Scale",
		ClampMin = "0.01",
		UIMin = "1.0",
		ToolTip = "For Energy-changing Resource V2 Family processes, cycle multiplier is 1 + max(0, Current Energy - Seed Energy) / this value."))
	float RefinementResistanceEnergyScaleV2 = 40.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Resource System|Fleet Capacity", meta = (
		DisplayName = "Base Fleet Capacity Per Hub",
		ClampMin = "1",
		UIMin = "1"))
	int32 BaseFleetCapacityV2 = 8;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Resource System|Fleet Capacity", meta = (
		DisplayName = "Fleet Berth Capacity",
		ClampMin = "0",
		UIMin = "0"))
	int32 FleetBerthCapacityV2 = 8;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment", meta = (DisplayName = "Augment Interval Cycles", ClampMin = "1", UIMin = "1"))
	int32 AugmentIntervalCycles = 3;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment", meta = (DisplayName = "Choices Per Offer", ClampMin = "1", UIMin = "1"))
	int32 AugmentChoicesPerOffer = 3;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "Basic Chance Percent", ClampMin = "0.0", UIMin = "0.0"))
	float AugmentBasicChancePercent = 60.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "Advanced Chance Percent", ClampMin = "0.0", UIMin = "0.0"))
	float AugmentAdvancedChancePercent = 35.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "High Tech Base Chance Percent", ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float AugmentHighTechBaseChancePercent = 5.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "High Tech Pity Increase Percent", ClampMin = "0.0", UIMin = "0.0"))
	float AugmentHighTechPityIncreasePercent = 0.5f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "High Tech Pity Cap Percent", ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float AugmentHighTechPityCapPercent = 30.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment", meta = (DisplayName = "Pause Simulation During Choice"))
	bool bPauseSimulationDuringAugmentChoice = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment", meta = (DisplayName = "Augment Random Seed"))
	int32 AugmentRandomSeed = 47219;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Debug", meta = (DisplayName = "Unlock All Facilities Without Augments", ToolTip = "Treats every buildable facility structure as unlocked without selecting augments. Intended for editor/debug playtests."))
	bool bDebugUnlockAllFacilitiesWithoutAugments = false;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Debug", meta = (
		DisplayName = "Unlock All Resource V2 Augment Packages",
		ToolTip = "Treats every Resource V2 Tag, Fuel Imprint, and conditional Facility grant as unlocked. Intended for editor/debug playtests."))
	bool bDebugUnlockAllAugmentPackagesV2 = false;

	FSRStellarRunContract BuildStellarRunContractV2() const
	{
		FSRStellarRunContract Contract;
		Contract.bFiniteVictoryEnabled = bFiniteStellarVictoryEnabledV2;
		Contract.EmergencyDeliveryTarget = StellarEmergencyDeliveryTargetV2;
		Contract.SustainedSupplyDeliveryTarget = StellarSustainedSupplyDeliveryTargetV2;
		Contract.VictoryDeliveryTarget = StellarVictoryDeliveryTargetV2;
		Contract.VictoryRequiredIncomePerSecond = StellarVictoryRequiredIncomePerSecondV2;
		Contract.VictoryRequiredSustainSeconds = StellarVictoryRequiredSustainSecondsV2;
		Contract.TargetRunDurationSeconds = TargetRunDurationSecondsV2;
		return FSRStellarRunContractModel::Sanitize(Contract);
	}

	FSRStellarDemandCurveV2 BuildStellarDemandCurveV2() const
	{
		FSRStellarDemandCurveV2 Curve;
		Curve.InitialDemandPerSecond = StellarInitialDemandPerSecondV2;
		Curve.GraceCycleCount = StellarDemandGraceCycleCountV2;
		Curve.DemandIncreasePerCycle = StellarDemandIncreasePerCycleV2;
		Curve.MaximumDemandPerSecond = StellarMaximumDemandPerSecondV2;
		return FSRStellarDemandModel::SanitizeCurveV2(Curve);
	}

	FSRStellarPressureRulesV2 BuildStellarPressureRulesV2() const
	{
		FSRStellarPressureRulesV2 Rules;
		Rules.FuelReserveCapacity = StellarFuelReserveCapacityV2;
		Rules.RedGiantEmergencyReserveRatio = StellarRedGiantEmergencyReserveRatioV2;
		return FSRStellarDemandModel::SanitizePressureRulesV2(Rules);
	}
};
