#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRStellarEvolutionTypes.h"
#include "Simulation/SRRunBalanceSimulation.h"
#include "Simulation/SRResourceReserveModel.h"
#include "Simulation/SRStellarRunContract.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRRunTelemetrySubsystem.generated.h"

class ASRStar;
class USRFacilityNetworkComponent;
struct FSRResourceInstance;

UENUM(BlueprintType)
enum class ESRRunTelemetryBottleneck : uint8
{
	None UMETA(DisplayName = "None"),
	MissingPrimaryStar UMETA(DisplayName = "Missing Primary Star"),
	StellarCollapse UMETA(DisplayName = "Stellar Collapse"),
	OperationalCapacity UMETA(DisplayName = "Operational Capacity"),
	StellarFuelProduction UMETA(DisplayName = "Stellar Fuel Production"),
	DeliveryPath UMETA(DisplayName = "Delivery Path"),
	StellarFuelDeficit UMETA(DisplayName = "Stellar Fuel Deficit"),
	FinalThroughput UMETA(DisplayName = "Final Throughput"),
	// Appended to preserve serialized values of existing telemetry diagnoses.
	ResourceDepletion UMETA(DisplayName = "Resource Depletion"),
};

/** Bounded, read-only observation of authoritative World systems. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunTelemetrySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double SimulationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 CycleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double CycleProgressSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double SecondsUntilNextCycle = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	bool bSimulationPaused = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	bool bHasPrimaryStar = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	ESRStellarEvolutionStage EvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double InitialStageFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double StoredStellarFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double StellarConsumptionPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	ESRStellarDemandPhaseV2 StellarDemandPhase = ESRStellarDemandPhaseV2::Grace;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	float StellarFuelPressureRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double RecentStellarFuelIncomePerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double StellarFuelNetPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	FSRStellarRunProgress RunProgress;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 CelestialBodyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 ConstructibleBodyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 FacilityNetworkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 RegisteredFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 EnabledFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 ProcessingFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 ThrottledFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 OperationalDemand = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 OperationalCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 ActiveServiceCoreCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 ActiveRouteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 BlockedRouteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 FleetQueuedRouteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 InFlightStellarFuelMissileCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double InFlightStellarFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double LastStellarFuelReserveOverflow = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int64 ProducedCardItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int64 ProducedStellarFuelItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double ProducedResourceEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	FSRResourceReserveSnapshot ResourceReserve;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunTelemetryMilestones
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double FirstCardProducedSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double FirstStellarFuelProducedSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double FirstStellarFuelDeliveredSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double EmergencyIgnitionCompletedSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double SustainedSupplyCompletedSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double RunCompletedSeconds = -1.0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunTelemetrySummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double RecordedDurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	ESRStellarRunOutcome Outcome = ESRStellarRunOutcome::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	FSRRunTelemetryMilestones Milestones;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double MinimumStoredStellarFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double PeakStellarConsumptionPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double PeakStellarFuelIncomePerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double AverageStellarFuelIncomePerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	float PeakOperationalUtilization = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	float PeakStellarFuelPressureRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 PeakThrottledFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int32 PeakBlockedOrQueuedRouteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	double FinalDeliveredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int64 ProducedCardItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int64 ProducedStellarFuelItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	float MinimumResourceReserveRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	int64 FinalPotentialFuelBatchCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	FName FinalLimitingReferenceCardId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	ESRRunTelemetryBottleneck PrimaryBottleneck = ESRRunTelemetryBottleneck::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Telemetry")
	FString SummaryText;
};

class STARROVERS_API FSRRunTelemetrySummaryModel final
{
public:
	static FSRRunTelemetrySummary BuildSummary(
		const TArray<FSRRunTelemetrySnapshot>& Samples);
	static FString ResolveBottleneckLabel(ESRRunTelemetryBottleneck Bottleneck);
};

/**
 * Samples real gameplay systems at a bounded interval. It never changes
 * Facility, logistics, Resource, Star, or Run state.
 */
UCLASS()
class STARROVERS_API USRRunTelemetrySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Telemetry")
	bool CaptureSnapshotNow();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Telemetry")
	void ResetTelemetry();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Telemetry")
	bool GetLatestSnapshot(FSRRunTelemetrySnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Telemetry")
	void GetTelemetrySamples(TArray<FSRRunTelemetrySnapshot>& OutSamples) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Telemetry")
	FSRRunTelemetrySummary GetSummary() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Telemetry")
	bool BuildCurrentFlatSupplyProjection(FSRRunBalanceResult& OutResult) const;

	void LogSummary() const;

private:
	void RefreshSourceBindings();
	void BindPrimaryStar(ASRStar* PrimaryStar);
	void BindFacilityNetwork(USRFacilityNetworkComponent* FacilityNetwork);
	bool BuildWorldSnapshot(FSRRunTelemetrySnapshot& OutSnapshot) const;
	void StoreSnapshot(const FSRRunTelemetrySnapshot& Snapshot);
	void HandleResourceProduced(
		USRFacilityNetworkComponent* FacilityNetwork,
		FName OccupantId,
		const FSRResourceInstance& ResourceInstance);

	UFUNCTION()
	void HandleStellarRunCompleted(ASRStar* Star);

	UPROPERTY(Transient)
	TArray<FSRRunTelemetrySnapshot> Samples;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASRStar> BoundPrimaryStar;

	TSet<TWeakObjectPtr<USRFacilityNetworkComponent>> BoundFacilityNetworks;

	UPROPERTY(Transient)
	int64 ProducedCardItemCount = 0;

	UPROPERTY(Transient)
	int64 ProducedStellarFuelItemCount = 0;

	UPROPERTY(Transient)
	double ProducedResourceEnergy = 0.0;

	double SampleAccumulatorSeconds = 0.0;
	double SampleIntervalSeconds = 5.0;
	int32 MaxSampleCount = 720;
	bool bTelemetryEnabled = true;
	bool bLogOnRunCompletion = true;
	bool bCapturedInitialStarSnapshot = false;
};
