#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"

class ASRSpaceshipActor;
class ASRStar;
class USRSpaceLogisticsSubsystem;

namespace StarRovers::SpaceLogistics::StarFuelMissiles
{
	bool HasValidMissileCargo(const FSRResourceInstance& Cargo);
	double CalculateMissileFuelValue(const FSRResourceInstance& Cargo);
	bool CanUseAsMissileFuelCargo(const FSRResourceInstance& Cargo);
}

class FSRSpaceLogisticsStarFuelMissileProcessor
{
public:
	static bool LaunchFromHub(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		FName& OutMissileId,
		float InitialSpeedUnitsPerSecond,
		float LaunchAccelerationUnitsPerSecondSquared,
		TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
		int32& NextStarFuelMissileSequence);

	static bool LaunchFromHubInputPort(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		int32 InputPortIndex,
		FName& OutMissileId,
		float InitialSpeedUnitsPerSecond,
		float LaunchAccelerationUnitsPerSecondSquared,
		TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
		int32& NextStarFuelMissileSequence);

	static void ProcessMissiles(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		float DeltaTime,
		TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& MissileActorsByMissileId);

private:
	static ASRStar* ResolvePrimaryStar(USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem);

	static bool TryTakeFuelCargoFromHub(
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		FSRResourceInstance& OutCargo);

	static bool TryTakeFuelCargoFromHubInputPort(
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		int32 InputPortIndex,
		FSRResourceInstance& OutCargo);

	static void ApplyMissileFlightSettings(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsStarFuelMissile& Missile,
		float InitialSpeedUnitsPerSecond,
		float LaunchAccelerationUnitsPerSecondSquared);

	static bool StartMissileTravel(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsStarFuelMissile& Missile);

	static void AdvanceMissile(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsStarFuelMissile& Missile,
		float DeltaTime,
		TArray<int32>& OutImpactedMissileIndices,
		int32 MissileIndex,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& MissileActorsByMissileId);

	static bool HasMissileImpactedTargetStar(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsStarFuelMissile& Missile);

	static float ResolveTargetStarImpactRadius(const AActor& TargetStarActor);
	static FName MakeMissileId(const FSRSpaceLogisticsHubEndpoint& SourceHub, int32& NextStarFuelMissileSequence);
};
