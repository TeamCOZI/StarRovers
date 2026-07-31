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
	static bool AllowsEmptyOutput(const FSRFacilityInstance& FacilityInstance);

	static void BuildOutputResources(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount = nullptr,
		FSRResourceInstance* OutBaselinePrimaryResource = nullptr,
		TArray<FString>* OutOperationTraceTexts = nullptr);

	static void BuildOutputResourcesFromPrimaryResource(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		const FSRResourceInstance& PrimaryResource,
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount = nullptr,
		FSRResourceInstance* OutBaselinePrimaryResource = nullptr,
		TArray<FString>* OutOperationTraceTexts = nullptr);
};
