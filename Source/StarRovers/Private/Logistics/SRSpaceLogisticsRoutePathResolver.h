#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"

class USRSpaceLogisticsSubsystem;

class FSRSpaceLogisticsRoutePathResolver
{
public:
	static float GetDefaultInitialSpeedUnitsPerSecond();
	static float GetDefaultLaunchAccelerationUnitsPerSecondSquared();
	static float GetMinimumTravelDurationSeconds();
	static float GetLaunchAscentEndRatio();
	static float ClampInitialSpeed(float InitialSpeedUnitsPerSecond);
	static float ClampLaunchAcceleration(float LaunchAccelerationUnitsPerSecondSquared);
	static float ResolveAcceleratedDuration(float SegmentDistance, float TargetSpeed, float Acceleration);

	static float ResolveMotionProgressRatio(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		float TravelProgressSeconds);

	static bool ResolveTargetWorldLocation(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		FVector& OutTargetLocation);

	static bool ResolveVisualWorldLocation(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		FVector& OutLocation,
		FVector& OutTargetLocation,
		FVector& OutTravelDirection);

	static float EstimateRoutePathLength(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		int32 SampleCount = 48);

	static float EstimateRoutePathLengthRange(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		float StartAlpha,
		float EndAlpha,
		int32 SampleCount = 16);

	static float ResolveTravelDurationSeconds(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRoute& HubRoute);
};
