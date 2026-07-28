#include "UI/SRCelestialBodyOperationsSummary.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Structure/SRStructureInstanceManagerComponent.h"

namespace
{
	bool IsHubOnBody(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, const AActor* BodyActor)
	{
		return HubEndpoint.IsValid() && HubEndpoint.BodyActor.Get() == BodyActor;
	}

	bool IsRouteConnectedToBody(const FSRSpaceLogisticsHubRoute& Route, const AActor* BodyActor)
	{
		return Route.SourceHub.BodyActor.Get() == BodyActor
			|| Route.DestinationHub.BodyActor.Get() == BodyActor;
	}

	float GetFleetUtilization(const FSRFleetCapacityReportV2& Report)
	{
		if (Report.TotalCapacity <= 0)
		{
			return Report.ReservedLoad > 0 ? BIG_NUMBER : 0.0f;
		}
		return static_cast<float>(Report.ReservedLoad)
			/ static_cast<float>(Report.TotalCapacity);
	}
}

bool FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalSummary(
	const AActor* CelestialBodyActor,
	FSRCelestialBodyOperationsSummary& OutSummary)
{
	OutSummary = FSRCelestialBodyOperationsSummary();
	if (!IsValid(CelestialBodyActor))
	{
		return false;
	}

	const USRFacilityNetworkComponent* FacilityNetwork =
		CelestialBodyActor->FindComponentByClass<USRFacilityNetworkComponent>();
	if (!IsValid(FacilityNetwork))
	{
		return false;
	}

	OutSummary.bIsValid = true;
	OutSummary.OperationalCapacity = FacilityNetwork->GetOperationalCapacityReport();
	OutSummary.bFleetCapacityRulesActive = OutSummary.OperationalCapacity.bRulesActive;

	const FSROperationalFacilityStatusCountsV2 FacilityCounts =
		FacilityNetwork->GetOperationalFacilityStatusCounts();
	OutSummary.FacilityCount = FacilityCounts.RegisteredFacilityCount;
	OutSummary.EnabledFacilityCount = FacilityCounts.EnabledFacilityCount;
	OutSummary.ProcessingFacilityCount = FacilityCounts.ProcessingFacilityCount;
	OutSummary.ThrottledFacilityCount = FacilityCounts.ThrottledFacilityCount;

	if (const USRStructureInstanceManagerComponent* StructureManager =
		CelestialBodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
	{
		TArray<FSRResourceDepositInstance> Deposits;
		StructureManager->GetResourceDepositInstances(Deposits);
		OutSummary.ResourceReserve = FSRResourceReserveModel::BuildSnapshot(Deposits);
	}

	return true;
}

bool FSRCelestialBodyOperationsSummaryBuilder::BuildSummary(
	const AActor* CelestialBodyActor,
	FSRCelestialBodyOperationsSummary& OutSummary)
{
	if (!BuildOperationalSummary(CelestialBodyActor, OutSummary))
	{
		return false;
	}

	const UWorld* World = CelestialBodyActor->GetWorld();
	const USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = IsValid(World)
		? World->GetSubsystem<USRSpaceLogisticsSubsystem>()
		: nullptr;
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		return true;
	}

	TArray<FSRSpaceLogisticsHubEndpoint> HubEndpoints;
	SpaceLogisticsSubsystem->GetHubEndpoints(HubEndpoints);
	float BusiestHubUtilization = -1.0f;
	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
	{
		if (!IsHubOnBody(HubEndpoint, CelestialBodyActor))
		{
			continue;
		}

		++OutSummary.HubCount;
		const FSRFleetCapacityReportV2 FleetReport =
			SpaceLogisticsSubsystem->GetHubFleetCapacityReport(HubEndpoint);
		OutSummary.bFleetCapacityRulesActive = FleetReport.bRulesActive;
		OutSummary.FleetReservedLoad += FleetReport.ReservedLoad;
		OutSummary.FleetTotalCapacity += FleetReport.TotalCapacity;
		OutSummary.FleetAvailableCapacity += FleetReport.AvailableCapacity;
		OutSummary.FleetQueuedDepartureCount += FleetReport.QueuedDepartureCount;
		OutSummary.ActiveFleetBerthCount += FleetReport.ActiveFleetBerthCount;

		const float HubUtilization = GetFleetUtilization(FleetReport);
		if (HubUtilization > BusiestHubUtilization)
		{
			BusiestHubUtilization = HubUtilization;
			OutSummary.BusiestHubReservedLoad = FleetReport.ReservedLoad;
			OutSummary.BusiestHubTotalCapacity = FleetReport.TotalCapacity;
		}
	}

	TArray<FSRSpaceLogisticsHubRoute> HubRoutes;
	SpaceLogisticsSubsystem->GetHubRoutes(HubRoutes);
	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (HubRoute.bDebugLocalOrbit
			|| !IsRouteConnectedToBody(HubRoute, CelestialBodyActor))
		{
			continue;
		}
		++OutSummary.ConnectedRouteCount;
		OutSummary.BlockedRouteCount +=
			HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::Blocked ? 1 : 0;
	}

	TArray<FSRSpaceLogisticsStarFuelMissile> StarFuelMissiles;
	SpaceLogisticsSubsystem->GetStarFuelMissiles(StarFuelMissiles);
	for (const FSRSpaceLogisticsStarFuelMissile& Missile : StarFuelMissiles)
	{
		if (Missile.bEnabled && Missile.SourceHub.BodyActor.Get() == CelestialBodyActor)
		{
			++OutSummary.ActiveStarFuelMissileCount;
		}
	}

	return true;
}

float FSRCelestialBodyOperationsSummaryBuilder::GetOperationalUtilization(
	const FSRCelestialBodyOperationsSummary& Summary)
{
	if (!Summary.bIsValid)
	{
		return 0.0f;
	}
	if (Summary.OperationalCapacity.TotalCapacity <= 0)
	{
		return Summary.OperationalCapacity.TotalDemand > 0 ? BIG_NUMBER : 0.0f;
	}
	return static_cast<float>(Summary.OperationalCapacity.TotalDemand)
		/ static_cast<float>(Summary.OperationalCapacity.TotalCapacity);
}

ESRCelestialBodyOperationsPressure
FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(
	const FSRCelestialBodyOperationsSummary& Summary)
{
	if (!Summary.bIsValid || Summary.OperationalCapacity.TotalDemand <= 0)
	{
		return ESRCelestialBodyOperationsPressure::Idle;
	}

	const float Utilization = GetOperationalUtilization(Summary);
	if (Utilization > 1.0f + KINDA_SMALL_NUMBER)
	{
		return ESRCelestialBodyOperationsPressure::OverCapacity;
	}
	if (Utilization >= 1.0f - KINDA_SMALL_NUMBER)
	{
		return ESRCelestialBodyOperationsPressure::AtCapacity;
	}
	if (Utilization >= 0.8f)
	{
		return ESRCelestialBodyOperationsPressure::NearCapacity;
	}
	return ESRCelestialBodyOperationsPressure::Nominal;
}

FString FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalBadgeText(
	const FSRCelestialBodyOperationsSummary& Summary)
{
	if (!Summary.bIsValid)
	{
		return FString();
	}
	const FString ReserveSuffix = Summary.ResourceReserve.bHasDeposits
		? FString::Printf(
			TEXT(" | R %d%%"),
			FMath::RoundToInt(Summary.ResourceReserve.RemainingRatio * 100.0f))
		: FString();
	return FString::Printf(
		TEXT("L %d/%d%s"),
		Summary.OperationalCapacity.TotalDemand,
		Summary.OperationalCapacity.TotalCapacity,
		*ReserveSuffix);
}

FString FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalStatusText(
	const FSRCelestialBodyOperationsSummary& Summary)
{
	if (!Summary.bIsValid)
	{
		return TEXT("Operational data unavailable");
	}

	const int32 Demand = Summary.OperationalCapacity.TotalDemand;
	const int32 Capacity = Summary.OperationalCapacity.TotalCapacity;
	const int32 Headroom = FMath::Max(0, Capacity - Demand);
	const int32 Excess = FMath::Max(0, Demand - Capacity);
	switch (ResolveOperationalPressure(Summary))
	{
	case ESRCelestialBodyOperationsPressure::Idle:
		return FString::Printf(TEXT("Idle - %d capacity available"), Headroom);
	case ESRCelestialBodyOperationsPressure::Nominal:
		return FString::Printf(TEXT("Nominal - %d capacity available"), Headroom);
	case ESRCelestialBodyOperationsPressure::NearCapacity:
		return FString::Printf(TEXT("Near capacity - %d capacity available"), Headroom);
	case ESRCelestialBodyOperationsPressure::AtCapacity:
		return TEXT("At capacity - no headroom");
	case ESRCelestialBodyOperationsPressure::OverCapacity:
		return FString::Printf(TEXT("Over capacity - %d load unsupported"), Excess);
	default:
		return TEXT("Operational data unavailable");
	}
}

FString FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalToolTipText(
	const FSRCelestialBodyOperationsSummary& Summary)
{
	if (!Summary.bIsValid)
	{
		return TEXT("No facility network is available on this celestial body.");
	}

	const FSROperationalCapacityReportV2& Capacity = Summary.OperationalCapacity;
	FString Result = FString::Printf(
		TEXT("Operational Load counts facilities that are processing right now; installed facilities that are idle or waiting for input consume no Capacity.\n")
		TEXT("Load %d / Capacity %d = Base %d + supplied Service Cores %d (+%d) + Augments %d.\n")
		TEXT("Facilities %d, enabled %d, processing %d, throttled %d."),
		Capacity.TotalDemand,
		Capacity.TotalCapacity,
		Capacity.BaseCapacity,
		Capacity.ActiveServiceCoreCount,
		Capacity.ServiceCoreCapacity,
		Capacity.AugmentCapacity,
		Summary.FacilityCount,
		Summary.EnabledFacilityCount,
		Summary.ProcessingFacilityCount,
		Summary.ThrottledFacilityCount);
	if (Summary.ResourceReserve.bHasDeposits)
	{
		Result += FString::Printf(
			TEXT("\nLocal reserves: %lld / %lld finite units (%d%%), active veins %d / %d, depleted %d. Card %lld, raw utility %lld."),
			Summary.ResourceReserve.RemainingFiniteAmount,
			Summary.ResourceReserve.TotalFiniteAmount,
			FMath::RoundToInt(Summary.ResourceReserve.RemainingRatio * 100.0f),
			Summary.ResourceReserve.ActiveDepositCount,
			Summary.ResourceReserve.DepositCount,
			Summary.ResourceReserve.DepletedDepositCount,
			Summary.ResourceReserve.RemainingCardAmount,
			Summary.ResourceReserve.RemainingUtilityAmount);
	}
	return Result;
}

FString FSRCelestialBodyOperationsSummaryBuilder::BuildResourceReserveText(
	const FSRCelestialBodyOperationsSummary& Summary)
{
	const FSRResourceReserveSnapshot& Reserve = Summary.ResourceReserve;
	if (!Summary.bIsValid || !Reserve.bHasDeposits)
	{
		return TEXT("RESERVES: no mineable deposits");
	}
	return FString::Printf(
		TEXT("RESERVES %d%% | VEINS %d/%d | CARD %lld | RAW %lld"),
		FMath::RoundToInt(Reserve.RemainingRatio * 100.0f),
		Reserve.ActiveDepositCount,
		Reserve.DepositCount,
		Reserve.RemainingCardAmount,
		Reserve.RemainingUtilityAmount);
}

FString FSRCelestialBodyOperationsSummaryBuilder::BuildResourceReserveStatusText(
	const FSRCelestialBodyOperationsSummary& Summary)
{
	const FSRResourceReserveSnapshot& Reserve = Summary.ResourceReserve;
	if (!Summary.bIsValid || !Reserve.bHasDeposits)
	{
		return TEXT("No local reserve data");
	}
	const FString PressureLabel = FSRResourceReserveModel::BuildPressureLabel(Reserve.Pressure);
	switch (Reserve.Pressure)
	{
	case ESRResourceReservePressure::Low:
		return FString::Printf(TEXT("%s RESERVES - prepare the next Miner"), *PressureLabel);
	case ESRResourceReservePressure::Critical:
		return FString::Printf(TEXT("%s RESERVES - reroute immediately"), *PressureLabel);
	case ESRResourceReservePressure::Depleted:
		return FString::Printf(TEXT("%s RESERVES - this body cannot sustain its Line"), *PressureLabel);
	case ESRResourceReservePressure::Healthy:
	default:
		return FString::Printf(TEXT("%s RESERVES"), *PressureLabel);
	}
}
