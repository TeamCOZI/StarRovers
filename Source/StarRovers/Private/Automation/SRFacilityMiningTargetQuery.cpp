#include "SRFacilityMiningTargetQuery.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "SRFacilityMiningTargetResolver.h"

bool FSRFacilityMiningTargetQuery::GetMiningTarget(
	const UActorComponent* OwnerComponent,
	const FSRFacilityNetworkRuntimeState& RuntimeState,
	FName OccupantId,
	FSRResourceDepositInstance& OutResourceDeposit)
{
	OutResourceDeposit = FSRResourceDepositInstance();
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance
		|| !IsValid(FacilityInstance->FacilityDataAsset.Get())
		|| FacilityInstance->FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine)
	{
		return false;
	}

	return FSRFacilityMiningTargetResolver::FindTargetDeposit(OwnerComponent, *FacilityInstance, OutResourceDeposit);
}
