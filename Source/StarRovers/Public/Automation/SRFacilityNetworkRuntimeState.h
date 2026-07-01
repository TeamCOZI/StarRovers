#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeTypes.h"
#include "SRFacilityNetworkRuntimeState.generated.h"

class USRTimeControlSubsystem;

USTRUCT()
struct STARROVERS_API FSRFacilityNetworkRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<USRTimeControlSubsystem> BoundTimeControlSubsystem;

	UPROPERTY(Transient)
	TMap<FName, FSRFacilityInstance> FacilityInstancesByOccupantId;
};
