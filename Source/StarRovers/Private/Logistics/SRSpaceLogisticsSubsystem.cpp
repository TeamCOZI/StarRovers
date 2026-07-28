#include "Logistics/SRSpaceLogisticsSubsystem.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "Logistics/SRFleetCapacityV2.h"
#include "Logistics/SRConditionedTransitV2.h"
#include "SRSpaceLogisticsHubEndpointResolver.h"
#include "SRSpaceLogisticsHubEndpointMotionTracker.h"
#include "SRSpaceLogisticsRouteProcessor.h"
#include "SRSpaceLogisticsRouteRegistry.h"
#include "SRSpaceLogisticsSaveAdapter.h"
#include "SRSpaceLogisticsStarFuelMissileProcessor.h"
#include "SRSpaceLogisticsRouteVisualController.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRAugmentSubsystem.h"

namespace
{
	constexpr float HubDockingHeightOffset = 320.0f;
}

void USRSpaceLogisticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CachedHubEndpoints.Reset();
	HubEndpointMotionSamples.Reset();
}

void USRSpaceLogisticsSubsystem::Deinitialize()
{
	FSRSpaceLogisticsRouteVisualController::Clear(SpaceshipActorsByRouteId);
	FSRSpaceLogisticsRouteVisualController::Clear(StarFuelMissileActorsByMissileId);
	CachedHubEndpoints.Reset();
	HubRoutes.Reset();
	StarFuelMissiles.Reset();
	HubEndpointMotionSamples.Reset();
	Super::Deinitialize();
}

void USRSpaceLogisticsSubsystem::Tick(float DeltaTime)
{
	const float SimulationDeltaTime = ResolveSimulationDeltaSeconds(DeltaTime);
	if (SimulationDeltaTime <= 0.0f)
	{
		return;
	}

	FSRSpaceLogisticsHubEndpointMotionTracker::Update(
		*this,
		SimulationDeltaTime,
		CachedHubEndpoints,
		HubEndpointMotionSamples);
	if (HubRoutes.IsEmpty())
	{
		if (StarFuelMissiles.IsEmpty())
		{
			return;
		}
	}

	if (!HubRoutes.IsEmpty())
	{
	FSRSpaceLogisticsRouteProcessor::ProcessRoutes(
			*this,
			SimulationDeltaTime,
			HubRoutes,
			SpaceshipActorsByRouteId,
			NextFleetDepartureQueueSequence);
		FSRSpaceLogisticsRouteVisualController::Refresh(*this, GetWorld(), HubRoutes, SpaceshipActorsByRouteId);
	}

	if (!StarFuelMissiles.IsEmpty())
	{
		FSRSpaceLogisticsStarFuelMissileProcessor::ProcessMissiles(
			*this,
			SimulationDeltaTime,
			StarFuelMissiles,
			StarFuelMissileActorsByMissileId);
		FSRSpaceLogisticsRouteVisualController::RefreshStarFuelMissiles(
			*this,
			GetWorld(),
			StarFuelMissiles,
			StarFuelMissileActorsByMissileId);
	}
}

TStatId USRSpaceLogisticsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USRSpaceLogisticsSubsystem, STATGROUP_Tickables);
}

void USRSpaceLogisticsSubsystem::RefreshHubEndpoints()
{
	RebuildHubEndpoints();
}

void USRSpaceLogisticsSubsystem::GetHubEndpoints(TArray<FSRSpaceLogisticsHubEndpoint>& OutHubEndpoints) const
{
	RebuildHubEndpoints();
	OutHubEndpoints = CachedHubEndpoints;
}

bool USRSpaceLogisticsSubsystem::GetHubEndpoint(AActor* BodyActor, FName HubOccupantId, FSRSpaceLogisticsHubEndpoint& OutHubEndpoint) const
{
	OutHubEndpoint = FSRSpaceLogisticsHubEndpoint();
	if (!IsValid(BodyActor) || HubOccupantId.IsNone())
	{
		return false;
	}

	RebuildHubEndpoints();
	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : CachedHubEndpoints)
	{
		if (HubEndpoint.BodyActor == BodyActor && HubEndpoint.HubOccupantId == HubOccupantId)
		{
			OutHubEndpoint = HubEndpoint;
			return true;
		}
	}

	return false;
}

bool USRSpaceLogisticsSubsystem::ResolveHubEndpointWorldLocation(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, FVector& OutWorldLocation) const
{
	return ResolveHubEndpointWorldLocationWithHeightOffset(HubEndpoint, HubDockingHeightOffset, OutWorldLocation);
}

bool USRSpaceLogisticsSubsystem::ResolveHubEndpointWorldLocationWithHeightOffset(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	float HeightOffset,
	FVector& OutWorldLocation) const
{
	return FSRSpaceLogisticsHubEndpointResolver::ResolveWorldLocationWithHeightOffset(
		HubEndpoint,
		HeightOffset,
		OutWorldLocation);
}

bool USRSpaceLogisticsSubsystem::ResolveHubEndpointSurfaceWorldLocation(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	FVector& OutWorldLocation) const
{
	return FSRSpaceLogisticsHubEndpointResolver::ResolveSurfaceWorldLocation(
		HubEndpoint,
		HubDockingHeightOffset,
		OutWorldLocation);
}

bool USRSpaceLogisticsSubsystem::CreateHubRoute(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	const FSRSpaceLogisticsHubEndpoint& DestinationHub,
	FName& OutRouteId,
	bool bReturnEmptyWhenNoCargo,
	int32 MaxCargoStackCount,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared)
{
	return FSRSpaceLogisticsRouteRegistry::CreateHubRoute(
		*this,
		SourceHub,
		DestinationHub,
		OutRouteId,
		bReturnEmptyWhenNoCargo,
		MaxCargoStackCount,
		InitialSpeedUnitsPerSecond,
		LaunchAccelerationUnitsPerSecondSquared,
		HubRoutes,
		NextHubRouteSequence);
}

bool USRSpaceLogisticsSubsystem::CreateDebugLocalOrbitRoute(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	FName& OutRouteId,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared)
{
	return FSRSpaceLogisticsRouteRegistry::CreateDebugLocalOrbitRoute(
		*this,
		SourceHub,
		OutRouteId,
		InitialSpeedUnitsPerSecond,
		LaunchAccelerationUnitsPerSecondSquared,
		HubRoutes,
		NextHubRouteSequence);
}

bool USRSpaceLogisticsSubsystem::RemoveHubRoute(FName RouteId)
{
	return FSRSpaceLogisticsRouteRegistry::RemoveHubRoute(RouteId, HubRoutes, SpaceshipActorsByRouteId);
}

bool USRSpaceLogisticsSubsystem::SetHubRouteMaxCargoStackCount(FName RouteId, int32 MaxCargoStackCount)
{
	return FSRSpaceLogisticsRouteRegistry::SetHubRouteMaxCargoStackCount(RouteId, MaxCargoStackCount, HubRoutes);
}

bool USRSpaceLogisticsSubsystem::SetHubRouteReturnEmptyWhenNoCargo(FName RouteId, bool bReturnEmptyWhenNoCargo)
{
	return FSRSpaceLogisticsRouteRegistry::SetHubRouteReturnEmptyWhenNoCargo(
		RouteId,
		bReturnEmptyWhenNoCargo,
		HubRoutes);
}

bool USRSpaceLogisticsSubsystem::SetHubRouteCargoResourceId(FName RouteId, FName CargoResourceId)
{
	return FSRSpaceLogisticsRouteRegistry::SetHubRouteCargoResourceId(RouteId, CargoResourceId, HubRoutes);
}

bool USRSpaceLogisticsSubsystem::SetHubRouteProfile(
	FName RouteId,
	ESRSpaceLogisticsRouteProfileV2 RouteProfile)
{
	if (!IsHubRouteProfileUnlocked(RouteProfile))
	{
		return false;
	}
	return FSRSpaceLogisticsRouteRegistry::SetHubRouteProfile(RouteId, RouteProfile, HubRoutes);
}

bool USRSpaceLogisticsSubsystem::IsHubRouteProfileUnlocked(
	ESRSpaceLogisticsRouteProfileV2 RouteProfile) const
{
	if (!IsFleetCapacityRulesActive())
	{
		return true;
	}

	const FSRSpaceLogisticsRouteProfileRulesV2 Rules =
		FSRFleetCapacityV2::GetRouteProfileRules(RouteProfile);
	if (Rules.Profile != RouteProfile || Rules.ProfileId.IsNone())
	{
		return false;
	}
	if (!Rules.bRequiresAugmentUnlock)
	{
		return true;
	}

	const USRAugmentSubsystem* AugmentSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<USRAugmentSubsystem>()
		: nullptr;
	return IsValid(AugmentSubsystem)
		&& AugmentSubsystem->IsRouteProfileUnlockedV2(Rules.ProfileId);
}

bool USRSpaceLogisticsSubsystem::SetHubRouteConditionedTransitModule(
	FName RouteId,
	ESRConditionedTransitModuleV2 ConditionedTransitModule)
{
	if (ConditionedTransitModule != ESRConditionedTransitModuleV2::None)
	{
		if (!IsFleetCapacityRulesActive())
		{
			return false;
		}
		const FSRConditionedTransitModuleRulesV2 Rules =
			FSRConditionedTransitV2::GetModuleRules(ConditionedTransitModule);
		const USRAugmentSubsystem* AugmentSubsystem = GetWorld()
			? GetWorld()->GetSubsystem<USRAugmentSubsystem>()
			: nullptr;
		if (!Rules.IsConditionedModule()
			|| !IsValid(AugmentSubsystem)
			|| !AugmentSubsystem->IsLogisticsModuleUnlockedV2(Rules.UnlockModuleId))
		{
			return false;
		}
	}

	return FSRSpaceLogisticsRouteRegistry::SetHubRouteConditionedTransitModule(
		RouteId,
		ConditionedTransitModule,
		HubRoutes);
}

FSRFleetCapacityReportV2 USRSpaceLogisticsSubsystem::GetHubFleetCapacityReport(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint) const
{
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	const bool bRulesActive = IsFleetCapacityRulesActive();
	return FSRFleetCapacityV2::BuildReport(
		HubEndpoint,
		HubRoutes,
		bRulesActive,
		bRulesActive ? ResolveActiveFleetBerthCountForHub(HubEndpoint) : 0,
		IsValid(Settings) ? Settings->BaseFleetCapacityV2 : 8,
		IsValid(Settings) ? Settings->FleetBerthCapacityV2 : 8);
}

bool USRSpaceLogisticsSubsystem::LaunchStarFuelMissileFromHub(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	FName& OutMissileId,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared)
{
	return FSRSpaceLogisticsStarFuelMissileProcessor::LaunchFromHub(
		*this,
		SourceHub,
		OutMissileId,
		InitialSpeedUnitsPerSecond,
		LaunchAccelerationUnitsPerSecondSquared,
		StarFuelMissiles,
		NextStarFuelMissileSequence);
}

bool USRSpaceLogisticsSubsystem::LaunchStarFuelMissileFromHubInputPort(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	int32 InputPortIndex,
	FName& OutMissileId,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared)
{
	return FSRSpaceLogisticsStarFuelMissileProcessor::LaunchFromHubInputPort(
		*this,
		SourceHub,
		InputPortIndex,
		OutMissileId,
		InitialSpeedUnitsPerSecond,
		LaunchAccelerationUnitsPerSecondSquared,
		StarFuelMissiles,
		NextStarFuelMissileSequence);
}

void USRSpaceLogisticsSubsystem::ClearHubRoutes()
{
	FSRSpaceLogisticsRouteRegistry::ClearHubRoutes(HubRoutes, SpaceshipActorsByRouteId);
	HubEndpointMotionSamples.Reset();
}

void USRSpaceLogisticsSubsystem::GetHubRoutes(TArray<FSRSpaceLogisticsHubRoute>& OutRoutes) const
{
	FSRSpaceLogisticsRouteRegistry::GetHubRoutes(HubRoutes, OutRoutes);
}

bool USRSpaceLogisticsSubsystem::GetHubRoute(FName RouteId, FSRSpaceLogisticsHubRoute& OutRoute) const
{
	return FSRSpaceLogisticsRouteRegistry::GetHubRoute(RouteId, HubRoutes, OutRoute);
}

void USRSpaceLogisticsSubsystem::GetStarFuelMissiles(TArray<FSRSpaceLogisticsStarFuelMissile>& OutMissiles) const
{
	OutMissiles = StarFuelMissiles;
}

void USRSpaceLogisticsSubsystem::ExportSaveData(FSRSpaceLogisticsSaveData& OutSaveData) const
{
	FSRSpaceLogisticsSaveAdapter::ExportSaveData(
		*this,
		HubRoutes,
		NextHubRouteSequence,
		NextFleetDepartureQueueSequence,
		StarFuelMissiles,
		NextStarFuelMissileSequence,
		OutSaveData);
}

bool USRSpaceLogisticsSubsystem::ImportSaveData(const FSRSpaceLogisticsSaveData& SaveData)
{
	return FSRSpaceLogisticsSaveAdapter::ImportSaveData(
		*this,
		SaveData,
		HubRoutes,
		NextHubRouteSequence,
		NextFleetDepartureQueueSequence,
		SpaceshipActorsByRouteId,
		StarFuelMissiles,
		NextStarFuelMissileSequence,
		StarFuelMissileActorsByMissileId,
		HubEndpointMotionSamples);
}

void USRSpaceLogisticsSubsystem::RebuildHubEndpoints() const
{
	FSRSpaceLogisticsHubEndpointResolver::Rebuild(GetWorld(), CachedHubEndpoints);
}

bool USRSpaceLogisticsSubsystem::BuildHubEndpoint(AActor* BodyActor, FName HubOccupantId, FSRSpaceLogisticsHubEndpoint& OutHubEndpoint) const
{
	return FSRSpaceLogisticsHubEndpointResolver::Build(BodyActor, HubOccupantId, OutHubEndpoint);
}

bool USRSpaceLogisticsSubsystem::BuildHubEndpointSaveData(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	FSRSpaceLogisticsHubEndpointSaveData& OutSaveData) const
{
	return FSRSpaceLogisticsHubEndpointResolver::BuildSaveData(HubEndpoint, OutSaveData);
}

bool USRSpaceLogisticsSubsystem::ResolveSavedHubEndpoint(
	const FSRSpaceLogisticsHubEndpointSaveData& SaveData,
	FSRSpaceLogisticsHubEndpoint& OutHubEndpoint) const
{
	return FSRSpaceLogisticsHubEndpointResolver::ResolveSaved(
		GetWorld(),
		CachedHubEndpoints,
		SaveData,
		OutHubEndpoint);
}

bool USRSpaceLogisticsSubsystem::ResolveCurrentHubEndpoint(const FSRSpaceLogisticsHubEndpoint& CandidateHubEndpoint, FSRSpaceLogisticsHubEndpoint& OutHubEndpoint) const
{
	return FSRSpaceLogisticsHubEndpointResolver::ResolveCurrent(CandidateHubEndpoint, OutHubEndpoint);
}

float USRSpaceLogisticsSubsystem::ResolveSimulationDeltaSeconds(float DeltaTime) const
{
	const float ClampedDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (const UWorld* World = GetWorld())
	{
		if (const USRTimeControlSubsystem* TimeControlSubsystem = World->GetSubsystem<USRTimeControlSubsystem>())
		{
			return ClampedDeltaTime * FMath::Max(0.0f, TimeControlSubsystem->GetEffectiveTimeScale());
		}
	}

	return ClampedDeltaTime;
}

bool USRSpaceLogisticsSubsystem::ResolveHubEndpointWorldVelocity(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	FVector& OutWorldVelocity) const
{
	return FSRSpaceLogisticsHubEndpointMotionTracker::ResolveWorldVelocity(
		HubEndpoint,
		HubEndpointMotionSamples,
		OutWorldVelocity);
}

bool USRSpaceLogisticsSubsystem::IsFleetCapacityRulesActive() const
{
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	return IsValid(Settings)
		&& Settings->ResourceRulesetVersion == ESRResourceRulesetVersion::ResourceV2;
}

int32 USRSpaceLogisticsSubsystem::ResolveActiveFleetBerthCountForHub(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint) const
{
	AActor* BodyActor = HubEndpoint.BodyActor.Get();
	USRFacilityNetworkComponent* FacilityNetwork = IsValid(BodyActor)
		? BodyActor->FindComponentByClass<USRFacilityNetworkComponent>()
		: nullptr;
	if (!IsValid(FacilityNetwork) || HubEndpoint.HubOccupantId.IsNone())
	{
		return 0;
	}

	RebuildHubEndpoints();
	TArray<const FSRSpaceLogisticsHubEndpoint*> BodyHubs;
	for (const FSRSpaceLogisticsHubEndpoint& CandidateHub : CachedHubEndpoints)
	{
		if (CandidateHub.BodyActor == BodyActor && !CandidateHub.HubOccupantId.IsNone())
		{
			BodyHubs.Add(&CandidateHub);
		}
	}
	if (BodyHubs.IsEmpty())
	{
		return 0;
	}

	TArray<FName> FacilityOccupantIds;
	FacilityNetwork->GetRegisteredFacilityOccupantIds(FacilityOccupantIds);
	int32 AssignedBerthCount = 0;
	for (const FName FacilityOccupantId : FacilityOccupantIds)
	{
		FSRFacilityInstance Facility;
		if (!FacilityNetwork->GetFacilityInstance(FacilityOccupantId, Facility)
			|| !FSROperationalEconomyProcessor::IsFleetBerthSupplied(Facility))
		{
			continue;
		}

		const FSRSpaceLogisticsHubEndpoint* AssignedHub = nullptr;
		int64 BestDistance = MAX_int64;
		for (const FSRSpaceLogisticsHubEndpoint* CandidateHub : BodyHubs)
		{
			if (!CandidateHub)
			{
				continue;
			}
			const bool bSameFace = CandidateHub->OriginCellId.Face == Facility.OriginCellId.Face;
			const int64 GridDistance = FMath::Abs(
				static_cast<int64>(CandidateHub->OriginCellId.CellX) - Facility.OriginCellId.CellX)
				+ FMath::Abs(
					static_cast<int64>(CandidateHub->OriginCellId.CellY) - Facility.OriginCellId.CellY);
			const int64 Distance = GridDistance + (bSameFace ? 0 : (static_cast<int64>(1) << 40));
			if (!AssignedHub
				|| Distance < BestDistance
				|| (Distance == BestDistance
					&& CandidateHub->HubOccupantId.LexicalLess(AssignedHub->HubOccupantId)))
			{
				AssignedHub = CandidateHub;
				BestDistance = Distance;
			}
		}

		if (AssignedHub && AssignedHub->HubOccupantId == HubEndpoint.HubOccupantId)
		{
			++AssignedBerthCount;
		}
	}
	return AssignedBerthCount;
}
