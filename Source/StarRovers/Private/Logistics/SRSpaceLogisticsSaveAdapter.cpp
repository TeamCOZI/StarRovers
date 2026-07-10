#include "SRSpaceLogisticsSaveAdapter.h"

#include "SRSpaceLogisticsRoutePathResolver.h"
#include "SRSpaceLogisticsRouteRegistry.h"
#include "SRSpaceLogisticsRouteVisualController.h"
#include "Utility/SRLog.h"

void FSRSpaceLogisticsSaveAdapter::ExportSaveData(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	int32 NextHubRouteSequence,
	FSRSpaceLogisticsSaveData& OutSaveData)
{
	OutSaveData = FSRSpaceLogisticsSaveData();
	OutSaveData.NextHubRouteSequence = FMath::Max(1, NextHubRouteSequence);
	OutSaveData.HubRoutes.Reserve(HubRoutes.Num());

	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		FSRSpaceLogisticsHubRouteSaveData RouteSaveData;
		if (BuildRouteSaveData(SpaceLogisticsSubsystem, HubRoute, RouteSaveData))
		{
			OutSaveData.HubRoutes.Add(RouteSaveData);
		}
	}
}

bool FSRSpaceLogisticsSaveAdapter::ImportSaveData(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsSaveData& SaveData,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	int32& NextHubRouteSequence,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
	TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples)
{
	FSRSpaceLogisticsRouteVisualController::Clear(SpaceshipActorsByRouteId);
	HubRoutes.Reset();
	HubEndpointMotionSamples.Reset();
	NextHubRouteSequence = FMath::Max(1, SaveData.NextHubRouteSequence);

	int32 ImportedRouteCount = 0;
	for (const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData : SaveData.HubRoutes)
	{
		if (ImportRoute(SpaceLogisticsSubsystem, RouteSaveData, HubRoutes))
		{
			++ImportedRouteCount;
		}
	}

	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Save data imported: ImportedRoutes=%d SavedRoutes=%d NextRouteSequence=%d"),
		ImportedRouteCount,
		SaveData.HubRoutes.Num(),
		NextHubRouteSequence);
	FSRSpaceLogisticsRouteVisualController::Refresh(
		SpaceLogisticsSubsystem,
		SpaceLogisticsSubsystem.GetWorld(),
		HubRoutes,
		SpaceshipActorsByRouteId);
	return ImportedRouteCount > 0 || SaveData.HubRoutes.IsEmpty();
}

bool FSRSpaceLogisticsSaveAdapter::BuildRouteSaveData(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	FSRSpaceLogisticsHubRouteSaveData& OutRouteSaveData)
{
	OutRouteSaveData = FSRSpaceLogisticsHubRouteSaveData();
	OutRouteSaveData.RouteId = HubRoute.RouteId;
	if (!SpaceLogisticsSubsystem.BuildHubEndpointSaveData(HubRoute.SourceHub, OutRouteSaveData.SourceHub)
		|| !SpaceLogisticsSubsystem.BuildHubEndpointSaveData(HubRoute.DestinationHub, OutRouteSaveData.DestinationHub))
	{
		return false;
	}

	OutRouteSaveData.bEnabled = HubRoute.bEnabled;
	OutRouteSaveData.bReturnEmptyWhenNoCargo = HubRoute.bReturnEmptyWhenNoCargo;
	OutRouteSaveData.MaxCargoStackCount = HubRoute.MaxCargoStackCount;
	OutRouteSaveData.CargoResourceId = HubRoute.CargoResourceId;
	OutRouteSaveData.bDebugLocalOrbit = HubRoute.bDebugLocalOrbit;
	OutRouteSaveData.Phase = HubRoute.Phase;
	OutRouteSaveData.CurrentDockSide = HubRoute.CurrentDockSide;
	OutRouteSaveData.TravelDurationSeconds = HubRoute.TravelDurationSeconds;
	OutRouteSaveData.InitialSpeedUnitsPerSecond = HubRoute.InitialSpeedUnitsPerSecond;
	OutRouteSaveData.LaunchAccelerationUnitsPerSecondSquared = HubRoute.LaunchAccelerationUnitsPerSecondSquared;
	OutRouteSaveData.TravelProgressSeconds = HubRoute.TravelProgressSeconds;
	OutRouteSaveData.TravelProgressRatio = HubRoute.TravelProgressRatio;
	OutRouteSaveData.TravelStartWorldLocation = HubRoute.TravelStartWorldLocation;
	OutRouteSaveData.bHasTravelStartWorldLocation = HubRoute.bHasTravelStartWorldLocation;
	OutRouteSaveData.LaunchWorldVelocity = HubRoute.LaunchWorldVelocity;
	OutRouteSaveData.bHasLaunchWorldVelocity = HubRoute.bHasLaunchWorldVelocity;
	OutRouteSaveData.Cargo = HubRoute.Cargo;
	return true;
}

bool FSRSpaceLogisticsSaveAdapter::ImportRoute(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes)
{
	if (!RouteSaveData.IsValid())
	{
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	FSRSpaceLogisticsHubEndpoint DestinationHub;
	if (!SpaceLogisticsSubsystem.ResolveSavedHubEndpoint(RouteSaveData.SourceHub, SourceHub)
		|| !SpaceLogisticsSubsystem.ResolveSavedHubEndpoint(RouteSaveData.DestinationHub, DestinationHub)
		|| (!RouteSaveData.bDebugLocalOrbit && FSRSpaceLogisticsRouteRegistry::AreHubEndpointKeysEqual(SourceHub, DestinationHub))
		|| FSRSpaceLogisticsRouteRegistry::DoesRouteAlreadyExist(HubRoutes, SourceHub, DestinationHub))
	{
		return false;
	}

	FSRSpaceLogisticsHubRoute& HubRoute = HubRoutes.AddDefaulted_GetRef();
	HubRoute.RouteId = RouteSaveData.RouteId;
	HubRoute.SourceHub = SourceHub;
	HubRoute.DestinationHub = RouteSaveData.bDebugLocalOrbit ? SourceHub : DestinationHub;
	HubRoute.bEnabled = RouteSaveData.bEnabled;
	HubRoute.bReturnEmptyWhenNoCargo = RouteSaveData.bReturnEmptyWhenNoCargo;
	HubRoute.MaxCargoStackCount = FMath::Max(1, RouteSaveData.MaxCargoStackCount);
	HubRoute.CargoResourceId = RouteSaveData.CargoResourceId;
	HubRoute.bDebugLocalOrbit = RouteSaveData.bDebugLocalOrbit;
	HubRoute.Phase = RouteSaveData.Phase;
	HubRoute.CurrentDockSide = RouteSaveData.CurrentDockSide;
	HubRoute.TravelDurationSeconds = FMath::Max(0.01f, RouteSaveData.TravelDurationSeconds);
	HubRoute.InitialSpeedUnitsPerSecond =
		FSRSpaceLogisticsRoutePathResolver::ClampInitialSpeed(RouteSaveData.InitialSpeedUnitsPerSecond);
	HubRoute.LaunchAccelerationUnitsPerSecondSquared =
		FSRSpaceLogisticsRoutePathResolver::ClampLaunchAcceleration(RouteSaveData.LaunchAccelerationUnitsPerSecondSquared);
	HubRoute.TravelProgressSeconds = FMath::Max(0.0f, RouteSaveData.TravelProgressSeconds);
	HubRoute.TravelProgressRatio = FMath::Clamp(RouteSaveData.TravelProgressRatio, 0.0f, 1.0f);
	HubRoute.TravelStartWorldLocation = RouteSaveData.TravelStartWorldLocation;
	HubRoute.bHasTravelStartWorldLocation = RouteSaveData.bHasTravelStartWorldLocation;
	HubRoute.LaunchWorldVelocity = RouteSaveData.LaunchWorldVelocity;
	HubRoute.bHasLaunchWorldVelocity = RouteSaveData.bHasLaunchWorldVelocity;
	HubRoute.Cargo = RouteSaveData.Cargo;
	return true;
}
