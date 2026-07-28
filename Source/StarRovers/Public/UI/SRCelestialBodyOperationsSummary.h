#pragma once

#include "CoreMinimal.h"
#include "Automation/SROperationalCapacityTypes.h"
#include "Simulation/SRResourceReserveModel.h"
#include "SRCelestialBodyOperationsSummary.generated.h"

class AActor;

UENUM(BlueprintType)
enum class ESRCelestialBodyOperationsPressure : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Nominal UMETA(DisplayName = "Nominal"),
	NearCapacity UMETA(DisplayName = "Near Capacity"),
	AtCapacity UMETA(DisplayName = "At Capacity"),
	OverCapacity UMETA(DisplayName = "Over Capacity"),
};

/**
 * Read-only UI snapshot for one planet or moon. Gameplay systems remain the
 * source of truth; this type only aggregates their current reports.
 */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyOperationsSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations")
	FSROperationalCapacityReportV2 OperationalCapacity;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations")
	int32 FacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations")
	int32 EnabledFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations")
	int32 ProcessingFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations")
	int32 ThrottledFacilityCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Resources")
	FSRResourceReserveSnapshot ResourceReserve;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	bool bFleetCapacityRulesActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 HubCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 ConnectedRouteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 BlockedRouteCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 FleetReservedLoad = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 FleetTotalCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 FleetAvailableCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 FleetQueuedDepartureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 ActiveFleetBerthCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 BusiestHubReservedLoad = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 BusiestHubTotalCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Body Operations|Logistics")
	int32 ActiveStarFuelMissileCount = 0;
};

/** Builds and formats a body-level view without owning or changing simulation state. */
class STARROVERS_API FSRCelestialBodyOperationsSummaryBuilder
{
public:
	/** Capacity and facility counters only. Safe for frequent overview-list refreshes. */
	static bool BuildOperationalSummary(
		const AActor* CelestialBodyActor,
		FSRCelestialBodyOperationsSummary& OutSummary);

	/** Full snapshot including the selected body's Hub, route, and fleet reports. */
	static bool BuildSummary(
		const AActor* CelestialBodyActor,
		FSRCelestialBodyOperationsSummary& OutSummary);

	static float GetOperationalUtilization(const FSRCelestialBodyOperationsSummary& Summary);
	static ESRCelestialBodyOperationsPressure ResolveOperationalPressure(
		const FSRCelestialBodyOperationsSummary& Summary);
	static FString BuildOperationalBadgeText(const FSRCelestialBodyOperationsSummary& Summary);
	static FString BuildOperationalStatusText(const FSRCelestialBodyOperationsSummary& Summary);
	static FString BuildOperationalToolTipText(const FSRCelestialBodyOperationsSummary& Summary);
	static FString BuildResourceReserveText(const FSRCelestialBodyOperationsSummary& Summary);
	static FString BuildResourceReserveStatusText(const FSRCelestialBodyOperationsSummary& Summary);
};
