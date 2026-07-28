#include "Automation/SRFacilityNetworkComponent.h"

#include "SRFacilityCellTemperatureEffectApplier.h"
#include "SRFacilityNetworkSaveAdapter.h"
#include "Utility/SRLog.h"

void USRFacilityNetworkComponent::ExportSaveData(FSRFacilityNetworkSaveData& OutSaveData) const
{
	FSRFacilityNetworkSaveAdapter::ExportSaveData(RuntimeState, OutSaveData);
}

bool USRFacilityNetworkComponent::ImportSaveData(const FSRFacilityNetworkSaveData& SaveData)
{
	FSRFacilityNetworkRuntimeState ImportedState;
	FString FailureReason;
	if (!FSRFacilityNetworkSaveAdapter::ImportSaveData(SaveData, ImportedState, &FailureReason))
	{
		if (bLogFacilityNetworkEvents)
		{
			SR_LOG(FacilityNetwork,
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] Save import rejected: Owner=%s Reason=%s"),
				*GetNameSafe(GetOwner()),
				*FailureReason);
		}
		return false;
	}

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityCellTemperatureEffectApplier::RemoveInstallationEffects(this, FacilityPair.Value);
	}
	const TWeakObjectPtr<USRTimeControlSubsystem> BoundTimeControlSubsystem =
		RuntimeState.BoundTimeControlSubsystem;
	RuntimeState = MoveTemp(ImportedState);
	RuntimeState.BoundTimeControlSubsystem = BoundTimeControlSubsystem;

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityCellTemperatureEffectApplier::ApplyInstallationEffects(this, FacilityPair.Value);
	}
	RefreshFacilityTemperaturesFromSurface();
	RefreshOperationalCapacity();
	SetComponentTickEnabled(
		bAutoProcessFacilities && !RuntimeState.FacilityInstancesByOccupantId.IsEmpty());

	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Save imported: Version=%d Facilities=%d NextScheduler=%s Owner=%s"),
			SaveData.Version,
			RuntimeState.FacilityInstancesByOccupantId.Num(),
			*RuntimeState.NextFacilitySchedulerOccupantId.ToString(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}
