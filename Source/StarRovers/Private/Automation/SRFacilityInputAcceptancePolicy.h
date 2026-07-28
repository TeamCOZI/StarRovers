#pragma once

#include "CoreMinimal.h"

struct FSRFacilityInstance;
struct FSRResourceInstance;

class FSRFacilityInputAcceptancePolicy final
{
public:
	// Pure admission gate. Rejection happens before either endpoint inventory is
	// mutated, so conveyor and direct transfers retain ownership of the resource.
	static bool CanAcceptResource(
		const FSRFacilityInstance& FacilityInstance,
		const FSRResourceInstance& ResourceInstance,
		FString* OutFailureReason = nullptr);
};
