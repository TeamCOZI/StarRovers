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
struct STARROVERS_API FSRSpaceLogisticsSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "Version"))
	int32 Version = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "NextHubRouteSequence"))
	int32 NextHubRouteSequence = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Space Logistics|Save", meta = (DisplayName = "HubRoutes"))
	TArray<FSRSpaceLogisticsHubRouteSaveData> HubRoutes;
};
