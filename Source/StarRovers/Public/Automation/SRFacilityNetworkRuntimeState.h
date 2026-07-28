#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Automation/SROperationalCapacityTypes.h"
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

	UPROPERTY(Transient)
	FSROperationalCapacityReportV2 OperationalCapacityReport;

	// Expensive start/completion transitions are time-sliced in a stable order.
	// Active process clocks still advance every tick, so this cursor is a CPU
	// budget rather than a hidden production-throughput limit.
	UPROPERTY(Transient)
	TArray<FName> FacilitySchedulerOrder;

	UPROPERTY(Transient)
	FName NextFacilitySchedulerOccupantId = NAME_None;

	UPROPERTY(Transient)
	bool bFacilitySchedulerOrderDirty = true;
};
