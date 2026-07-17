#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

struct FSRFacilityHubCargoTransferResult
{
	FName PortId = NAME_None;
	int32 RemainingPortStackCount = 0;
};

class FSRFacilityHubCargoRouter
{
public:
	static bool IsHubFacility(const FSRFacilityInstance& FacilityInstance);

	static bool TryTakeOutboundCargo(
		FSRFacilityInstance& FacilityInstance,
		int32 MaxStackCount,
		FName ResourceId,
		FSRResourceInstance& OutCargo,
		FSRFacilityHubCargoTransferResult* OutTransferResult = nullptr);

	static bool TryTakeOutboundCargoMatching(
		FSRFacilityInstance& FacilityInstance,
		int32 MaxStackCount,
		TFunctionRef<bool(const FSRResourceInstance&)> CargoPredicate,
		FSRResourceInstance& OutCargo,
		FSRFacilityHubCargoTransferResult* OutTransferResult = nullptr);

	static bool TryTakeOutboundCargoMatchingFromInputPort(
		FSRFacilityInstance& FacilityInstance,
		int32 InputPortIndex,
		int32 MaxStackCount,
		TFunctionRef<bool(const FSRResourceInstance&)> CargoPredicate,
		FSRResourceInstance& OutCargo,
		FSRFacilityHubCargoTransferResult* OutTransferResult = nullptr);

	static void GetOutboundCargoResourceIds(
		const FSRFacilityInstance& FacilityInstance,
		TArray<FName>& OutResourceIds);

	static bool CanStoreInboundCargo(
		const FSRFacilityInstance& FacilityInstance,
		const FSRResourceInstance& Cargo);

	static bool TryStoreInboundCargo(
		FSRFacilityInstance& FacilityInstance,
		const FSRResourceInstance& Cargo);
};
