#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SRSpaceLogisticsSubsystem.generated.h"

class ASRSpaceshipActor;
class FSRSpaceLogisticsRoutePathResolver;
class FSRSpaceLogisticsRouteProcessor;
class FSRSpaceLogisticsRouteRegistry;
class FSRSpaceLogisticsSaveAdapter;
class FSRSpaceLogisticsStarFuelMissileProcessor;

struct FSRSpaceLogisticsHubEndpointMotionSample
{
	FVector LastWorldLocation = FVector::ZeroVector;
	FVector WorldVelocity = FVector::ZeroVector;
	bool bHasWorldLocation = false;
};

UCLASS()
class STARROVERS_API USRSpaceLogisticsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Hub")
	void RefreshHubEndpoints();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Hub")
	void GetHubEndpoints(TArray<FSRSpaceLogisticsHubEndpoint>& OutHubEndpoints) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Hub")
	bool GetHubEndpoint(AActor* BodyActor, FName HubOccupantId, FSRSpaceLogisticsHubEndpoint& OutHubEndpoint) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Hub")
	bool ResolveHubEndpointWorldLocation(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	bool CreateHubRoute(
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub,
		FName& OutRouteId,
		bool bReturnEmptyWhenNoCargo = true,
		int32 MaxCargoStackCount = 1,
		float InitialSpeedUnitsPerSecond = -1.0f,
		float LaunchAccelerationUnitsPerSecondSquared = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	bool CreateDebugLocalOrbitRoute(
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		FName& OutRouteId,
		float InitialSpeedUnitsPerSecond = -1.0f,
		float LaunchAccelerationUnitsPerSecondSquared = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	bool RemoveHubRoute(FName RouteId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	bool SetHubRouteMaxCargoStackCount(FName RouteId, int32 MaxCargoStackCount);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	bool SetHubRouteReturnEmptyWhenNoCargo(FName RouteId, bool bReturnEmptyWhenNoCargo);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	bool SetHubRouteCargoResourceId(FName RouteId, FName CargoResourceId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Missile")
	bool LaunchStarFuelMissileFromHub(
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		FName& OutMissileId,
		float InitialSpeedUnitsPerSecond = -1.0f,
		float LaunchAccelerationUnitsPerSecondSquared = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Missile")
	bool LaunchStarFuelMissileFromHubInputPort(
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		int32 InputPortIndex,
		FName& OutMissileId,
		float InitialSpeedUnitsPerSecond = -1.0f,
		float LaunchAccelerationUnitsPerSecondSquared = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	void ClearHubRoutes();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	void GetHubRoutes(TArray<FSRSpaceLogisticsHubRoute>& OutRoutes) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Route")
	bool GetHubRoute(FName RouteId, FSRSpaceLogisticsHubRoute& OutRoute) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Missile")
	void GetStarFuelMissiles(TArray<FSRSpaceLogisticsStarFuelMissile>& OutMissiles) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Save")
	void ExportSaveData(FSRSpaceLogisticsSaveData& OutSaveData) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Space Logistics|Save")
	bool ImportSaveData(const FSRSpaceLogisticsSaveData& SaveData);

private:
	friend class FSRSpaceLogisticsRoutePathResolver;
	friend class FSRSpaceLogisticsRouteProcessor;
	friend class FSRSpaceLogisticsRouteRegistry;
	friend class FSRSpaceLogisticsSaveAdapter;
	friend class FSRSpaceLogisticsStarFuelMissileProcessor;

	void RebuildHubEndpoints() const;
	bool BuildHubEndpoint(AActor* BodyActor, FName HubOccupantId, FSRSpaceLogisticsHubEndpoint& OutHubEndpoint) const;
	bool BuildHubEndpointSaveData(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, FSRSpaceLogisticsHubEndpointSaveData& OutSaveData) const;
	bool ResolveSavedHubEndpoint(const FSRSpaceLogisticsHubEndpointSaveData& SaveData, FSRSpaceLogisticsHubEndpoint& OutHubEndpoint) const;
	bool ResolveCurrentHubEndpoint(const FSRSpaceLogisticsHubEndpoint& CandidateHubEndpoint, FSRSpaceLogisticsHubEndpoint& OutHubEndpoint) const;
	bool ResolveHubEndpointWorldLocationWithHeightOffset(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, float HeightOffset, FVector& OutWorldLocation) const;
	bool ResolveHubEndpointSurfaceWorldLocation(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, FVector& OutWorldLocation) const;
	float ResolveSimulationDeltaSeconds(float DeltaTime) const;
	bool ResolveHubEndpointWorldVelocity(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, FVector& OutWorldVelocity) const;

	UPROPERTY(Transient)
	mutable TArray<FSRSpaceLogisticsHubEndpoint> CachedHubEndpoints;

	UPROPERTY(Transient)
	TArray<FSRSpaceLogisticsHubRoute> HubRoutes;

	UPROPERTY(Transient)
	TArray<FSRSpaceLogisticsStarFuelMissile> StarFuelMissiles;

	UPROPERTY(Transient)
	int32 NextHubRouteSequence = 1;

	UPROPERTY(Transient)
	int32 NextStarFuelMissileSequence = 1;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ASRSpaceshipActor>> SpaceshipActorsByRouteId;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ASRSpaceshipActor>> StarFuelMissileActorsByMissileId;

	TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample> HubEndpointMotionSamples;
};
