#include "SRFacilityInputAcceptancePolicy.h"

#include "Automation/SRFacilityRuntimeData.h"
#include "Automation/SRStellarFuelFabricator.h"

bool FSRFacilityInputAcceptancePolicy::CanAcceptResource(
	const FSRFacilityInstance& FacilityInstance,
	const FSRResourceInstance& ResourceInstance,
	FString* OutFailureReason)
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		return true;
	}

	FString FailureReason;
	const bool bAccepted = FSRStellarFuelFabricator::ValidateInputCard(
		ResourceInstance,
		FailureReason);
	if (!bAccepted && OutFailureReason)
	{
		*OutFailureReason = MoveTemp(FailureReason);
	}
	return bAccepted;
}
