#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

class USRFacilityDataAsset;
struct FSRResourceProcessResult;
struct FSRStellarFuelFabricationResultV2;

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
		TArray<FString>* OutEnergyFormulaTexts = nullptr,
		FSRResourceProcessResult* OutResourceV2ProcessResult = nullptr,
		FSRStellarFuelFabricationResultV2* OutStellarFuelFabricationResult = nullptr,
		FName ProcessingBodyId = NAME_None);

	static void BuildOutputResourcesFromPrimaryResource(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		const FSRResourceInstance& PrimaryResource,
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount = nullptr,
		FSRResourceInstance* OutBaselinePrimaryResource = nullptr,
		TArray<FString>* OutEnergyFormulaTexts = nullptr,
		FSRResourceProcessResult* OutResourceV2ProcessResult = nullptr,
		FName ProcessingBodyId = NAME_None);
};
