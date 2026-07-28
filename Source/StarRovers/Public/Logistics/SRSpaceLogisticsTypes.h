#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRSpaceLogisticsTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class ESRSpaceLogisticsHubRoutePhase : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	WaitingForCargo UMETA(DisplayName = "Waiting For Cargo"),
	TravelingToDestination UMETA(DisplayName = "Traveling To Destination"),
	UnloadingAtDestination UMETA(DisplayName = "Unloading At Destination"),
	TravelingToSource UMETA(DisplayName = "Traveling To Source"),
	UnloadingAtSource UMETA(DisplayName = "Unloading At Source"),
	Blocked UMETA(DisplayName = "Blocked"),
	// Appended to preserve the serialized numeric values of the legacy phases.
	WaitingForFleetCapacity UMETA(DisplayName = "Waiting For Fleet Capacity"),
	// Appended in save schema 5. Never reorder serialized route phases.
	ConditioningAtDestination UMETA(DisplayName = "Conditioning At Destination"),
	ConditioningAtSource UMETA(DisplayName = "Conditioning At Source"),
};

UENUM(BlueprintType)
enum class ESRConditionedTransitModuleV2 : uint8
{
	None UMETA(DisplayName = "No Conditioned Module"),
	CryogenicHold UMETA(DisplayName = "Cryogenic Hold"),
	BioCultureHold UMETA(DisplayName = "Bio-Culture Hold"),
	GroundingHold UMETA(DisplayName = "Grounding Hold"),
};

UENUM(BlueprintType)
enum class ESRSpaceLogisticsRouteProfileV2 : uint8
{
	NeutralShuttle UMETA(DisplayName = "Neutral Shuttle"),
	CardCourier UMETA(DisplayName = "Card Courier"),
	BulkRawHold UMETA(DisplayName = "Bulk Raw Hold"),
	ConditionedHold UMETA(DisplayName = "Conditioned Hold"),
};

UENUM(BlueprintType)
enum class ESRSpaceLogisticsHubRouteDockSide : uint8
{
	Source UMETA(DisplayName = "Source"),
	Destination UMETA(DisplayName = "Destination"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSpaceLogisticsHubEndpoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "BodyActor"))
	TObjectPtr<AActor> BodyActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "HubOccupantId"))
	FName HubOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "OriginCellId"))
	FSRPlanetSurfaceGridCellId OriginCellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "FootprintCellIds"))
	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;

	bool IsValid() const
	{
		return ::IsValid(BodyActor) && !HubOccupantId.IsNone();
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSpaceLogisticsHubEndpointSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "BodyActorName"))
	FName BodyActorName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "BodyVariableName"))
	FString BodyVariableName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "HubOccupantId"))
	FName HubOccupantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	bool IsValid() const
	{
		return (!BodyActorName.IsNone() || !BodyVariableName.IsEmpty()) && !HubOccupantId.IsNone();
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSpaceLogisticsRouteProfileRulesV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	FName ProfileId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	ESRSpaceLogisticsRouteProfileV2 Profile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	FText CargoContractText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 CargoCapacity = 8;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 FleetLoad = 2;

	// The profile reserves the smaller conditioned hull. A concrete Hold module
	// selected by a later Augment phase owns the actual in-transit Family Action.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	bool bSupportsConditionedTransit = false;

	// Neutral Shuttle and Card Courier are guaranteed Technology hulls. Bulk Raw
	// and Conditioned hulls are explicit Augment grants so a strategic Package
	// changes the Route configuration that the player can actually select.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	bool bRequiresAugmentUnlock = false;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFleetCapacityReportV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	bool bRulesActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 BaseCapacity = 8;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 ActiveFleetBerthCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 FleetBerthCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 TotalCapacity = 8;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 ReservedLoad = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 AvailableCapacity = 8;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity")
	int32 QueuedDepartureCount = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSpaceLogisticsHubRoute
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "RouteId"))
	FName RouteId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "SourceHub"))
	FSRSpaceLogisticsHubEndpoint SourceHub;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "DestinationHub"))
	FSRSpaceLogisticsHubEndpoint DestinationHub;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "bEnabled"))
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "bReturnEmptyWhenNoCargo"))
	bool bReturnEmptyWhenNoCargo = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "MaxCargoStackCount"))
	int32 MaxCargoStackCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "CargoResourceId"))
	FName CargoResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity", meta = (DisplayName = "RouteProfile"))
	ESRSpaceLogisticsRouteProfileV2 RouteProfile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;

	// None keeps even a Conditioned Hold state-neutral. A concrete module is only
	// assignable while its Augment Package is unlocked.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit", meta = (DisplayName = "ConditionedTransitModule"))
	ESRConditionedTransitModuleV2 ConditionedTransitModule = ESRConditionedTransitModuleV2::None;

	// A stable ticket prevents a route that has just returned from jumping ahead
	// of older capacity waiters. Zero means that the route is not queued.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Fleet Capacity", meta = (DisplayName = "FleetDepartureQueueSequence"))
	int64 FleetDepartureQueueSequence = 0;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "StarRovers|Space Logistics|Fleet Capacity", meta = (DisplayName = "FleetQueuePosition"))
	int32 FleetQueuePosition = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "bDebugLocalOrbit"))
	bool bDebugLocalOrbit = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "Phase"))
	ESRSpaceLogisticsHubRoutePhase Phase = ESRSpaceLogisticsHubRoutePhase::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "CurrentDockSide"))
	ESRSpaceLogisticsHubRouteDockSide CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TravelDurationSeconds"))
	float TravelDurationSeconds = 8.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "InitialSpeedUnitsPerSecond"))
	float InitialSpeedUnitsPerSecond = 6000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "LaunchAccelerationUnitsPerSecondSquared"))
	float LaunchAccelerationUnitsPerSecondSquared = 3000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TravelProgressSeconds"))
	float TravelProgressSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TravelProgressRatio"))
	float TravelProgressRatio = 0.0f;

	// Captured once when a Conditioned Hold reaches its dock. Runtime balance
	// changes cannot move an in-flight conditioning cycle's finish line.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit", meta = (DisplayName = "ConditioningDurationSeconds"))
	float ConditioningDurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit", meta = (DisplayName = "ConditioningProgressSeconds"))
	float ConditioningProgressSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TravelStartWorldLocation"))
	FVector TravelStartWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "bHasTravelStartWorldLocation"))
	bool bHasTravelStartWorldLocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "LaunchWorldVelocity"))
	FVector LaunchWorldVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "bHasLaunchWorldVelocity"))
	bool bHasLaunchWorldVelocity = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "Cargo"))
	FSRResourceInstance Cargo;

	bool IsValid() const
	{
		return !RouteId.IsNone() && SourceHub.IsValid() && DestinationHub.IsValid();
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSpaceLogisticsStarFuelMissile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "MissileId"))
	FName MissileId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "SourceHub"))
	FSRSpaceLogisticsHubEndpoint SourceHub;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TargetStarActor"))
	TObjectPtr<AActor> TargetStarActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "bEnabled"))
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TravelDurationSeconds"))
	float TravelDurationSeconds = 8.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "InitialSpeedUnitsPerSecond"))
	float InitialSpeedUnitsPerSecond = 6000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "LaunchAccelerationUnitsPerSecondSquared"))
	float LaunchAccelerationUnitsPerSecondSquared = 3000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TravelProgressSeconds"))
	float TravelProgressSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TravelProgressRatio"))
	float TravelProgressRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "TravelStartWorldLocation"))
	FVector TravelStartWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "bHasTravelStartWorldLocation"))
	bool bHasTravelStartWorldLocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "LaunchWorldVelocity"))
	FVector LaunchWorldVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "bHasLaunchWorldVelocity"))
	bool bHasLaunchWorldVelocity = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics", meta = (DisplayName = "Cargo"))
	FSRResourceInstance Cargo;

	bool IsValid() const
	{
		return !MissileId.IsNone()
			&& SourceHub.IsValid()
			&& ::IsValid(TargetStarActor)
			&& !Cargo.ResourceId.IsNone()
			&& Cargo.StackCount > 0;
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSpaceLogisticsHubRouteSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "RouteId"))
	FName RouteId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "SourceHub"))
	FSRSpaceLogisticsHubEndpointSaveData SourceHub;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "DestinationHub"))
	FSRSpaceLogisticsHubEndpointSaveData DestinationHub;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "bEnabled"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "bReturnEmptyWhenNoCargo"))
	bool bReturnEmptyWhenNoCargo = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "MaxCargoStackCount"))
	int32 MaxCargoStackCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "CargoResourceId"))
	FName CargoResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "RouteProfile"))
	ESRSpaceLogisticsRouteProfileV2 RouteProfile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "ConditionedTransitModule"))
	ESRConditionedTransitModuleV2 ConditionedTransitModule = ESRConditionedTransitModuleV2::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "FleetDepartureQueueSequence"))
	int64 FleetDepartureQueueSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "bDebugLocalOrbit"))
	bool bDebugLocalOrbit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "Phase"))
	ESRSpaceLogisticsHubRoutePhase Phase = ESRSpaceLogisticsHubRoutePhase::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "CurrentDockSide"))
	ESRSpaceLogisticsHubRouteDockSide CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TravelDurationSeconds"))
	float TravelDurationSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "InitialSpeedUnitsPerSecond"))
	float InitialSpeedUnitsPerSecond = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "LaunchAccelerationUnitsPerSecondSquared"))
	float LaunchAccelerationUnitsPerSecondSquared = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TravelProgressSeconds"))
	float TravelProgressSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TravelProgressRatio"))
	float TravelProgressRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "ConditioningDurationSeconds"))
	float ConditioningDurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "ConditioningProgressSeconds"))
	float ConditioningProgressSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TravelStartWorldLocation"))
	FVector TravelStartWorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "bHasTravelStartWorldLocation"))
	bool bHasTravelStartWorldLocation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "LaunchWorldVelocity"))
	FVector LaunchWorldVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "bHasLaunchWorldVelocity"))
	bool bHasLaunchWorldVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "Cargo"))
	FSRResourceInstance Cargo;

	bool IsValid() const
	{
		return !RouteId.IsNone() && SourceHub.IsValid() && DestinationHub.IsValid();
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSpaceLogisticsStarFuelMissileSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "MissileId"))
	FName MissileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "SourceHub"))
	FSRSpaceLogisticsHubEndpointSaveData SourceHub;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TargetStarActorName"))
	FName TargetStarActorName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TargetStarVariableName"))
	FString TargetStarVariableName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "bEnabled"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TravelDurationSeconds"))
	float TravelDurationSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "InitialSpeedUnitsPerSecond"))
	float InitialSpeedUnitsPerSecond = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "LaunchAccelerationUnitsPerSecondSquared"))
	float LaunchAccelerationUnitsPerSecondSquared = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TravelProgressSeconds"))
	float TravelProgressSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TravelProgressRatio"))
	float TravelProgressRatio = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "TravelStartWorldLocation"))
	FVector TravelStartWorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "bHasTravelStartWorldLocation"))
	bool bHasTravelStartWorldLocation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "LaunchWorldVelocity"))
	FVector LaunchWorldVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "bHasLaunchWorldVelocity"))
	bool bHasLaunchWorldVelocity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "Cargo"))
	FSRResourceInstance Cargo;

	bool IsValid() const
	{
		return !MissileId.IsNone()
			&& SourceHub.IsValid()
			&& (!TargetStarActorName.IsNone() || !TargetStarVariableName.IsEmpty())
			&& !Cargo.ResourceId.IsNone()
			&& Cargo.StackCount > 0;
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRSpaceLogisticsSaveData
{
	GENERATED_BODY()

	static constexpr int32 InitialVersion = 1;
	static constexpr int32 FleetCapacityVersion = 3;
	static constexpr int32 ConditionedTransitVersion = 4;
	static constexpr int32 ConditioningDwellVersion = 5;
	static constexpr int32 CurrentVersion = ConditioningDwellVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "Version"))
	int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "NextHubRouteSequence"))
	int32 NextHubRouteSequence = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "NextFleetDepartureQueueSequence"))
	int64 NextFleetDepartureQueueSequence = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "NextStarFuelMissileSequence"))
	int32 NextStarFuelMissileSequence = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "HubRoutes"))
	TArray<FSRSpaceLogisticsHubRouteSaveData> HubRoutes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "StarFuelMissiles"))
	TArray<FSRSpaceLogisticsStarFuelMissileSaveData> StarFuelMissiles;

	bool IsSupportedVersion() const
	{
		return Version >= InitialVersion && Version <= CurrentVersion;
	}
};
