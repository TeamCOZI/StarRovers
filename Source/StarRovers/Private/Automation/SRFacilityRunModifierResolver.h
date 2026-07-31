#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

class FSRFacilityRunModifierResolver final
{
public:
	static void SnapshotCurrentContext(const UObject* WorldContextObject, FSRFacilityInstance& FacilityInstance);

	static FSRRunModifierQuery BuildQuery(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources);

	static FSRResolvedRunModifiers Resolve(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources);

	static FSRPatternEnvironmentSpec ResolveEnvironmentSpec(
		const FSRPatternEnvironmentSpec& BaseEnvironment,
		int32 IntensityDelta);
};
