#pragma once

#include "CoreMinimal.h"

struct FSRFacilityInstance;

class FSRFacilityPortInventoryBuilder
{
public:
	static void Initialize(FSRFacilityInstance& FacilityInstance);
	static void RefreshAggregateInventories(FSRFacilityInstance& FacilityInstance);
};
