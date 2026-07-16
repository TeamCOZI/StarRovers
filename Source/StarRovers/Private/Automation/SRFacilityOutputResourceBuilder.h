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
		TArray<FString>* OutEnergyFormulaTexts = nullptr);

	static void BuildOutputResourcesFromPrimaryResource(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		const FSRResourceInstance& PrimaryResource,
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount = nullptr,
		FSRResourceInstance* OutBaselinePrimaryResource = nullptr,
		TArray<FString>* OutEnergyFormulaTexts = nullptr,
		int32 InitialEnergyChangeCount = 0,
		double InitialTagEffectEnergyChangeAmount = 0.0,
		const FSRResourceInstance* ConditionBaselineResource = nullptr,
		int32 CompletedProcessCountIncrement = 0,
		const FString* PrimaryEnergyFormulaText = nullptr);

private:
	static void BuildProcessedResourcesBeforeFacilityEffects(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& ConsumedResources,
		TArray<FSRResourceInstance>& OutProcessedResources,
		int32& OutEnergyChangeCount,
		TArray<double>& OutTagEffectEnergyChangeAmounts,
		TArray<FSRResourceInstance>* OutResourcesBeforeTagEffects = nullptr,
		TArray<FString>* OutEnergyFormulaTexts = nullptr);

	static FSRResourceInstance BuildBaseOutputResource(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& ConsumedResources,
		int32& OutEnergyChangeCount,
		double& OutTagEffectEnergyChangeAmount,
		FSRResourceInstance* OutConditionBaselineResource = nullptr,
		FString* OutEnergyFormulaText = nullptr);

	static void ApplyFacilityEffects(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		FSRResourceInstance& ResourceInstance,
		bool& bHasPrimaryResource,
		TArray<FSRResourceInstance>& OutAdditionalOutputs,
		int32 InitialEnergyChangeCount,
		double InitialTagEffectEnergyChangeAmount,
		bool bApplyResourceEffects = true,
		bool bApplyAdditionalOutputEffects = true,
		const FSRResourceInstance* ConditionBaselineResource = nullptr,
		FString* EnergyFormulaText = nullptr,
		TArray<FString>* OutAdditionalOutputEnergyFormulaTexts = nullptr);

	static void AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count);
};
