#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"

// Resource V2's Hub-local logistics budget. This class owns only deterministic
// profile, reservation, and queue policy; the subsystem resolves supplied
// Fleet Berths from the body that owns a Hub.
class STARROVERS_API FSRFleetCapacityV2 final
{
public:
	static void GetRouteProfiles(TArray<ESRSpaceLogisticsRouteProfileV2>& OutProfiles);
	static FSRSpaceLogisticsRouteProfileRulesV2 GetRouteProfileRules(
		ESRSpaceLogisticsRouteProfileV2 Profile);
	static FName GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2 Profile);
	static bool TryResolveRouteProfileId(
		FName ProfileId,
		ESRSpaceLogisticsRouteProfileV2& OutProfile);
	static bool IsTechnologyRouteProfile(ESRSpaceLogisticsRouteProfileV2 Profile);
	static ESRSpaceLogisticsRouteProfileV2 GetNextRouteProfile(
		ESRSpaceLogisticsRouteProfileV2 Profile);
	static int32 ResolveEffectiveCargoCapacity(const FSRSpaceLogisticsHubRoute& Route);
	static int32 ResolveFleetLoad(const FSRSpaceLogisticsHubRoute& Route);
	static double ResolveMaximumCargoPerFleetLoad(ESRSpaceLogisticsRouteProfileV2 Profile);
	static bool IsUntouchedCard(const FSRResourceInstance& Cargo);
	static bool IsCargoEligible(
		ESRSpaceLogisticsRouteProfileV2 Profile,
		const FSRResourceInstance& Cargo);

	static FSRFleetCapacityReportV2 BuildReport(
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		const TArray<FSRSpaceLogisticsHubRoute>& Routes,
		bool bRulesActive,
		int32 ActiveFleetBerthCount,
		int32 BaseCapacity = 8,
		int32 CapacityPerFleetBerth = 8);

	static bool CanGrantDeparture(
		const FSRSpaceLogisticsHubRoute& Route,
		const FSRSpaceLogisticsHubEndpoint& DockHub,
		const TArray<FSRSpaceLogisticsHubRoute>& Routes,
		const FSRFleetCapacityReportV2& Report);

	static void RefreshQueuePositions(TArray<FSRSpaceLogisticsHubRoute>& Routes);

private:
	static bool AreEndpointKeysEqual(
		const FSRSpaceLogisticsHubEndpoint& Left,
		const FSRSpaceLogisticsHubEndpoint& Right);
	static bool TryGetReservedDepartureHub(
		const FSRSpaceLogisticsHubRoute& Route,
		FSRSpaceLogisticsHubEndpoint& OutHubEndpoint);
	static bool TryGetQueuedDepartureHub(
		const FSRSpaceLogisticsHubRoute& Route,
		FSRSpaceLogisticsHubEndpoint& OutHubEndpoint);
};
