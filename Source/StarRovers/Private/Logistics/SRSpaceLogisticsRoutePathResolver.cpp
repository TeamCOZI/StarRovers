#include "SRSpaceLogisticsRoutePathResolver.h"

#include "Celestial/SRCelestialBody.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SROrbit.h"

namespace
{
	constexpr float HubRouteArcHeightRatio = 0.25f;
	constexpr float HubRouteMinimumArcHeight = 600.0f;
	constexpr float HubRouteLaunchVelocityMaxControlDistanceRatio = 0.45f;
	constexpr float HubRouteLaunchVelocityMinimumControlDistance = 600.0f;
	constexpr float HubRouteLaunchAscentEndRatio = 0.12f;
	constexpr float HubRouteSourceOrbitEndRatio = 0.34f;
	constexpr float HubRouteTransferEndRatio = 0.72f;
	constexpr float HubRouteDestinationOrbitEndRatio = 0.90f;
	constexpr float HubRouteLowOrbitMinimumAltitude = 900.0f;
	constexpr float HubRouteLowOrbitRouteDistanceAltitudeRatio = 0.035f;
	constexpr float HubRouteLowOrbitBodyRadiusAltitudeRatio = 0.15f;
	constexpr float HubRouteSourceOrbitExtraRadians = UE_TWO_PI * 0.85f;
	constexpr float HubRouteDestinationOrbitExtraRadians = UE_TWO_PI * 0.70f;
	constexpr float HubRouteAscentControlRatio = 0.45f;
	constexpr float HubRouteLandingControlRatio = 0.45f;
	constexpr float DefaultHubRouteInitialSpeedUnitsPerSecond = 6000.0f;
	constexpr float MinimumHubRouteInitialSpeedUnitsPerSecond = 100.0f;
	constexpr float DefaultHubRouteLaunchAccelerationUnitsPerSecondSquared = 3000.0f;
	constexpr float MinimumHubRouteLaunchAccelerationUnitsPerSecondSquared = 1.0f;
	constexpr float MinimumHubRouteTravelDurationSeconds = 0.5f;

	struct FSRSpaceLogisticsHubRouteOrbitArc
	{
		FVector Center = FVector::ZeroVector;
		FVector StartRadial = FVector::UpVector;
		FVector PlaneNormal = FVector::ForwardVector;
		float Radius = 0.0f;
		float ArcRadians = 0.0f;
	};

	float RemapSegmentAlpha(float Alpha, float SegmentStart, float SegmentEnd)
	{
		return FMath::Clamp((Alpha - SegmentStart) / FMath::Max(SegmentEnd - SegmentStart, UE_SMALL_NUMBER), 0.0f, 1.0f);
	}

	FVector ResolveSafeRadial(const FVector& Center, const FVector& Location, const FVector& FallbackDirection)
	{
		FVector Radial = (Location - Center).GetSafeNormal();
		if (Radial.IsNearlyZero())
		{
			Radial = FallbackDirection.GetSafeNormal();
		}
		if (Radial.IsNearlyZero())
		{
			Radial = FVector::UpVector;
		}
		return Radial;
	}

	FVector ResolveSafePlaneDirection(const FVector& Direction, const FVector& PlaneNormal, const FVector& FallbackDirection)
	{
		const FVector Normal = PlaneNormal.GetSafeNormal();
		FVector PlaneDirection = FVector::VectorPlaneProject(Direction, Normal).GetSafeNormal();
		if (!PlaneDirection.IsNearlyZero())
		{
			return PlaneDirection;
		}

		PlaneDirection = FVector::VectorPlaneProject(FallbackDirection, Normal).GetSafeNormal();
		if (!PlaneDirection.IsNearlyZero())
		{
			return PlaneDirection;
		}

		PlaneDirection = FVector::CrossProduct(Normal, FVector::UpVector).GetSafeNormal();
		if (!PlaneDirection.IsNearlyZero())
		{
			return PlaneDirection;
		}

		PlaneDirection = FVector::CrossProduct(Normal, FVector::RightVector).GetSafeNormal();
		return PlaneDirection.IsNearlyZero() ? FVector::ForwardVector : PlaneDirection;
	}

	bool TryResolveBodyOrbitPlaneNormal(const AActor* BodyActor, FVector& OutPlaneNormal)
	{
		OutPlaneNormal = FVector::ZeroVector;
		const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
		const USROrbit* Orbit = CelestialBody ? CelestialBody->GetOrbit() : nullptr;
		if (Orbit && Orbit->HasOrbit())
		{
			const FVector OrbitPlaneNormal = Orbit->ComputeOrbitPlaneNormal().GetSafeNormal();
			if (!OrbitPlaneNormal.IsNearlyZero())
			{
				OutPlaneNormal = OrbitPlaneNormal;
				return true;
			}
		}

		return false;
	}

	FVector ResolveRouteOrbitPlaneNormal(
		const AActor* StartBodyActor,
		const AActor* TargetBodyActor,
		const FVector& StartCenter,
		const FVector& TargetCenter,
		const FVector& FallbackRouteDirection)
	{
		FVector PlaneNormal = FVector::ZeroVector;
		if (TryResolveBodyOrbitPlaneNormal(StartBodyActor, PlaneNormal))
		{
			return PlaneNormal;
		}

		if (TryResolveBodyOrbitPlaneNormal(TargetBodyActor, PlaneNormal))
		{
			return PlaneNormal;
		}

		PlaneNormal = FVector::CrossProduct(StartCenter, TargetCenter).GetSafeNormal();
		if (!PlaneNormal.IsNearlyZero())
		{
			return PlaneNormal;
		}

		PlaneNormal = FVector::CrossProduct(FallbackRouteDirection, FVector::UpVector).GetSafeNormal();
		return PlaneNormal.IsNearlyZero() ? FVector::XAxisVector : PlaneNormal;
	}

	float PositiveSignedAngleRadians(const FVector& From, const FVector& To, const FVector& Axis)
	{
		const FVector FromNormal = From.GetSafeNormal();
		const FVector ToNormal = To.GetSafeNormal();
		const FVector AxisNormal = Axis.GetSafeNormal();
		if (FromNormal.IsNearlyZero() || ToNormal.IsNearlyZero() || AxisNormal.IsNearlyZero())
		{
			return 0.0f;
		}

		float Angle = FMath::Atan2(
			FVector::DotProduct(AxisNormal, FVector::CrossProduct(FromNormal, ToNormal)),
			FVector::DotProduct(FromNormal, ToNormal));
		if (Angle < 0.0f)
		{
			Angle += UE_TWO_PI;
		}
		return Angle;
	}

	void EvaluateCubicBezier(
		const FVector& P0,
		const FVector& P1,
		const FVector& P2,
		const FVector& P3,
		float Alpha,
		FVector& OutLocation,
		FVector& OutDirection)
	{
		const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
		const float InverseT = 1.0f - T;
		OutLocation = (InverseT * InverseT * InverseT * P0)
			+ (3.0f * InverseT * InverseT * T * P1)
			+ (3.0f * InverseT * T * T * P2)
			+ (T * T * T * P3);

		const FVector Tangent = (3.0f * InverseT * InverseT * (P1 - P0))
			+ (6.0f * InverseT * T * (P2 - P1))
			+ (3.0f * T * T * (P3 - P2));
		OutDirection = Tangent.GetSafeNormal();
		if (OutDirection.IsNearlyZero())
		{
			OutDirection = (P3 - P0).GetSafeNormal();
		}
	}

	void EvaluateOrbitArc(const FSRSpaceLogisticsHubRouteOrbitArc& OrbitArc, float Alpha, FVector& OutLocation, FVector& OutDirection)
	{
		const float Angle = OrbitArc.ArcRadians * FMath::Clamp(Alpha, 0.0f, 1.0f);
		const FQuat Rotation(OrbitArc.PlaneNormal.GetSafeNormal(), Angle);
		const FVector Radial = Rotation.RotateVector(OrbitArc.StartRadial).GetSafeNormal();
		OutLocation = OrbitArc.Center + (Radial * OrbitArc.Radius);
		OutDirection = FVector::CrossProduct(OrbitArc.PlaneNormal, Radial).GetSafeNormal();
	}

	float ResolveLowOrbitRadius(const FVector& Center, const FVector& HubLocation, float RouteDistance)
	{
		const float SurfaceRadius = FVector::Distance(Center, HubLocation);
		const float OrbitAltitude = FMath::Max3(
			HubRouteLowOrbitMinimumAltitude,
			RouteDistance * HubRouteLowOrbitRouteDistanceAltitudeRatio,
			SurfaceRadius * HubRouteLowOrbitBodyRadiusAltitudeRatio);
		return SurfaceRadius + OrbitAltitude;
	}

	float ResolveAcceleratedDistance(float SegmentDistance, float TargetSpeed, float Acceleration, float ElapsedSeconds)
	{
		const float SafeSegmentDistance = FMath::Max(0.0f, SegmentDistance);
		const float SafeTargetSpeed = FSRSpaceLogisticsRoutePathResolver::ClampInitialSpeed(TargetSpeed);
		const float SafeAcceleration = FSRSpaceLogisticsRoutePathResolver::ClampLaunchAcceleration(Acceleration);
		const float SafeElapsedSeconds = FMath::Max(0.0f, ElapsedSeconds);
		const float TimeToTargetSpeed = SafeTargetSpeed / SafeAcceleration;
		const float AccelerationDistance = 0.5f * SafeAcceleration * TimeToTargetSpeed * TimeToTargetSpeed;

		if (SafeElapsedSeconds <= TimeToTargetSpeed)
		{
			return FMath::Min(SafeSegmentDistance, 0.5f * SafeAcceleration * SafeElapsedSeconds * SafeElapsedSeconds);
		}

		const float DistanceAfterAcceleration = AccelerationDistance
			+ (SafeTargetSpeed * (SafeElapsedSeconds - TimeToTargetSpeed));
		return FMath::Min(SafeSegmentDistance, DistanceAfterAcceleration);
	}

	FSRSpaceLogisticsHubEndpoint SelectHubEndpointByDockSide(const FSRSpaceLogisticsHubRoute& HubRoute, ESRSpaceLogisticsHubRouteDockSide DockSide)
	{
		return DockSide == ESRSpaceLogisticsHubRouteDockSide::Destination ? HubRoute.DestinationHub : HubRoute.SourceHub;
	}
}

float FSRSpaceLogisticsRoutePathResolver::GetDefaultInitialSpeedUnitsPerSecond()
{
	return DefaultHubRouteInitialSpeedUnitsPerSecond;
}

float FSRSpaceLogisticsRoutePathResolver::GetDefaultLaunchAccelerationUnitsPerSecondSquared()
{
	return DefaultHubRouteLaunchAccelerationUnitsPerSecondSquared;
}

float FSRSpaceLogisticsRoutePathResolver::GetMinimumTravelDurationSeconds()
{
	return MinimumHubRouteTravelDurationSeconds;
}

float FSRSpaceLogisticsRoutePathResolver::GetLaunchAscentEndRatio()
{
	return HubRouteLaunchAscentEndRatio;
}

float FSRSpaceLogisticsRoutePathResolver::ClampInitialSpeed(float InitialSpeedUnitsPerSecond)
{
	return FMath::Max(
		MinimumHubRouteInitialSpeedUnitsPerSecond,
		InitialSpeedUnitsPerSecond > 0.0f
			? InitialSpeedUnitsPerSecond
			: DefaultHubRouteInitialSpeedUnitsPerSecond);
}

float FSRSpaceLogisticsRoutePathResolver::ClampLaunchAcceleration(float LaunchAccelerationUnitsPerSecondSquared)
{
	return FMath::Max(
		MinimumHubRouteLaunchAccelerationUnitsPerSecondSquared,
		LaunchAccelerationUnitsPerSecondSquared > 0.0f
			? LaunchAccelerationUnitsPerSecondSquared
			: DefaultHubRouteLaunchAccelerationUnitsPerSecondSquared);
}

float FSRSpaceLogisticsRoutePathResolver::ResolveAcceleratedDuration(
	float SegmentDistance,
	float TargetSpeed,
	float Acceleration)
{
	const float SafeSegmentDistance = FMath::Max(0.0f, SegmentDistance);
	if (SafeSegmentDistance <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float SafeTargetSpeed = ClampInitialSpeed(TargetSpeed);
	const float SafeAcceleration = ClampLaunchAcceleration(Acceleration);
	const float TimeToTargetSpeed = SafeTargetSpeed / SafeAcceleration;
	const float AccelerationDistance = 0.5f * SafeAcceleration * TimeToTargetSpeed * TimeToTargetSpeed;
	if (SafeSegmentDistance <= AccelerationDistance)
	{
		return FMath::Sqrt((2.0f * SafeSegmentDistance) / SafeAcceleration);
	}

	return TimeToTargetSpeed + ((SafeSegmentDistance - AccelerationDistance) / SafeTargetSpeed);
}

float FSRSpaceLogisticsRoutePathResolver::ResolveMotionProgressRatio(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	float TravelProgressSeconds)
{
	const float SafeTravelProgressSeconds = FMath::Max(0.0f, TravelProgressSeconds);
	const float PathLength = EstimateRoutePathLength(SpaceLogisticsSubsystem, HubRoute);
	const float LaunchAscentLength = EstimateRoutePathLengthRange(
		SpaceLogisticsSubsystem,
		HubRoute,
		0.0f,
		HubRouteLaunchAscentEndRatio);
	if (PathLength <= UE_SMALL_NUMBER || LaunchAscentLength <= UE_SMALL_NUMBER)
	{
		return FMath::Clamp(
			SafeTravelProgressSeconds / FMath::Max(HubRoute.TravelDurationSeconds, UE_SMALL_NUMBER),
			0.0f,
			1.0f);
	}

	const float InitialSpeed = ClampInitialSpeed(HubRoute.InitialSpeedUnitsPerSecond);
	const float LaunchAcceleration = ClampLaunchAcceleration(HubRoute.LaunchAccelerationUnitsPerSecondSquared);
	const float LaunchAscentDuration = ResolveAcceleratedDuration(
		LaunchAscentLength,
		InitialSpeed,
		LaunchAcceleration);

	if (SafeTravelProgressSeconds <= LaunchAscentDuration)
	{
		const float LaunchAscentDistance = ResolveAcceleratedDistance(
			LaunchAscentLength,
			InitialSpeed,
			LaunchAcceleration,
			SafeTravelProgressSeconds);
		const float LaunchAscentRatio = FMath::Clamp(LaunchAscentDistance / LaunchAscentLength, 0.0f, 1.0f);
		return LaunchAscentRatio * HubRouteLaunchAscentEndRatio;
	}

	const float RestDuration = FMath::Max(
		UE_SMALL_NUMBER,
		HubRoute.TravelDurationSeconds - LaunchAscentDuration);
	const float RestRatio = FMath::Clamp(
		(SafeTravelProgressSeconds - LaunchAscentDuration) / RestDuration,
		0.0f,
		1.0f);
	return FMath::Lerp(HubRouteLaunchAscentEndRatio, 1.0f, RestRatio);
}

bool FSRSpaceLogisticsRoutePathResolver::ResolveTargetWorldLocation(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	FVector& OutTargetLocation)
{
	OutTargetLocation = FVector::ZeroVector;
	if (HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination)
	{
		return SpaceLogisticsSubsystem.ResolveHubEndpointSurfaceWorldLocation(HubRoute.DestinationHub, OutTargetLocation);
	}

	if (HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToSource)
	{
		return SpaceLogisticsSubsystem.ResolveHubEndpointSurfaceWorldLocation(HubRoute.SourceHub, OutTargetLocation);
	}

	return false;
}

bool FSRSpaceLogisticsRoutePathResolver::ResolveVisualWorldLocation(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	FVector& OutLocation,
	FVector& OutTargetLocation,
	FVector& OutTravelDirection)
{
	OutLocation = FVector::ZeroVector;
	OutTargetLocation = FVector::ZeroVector;
	OutTravelDirection = FVector::ZeroVector;

	if (HubRoute.bDebugLocalOrbit)
	{
		const FSRSpaceLogisticsHubEndpoint SourceHub = HubRoute.SourceHub;
		AActor* SourceBodyActor = SourceHub.BodyActor.Get();
		if (!IsValid(SourceBodyActor))
		{
			return false;
		}

		FVector SourceHubWorldLocation = FVector::ZeroVector;
		if (!SpaceLogisticsSubsystem.ResolveHubEndpointSurfaceWorldLocation(SourceHub, SourceHubWorldLocation))
		{
			return false;
		}

		const FVector SourceCenter = SourceBodyActor->GetActorLocation();
		const FVector StartSurfaceRadial = ResolveSafeRadial(SourceCenter, SourceHubWorldLocation, FVector::UpVector);
		const float OrbitRadius = ResolveLowOrbitRadius(SourceCenter, SourceHubWorldLocation, 0.0f);
		const FVector OrbitPlaneNormal = ResolveRouteOrbitPlaneNormal(
			SourceBodyActor,
			SourceBodyActor,
			SourceCenter,
			SourceCenter,
			HubRoute.LaunchWorldVelocity);
		FVector ReferenceDirection = HubRoute.LaunchWorldVelocity.GetSafeNormal();
		if (ReferenceDirection.IsNearlyZero())
		{
			ReferenceDirection = FVector::CrossProduct(OrbitPlaneNormal, StartSurfaceRadial).GetSafeNormal();
		}
		if (ReferenceDirection.IsNearlyZero())
		{
			ReferenceDirection = FVector::CrossProduct(OrbitPlaneNormal, FVector::RightVector).GetSafeNormal();
		}
		const FVector StartOrbitRadial = ResolveSafePlaneDirection(StartSurfaceRadial, OrbitPlaneNormal, ReferenceDirection);

		const FSRSpaceLogisticsHubRouteOrbitArc DebugOrbitArc = {
			SourceCenter,
			StartOrbitRadial,
			OrbitPlaneNormal,
			OrbitRadius,
			UE_TWO_PI
		};

		const float Alpha = FMath::Clamp(HubRoute.TravelProgressRatio, 0.0f, 1.0f);
		const FVector SourceOrbitStart = SourceCenter + (StartOrbitRadial * OrbitRadius);
		if (Alpha <= HubRouteLaunchAscentEndRatio)
		{
			const float SegmentAlpha = RemapSegmentAlpha(Alpha, 0.0f, HubRouteLaunchAscentEndRatio);
			const float AscentDistance = FVector::Distance(SourceHubWorldLocation, SourceOrbitStart);
			const float ControlDistance = FMath::Max(HubRouteMinimumArcHeight, AscentDistance * HubRouteAscentControlRatio);
			const FVector SourceOrbitStartDirection = FVector::CrossProduct(DebugOrbitArc.PlaneNormal, StartOrbitRadial).GetSafeNormal();
			EvaluateCubicBezier(
				SourceHubWorldLocation,
				SourceHubWorldLocation + (StartSurfaceRadial * ControlDistance),
				SourceOrbitStart - (SourceOrbitStartDirection * ControlDistance),
				SourceOrbitStart,
				SegmentAlpha,
				OutLocation,
				OutTravelDirection);
			OutTargetLocation = SourceHubWorldLocation;
			return true;
		}

		EvaluateOrbitArc(
			DebugOrbitArc,
			RemapSegmentAlpha(Alpha, HubRouteLaunchAscentEndRatio, 1.0f),
			OutLocation,
			OutTravelDirection);
		OutTargetLocation = SourceHubWorldLocation;
		return true;
	}

	const ESRSpaceLogisticsHubRouteDockSide StartDockSide = HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
		? ESRSpaceLogisticsHubRouteDockSide::Source
		: ESRSpaceLogisticsHubRouteDockSide::Destination;
	const ESRSpaceLogisticsHubRouteDockSide TargetDockSide = StartDockSide == ESRSpaceLogisticsHubRouteDockSide::Source
		? ESRSpaceLogisticsHubRouteDockSide::Destination
		: ESRSpaceLogisticsHubRouteDockSide::Source;
	const FSRSpaceLogisticsHubEndpoint StartHub = SelectHubEndpointByDockSide(HubRoute, StartDockSide);
	const FSRSpaceLogisticsHubEndpoint TargetHub = SelectHubEndpointByDockSide(HubRoute, TargetDockSide);

	AActor* StartBodyActor = StartHub.BodyActor.Get();
	AActor* TargetBodyActor = TargetHub.BodyActor.Get();
	if (!IsValid(StartBodyActor) || !IsValid(TargetBodyActor))
	{
		return false;
	}

	FVector StartHubWorldLocation = FVector::ZeroVector;
	if (!SpaceLogisticsSubsystem.ResolveHubEndpointSurfaceWorldLocation(StartHub, StartHubWorldLocation))
	{
		if (HubRoute.bHasTravelStartWorldLocation)
		{
			StartHubWorldLocation = HubRoute.TravelStartWorldLocation;
		}
		else
		{
			return false;
		}
	}

	FVector TargetHubWorldLocation = FVector::ZeroVector;
	if (!SpaceLogisticsSubsystem.ResolveHubEndpointSurfaceWorldLocation(TargetHub, TargetHubWorldLocation))
	{
		return false;
	}
	OutTargetLocation = TargetHubWorldLocation;

	const float Alpha = FMath::Clamp(HubRoute.TravelProgressRatio, 0.0f, 1.0f);
	const FVector StartCenter = StartBodyActor->GetActorLocation();
	const FVector TargetCenter = TargetBodyActor->GetActorLocation();
	const float RouteDistance = FVector::Distance(StartHubWorldLocation, TargetHubWorldLocation);
	const FVector RouteDirection = (TargetHubWorldLocation - StartHubWorldLocation).GetSafeNormal();
	const FVector StartSurfaceRadial = ResolveSafeRadial(StartCenter, StartHubWorldLocation, -RouteDirection);
	const FVector TargetSurfaceRadial = ResolveSafeRadial(TargetCenter, TargetHubWorldLocation, RouteDirection);
	const FVector TransferPlaneNormal = ResolveRouteOrbitPlaneNormal(
		StartBodyActor,
		TargetBodyActor,
		StartCenter,
		TargetCenter,
		RouteDirection);
	const FVector PlaneRouteDirection = ResolveSafePlaneDirection(RouteDirection, TransferPlaneNormal, TargetCenter - StartCenter);
	const FVector StartOrbitRadial = ResolveSafePlaneDirection(StartSurfaceRadial, TransferPlaneNormal, -PlaneRouteDirection);
	const FVector TargetOrbitLandingRadial = ResolveSafePlaneDirection(TargetSurfaceRadial, TransferPlaneNormal, PlaneRouteDirection);
	const float SourceOrbitRadius = ResolveLowOrbitRadius(StartCenter, StartHubWorldLocation, RouteDistance);
	const float TargetOrbitRadius = ResolveLowOrbitRadius(TargetCenter, TargetHubWorldLocation, RouteDistance);
	const FVector SourceOrbitStart = StartCenter + (StartOrbitRadial * SourceOrbitRadius);

	FVector SourceTransferDirection = ResolveSafePlaneDirection(TargetCenter - SourceOrbitStart, TransferPlaneNormal, PlaneRouteDirection);
	if (SourceTransferDirection.IsNearlyZero())
	{
		SourceTransferDirection = PlaneRouteDirection.IsNearlyZero() ? StartOrbitRadial : PlaneRouteDirection;
	}
	const FVector SourceOrbitNormal = TransferPlaneNormal;
	FVector SourceExitRadial = FVector::CrossProduct(SourceTransferDirection, SourceOrbitNormal).GetSafeNormal();
	if (SourceExitRadial.IsNearlyZero())
	{
		SourceExitRadial = StartOrbitRadial;
	}
	const FSRSpaceLogisticsHubRouteOrbitArc SourceOrbitArc = {
		StartCenter,
		StartOrbitRadial,
		SourceOrbitNormal,
		SourceOrbitRadius,
		PositiveSignedAngleRadians(StartOrbitRadial, SourceExitRadial, SourceOrbitNormal) + HubRouteSourceOrbitExtraRadians
	};

	FVector SourceOrbitExit = FVector::ZeroVector;
	FVector SourceOrbitExitDirection = FVector::ZeroVector;
	EvaluateOrbitArc(SourceOrbitArc, 1.0f, SourceOrbitExit, SourceOrbitExitDirection);

	FVector IncomingTargetDirection = ResolveSafePlaneDirection(TargetCenter - SourceOrbitExit, TransferPlaneNormal, PlaneRouteDirection);
	if (IncomingTargetDirection.IsNearlyZero())
	{
		IncomingTargetDirection = PlaneRouteDirection.IsNearlyZero() ? TargetOrbitLandingRadial : PlaneRouteDirection;
	}
	const FVector TargetOrbitNormal = TransferPlaneNormal;
	FVector TargetOrbitStartRadial = FVector::CrossProduct(IncomingTargetDirection, TargetOrbitNormal).GetSafeNormal();
	if (TargetOrbitStartRadial.IsNearlyZero())
	{
		TargetOrbitStartRadial = TargetOrbitLandingRadial;
	}
	const FSRSpaceLogisticsHubRouteOrbitArc TargetOrbitArc = {
		TargetCenter,
		TargetOrbitStartRadial,
		TargetOrbitNormal,
		TargetOrbitRadius,
		PositiveSignedAngleRadians(TargetOrbitStartRadial, TargetOrbitLandingRadial, TargetOrbitNormal) + HubRouteDestinationOrbitExtraRadians
	};

	FVector TargetOrbitEntry = FVector::ZeroVector;
	FVector TargetOrbitEntryDirection = FVector::ZeroVector;
	EvaluateOrbitArc(TargetOrbitArc, 0.0f, TargetOrbitEntry, TargetOrbitEntryDirection);

	FVector TargetOrbitExit = FVector::ZeroVector;
	FVector TargetOrbitExitDirection = FVector::ZeroVector;
	EvaluateOrbitArc(TargetOrbitArc, 1.0f, TargetOrbitExit, TargetOrbitExitDirection);

	if (Alpha <= HubRouteLaunchAscentEndRatio)
	{
		const float SegmentAlpha = RemapSegmentAlpha(Alpha, 0.0f, HubRouteLaunchAscentEndRatio);
		const float AscentDistance = FVector::Distance(StartHubWorldLocation, SourceOrbitStart);
		const float ControlDistance = FMath::Max(HubRouteMinimumArcHeight, AscentDistance * HubRouteAscentControlRatio);
		const FVector SourceOrbitStartDirection = FVector::CrossProduct(SourceOrbitNormal, StartOrbitRadial).GetSafeNormal();
		EvaluateCubicBezier(
			StartHubWorldLocation,
			StartHubWorldLocation + (StartSurfaceRadial * ControlDistance),
			SourceOrbitStart - (SourceOrbitStartDirection * ControlDistance),
			SourceOrbitStart,
			SegmentAlpha,
			OutLocation,
			OutTravelDirection);
		return true;
	}

	if (Alpha <= HubRouteSourceOrbitEndRatio)
	{
		EvaluateOrbitArc(
			SourceOrbitArc,
			RemapSegmentAlpha(Alpha, HubRouteLaunchAscentEndRatio, HubRouteSourceOrbitEndRatio),
			OutLocation,
			OutTravelDirection);
		return true;
	}

	if (Alpha <= HubRouteTransferEndRatio)
	{
		const float SegmentAlpha = RemapSegmentAlpha(Alpha, HubRouteSourceOrbitEndRatio, HubRouteTransferEndRatio);
		const float TransferDistance = FVector::Distance(SourceOrbitExit, TargetOrbitEntry);
		const float ControlDistance = FMath::Max(
			HubRouteLaunchVelocityMinimumControlDistance,
			TransferDistance * HubRouteLaunchVelocityMaxControlDistanceRatio);

		FVector TransferArcDirection = FVector::ZeroVector;
		const FVector TransferMidpoint = (SourceOrbitExit + TargetOrbitEntry) * 0.5f;
		if (const UWorld* World = SpaceLogisticsSubsystem.GetWorld())
		{
			if (const USRCelestialBodyRegistrySubsystem* CelestialRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
			{
				if (const AActor* PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor())
				{
					TransferArcDirection = (TransferMidpoint - PrimaryStarActor->GetActorLocation()).GetSafeNormal();
				}
			}
		}
		if (TransferArcDirection.IsNearlyZero())
		{
			TransferArcDirection = TransferMidpoint.GetSafeNormal();
		}
		if (TransferArcDirection.IsNearlyZero())
		{
			TransferArcDirection = FVector::UpVector;
		}

		FVector DepartureDirection = SourceOrbitExitDirection;
		if (HubRoute.bHasLaunchWorldVelocity && !HubRoute.LaunchWorldVelocity.IsNearlyZero())
		{
			const FVector LaunchVelocityDirection = HubRoute.LaunchWorldVelocity.GetSafeNormal();
			DepartureDirection = (SourceOrbitExitDirection + (LaunchVelocityDirection * 0.25f)).GetSafeNormal();
		}
		if (DepartureDirection.IsNearlyZero())
		{
			DepartureDirection = (TargetOrbitEntry - SourceOrbitExit).GetSafeNormal();
		}

		const FVector ArrivalDirection = TargetOrbitEntryDirection.IsNearlyZero()
			? (TargetOrbitEntry - SourceOrbitExit).GetSafeNormal()
			: TargetOrbitEntryDirection;
		const FVector TransferLift = TransferArcDirection * FMath::Max(HubRouteMinimumArcHeight, TransferDistance * HubRouteArcHeightRatio);
		EvaluateCubicBezier(
			SourceOrbitExit,
			SourceOrbitExit + (DepartureDirection * ControlDistance) + TransferLift,
			TargetOrbitEntry - (ArrivalDirection * ControlDistance) + TransferLift,
			TargetOrbitEntry,
			SegmentAlpha,
			OutLocation,
			OutTravelDirection);
		return true;
	}

	if (Alpha <= HubRouteDestinationOrbitEndRatio)
	{
		EvaluateOrbitArc(
			TargetOrbitArc,
			RemapSegmentAlpha(Alpha, HubRouteTransferEndRatio, HubRouteDestinationOrbitEndRatio),
			OutLocation,
			OutTravelDirection);
		return true;
	}

	const float SegmentAlpha = RemapSegmentAlpha(Alpha, HubRouteDestinationOrbitEndRatio, 1.0f);
	const float LandingDistance = FVector::Distance(TargetOrbitExit, TargetHubWorldLocation);
	const float ControlDistance = FMath::Max(HubRouteMinimumArcHeight, LandingDistance * HubRouteLandingControlRatio);
	EvaluateCubicBezier(
		TargetOrbitExit,
		TargetOrbitExit + (TargetOrbitExitDirection * ControlDistance),
		TargetHubWorldLocation + (TargetSurfaceRadial * ControlDistance),
		TargetHubWorldLocation,
		SegmentAlpha,
		OutLocation,
		OutTravelDirection);
	if (OutTravelDirection.IsNearlyZero())
	{
		OutTravelDirection = (TargetHubWorldLocation - TargetOrbitExit).GetSafeNormal();
	}
	return true;
}

float FSRSpaceLogisticsRoutePathResolver::EstimateRoutePathLength(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	int32 SampleCount)
{
	return EstimateRoutePathLengthRange(SpaceLogisticsSubsystem, HubRoute, 0.0f, 1.0f, SampleCount);
}

float FSRSpaceLogisticsRoutePathResolver::EstimateRoutePathLengthRange(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	float StartAlpha,
	float EndAlpha,
	int32 SampleCount)
{
	const int32 ClampedSampleCount = FMath::Max(2, SampleCount);
	const float ClampedStartAlpha = FMath::Clamp(StartAlpha, 0.0f, 1.0f);
	const float ClampedEndAlpha = FMath::Clamp(EndAlpha, ClampedStartAlpha, 1.0f);
	FSRSpaceLogisticsHubRoute SampleRoute = HubRoute;
	float PathLength = 0.0f;
	FVector PreviousLocation = FVector::ZeroVector;
	bool bHasPreviousLocation = false;

	for (int32 SampleIndex = 0; SampleIndex <= ClampedSampleCount; ++SampleIndex)
	{
		const float SegmentAlpha = static_cast<float>(SampleIndex) / static_cast<float>(ClampedSampleCount);
		SampleRoute.TravelProgressRatio = FMath::Lerp(ClampedStartAlpha, ClampedEndAlpha, SegmentAlpha);

		FVector SampleLocation = FVector::ZeroVector;
		FVector SampleTargetLocation = FVector::ZeroVector;
		FVector SampleTravelDirection = FVector::ZeroVector;
		if (!ResolveVisualWorldLocation(SpaceLogisticsSubsystem, SampleRoute, SampleLocation, SampleTargetLocation, SampleTravelDirection))
		{
			return 0.0f;
		}

		if (bHasPreviousLocation)
		{
			PathLength += FVector::Distance(PreviousLocation, SampleLocation);
		}

		PreviousLocation = SampleLocation;
		bHasPreviousLocation = true;
	}

	return PathLength;
}

float FSRSpaceLogisticsRoutePathResolver::ResolveTravelDurationSeconds(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute)
{
	float PathLength = EstimateRoutePathLength(SpaceLogisticsSubsystem, HubRoute);
	if (PathLength <= UE_SMALL_NUMBER)
	{
		FVector TargetLocation = FVector::ZeroVector;
		if (ResolveTargetWorldLocation(SpaceLogisticsSubsystem, HubRoute, TargetLocation))
		{
			PathLength = FVector::Distance(HubRoute.TravelStartWorldLocation, TargetLocation);
		}
	}

	const float InitialSpeed = ClampInitialSpeed(HubRoute.InitialSpeedUnitsPerSecond);
	const float LaunchAcceleration = ClampLaunchAcceleration(HubRoute.LaunchAccelerationUnitsPerSecondSquared);
	const float LaunchAscentLength = EstimateRoutePathLengthRange(
		SpaceLogisticsSubsystem,
		HubRoute,
		0.0f,
		HubRouteLaunchAscentEndRatio);
	const float RestLength = FMath::Max(0.0f, PathLength - LaunchAscentLength);
	const float LaunchAscentDuration = ResolveAcceleratedDuration(
		LaunchAscentLength,
		InitialSpeed,
		LaunchAcceleration);
	const float RestDuration = RestLength / InitialSpeed;
	return FMath::Max(MinimumHubRouteTravelDurationSeconds, LaunchAscentDuration + RestDuration);
}
