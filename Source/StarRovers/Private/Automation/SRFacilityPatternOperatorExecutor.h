#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Pattern/SRPatternFacilityResolver.h"

class USRFacilityDataAsset;

class FSRFacilityPatternOperatorExecutor
{
public:
	static bool IsPatternOperation(const USRFacilityDataAsset* FacilityDataAsset);
	static int32 ResolveRequiredInputCount(const USRFacilityDataAsset* FacilityDataAsset);
	static int32 ResolveExpectedOutputCount(const USRFacilityDataAsset* FacilityDataAsset);

	static bool CanExecute(
		const USRFacilityDataAsset* FacilityDataAsset,
		const TArray<FSRResourceInstance>& InputResources);

	static bool TryBuildOutputResources(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		TArray<FSRResourceInstance>& OutOutputResources,
		FSRPatternFacilityResolveResult* OutResolveResult = nullptr);
};
