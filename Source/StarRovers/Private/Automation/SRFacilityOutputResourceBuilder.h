#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

class USRFacilityDataAsset;

class FSRFacilityOutputResourceBuilder
{
public:
	static bool DoesInputSetMatchOperation(
		const USRFacilityDataAsset* FacilityDataAsset,
		const TArray<FSRResourceInstance>& InputResources,
		ESRFacilityTemperatureState TemperatureState);

	static int32 CountProducedOutputResources(const USRFacilityDataAsset* FacilityDataAsset);
	static int32 ResolvePrimaryOutputCount(const FSRFacilityInstance& FacilityInstance);
	static int32 ResolveRequiredOutputSlots(const FSRFacilityInstance& FacilityInstance);

	static void BuildOutputResources(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		TArray<FSRResourceInstance>& OutOutputResources);

private:
	static FSRResourceInstance BuildBaseOutputResource(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& ConsumedResources);

	static void ApplyFacilityEffects(
		const USRFacilityDataAsset* FacilityDataAsset,
		FSRResourceInstance& ResourceInstance,
		TArray<FSRResourceInstance>& OutAdditionalOutputs);

	static void AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count);
	static void RemoveTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count);
};
