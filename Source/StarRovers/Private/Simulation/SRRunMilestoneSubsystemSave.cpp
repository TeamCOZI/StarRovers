#include "Simulation/SRRunMilestoneSubsystem.h"

#include "Simulation/SRCelestialBodyRegistrySubsystem.h"

void USRRunMilestoneSubsystem::ExportSaveData(
	FSRRunMilestoneSaveData& OutSaveData) const
{
	OutSaveData = FSRRunMilestoneSaveData();
	OutSaveData.Facts = ObservedFacts;
	OutSaveData.FirstResourceFamily = FirstResourceFamily;
	OutSaveData.bEmergencyRecoveryAttempted =
		InitialProgressRecovery.bAttempted;
	OutSaveData.bEmergencyRecoveryApplied = InitialProgressRecovery.bApplied;
	OutSaveData.EmergencyRecoveryBodyId =
		IsValid(InitialProgressRecovery.BodyActor.Get())
			? InitialProgressRecovery.BodyActor->GetFName()
			: NAME_None;
	OutSaveData.EmergencyRecoveryDepositOccupantId =
		InitialProgressRecovery.DepositOccupantId;
	OutSaveData.EmergencyRecoveryResourceId =
		InitialProgressRecovery.ResourceId;
	OutSaveData.EmergencyRecoveryGrantedCardAmount =
		InitialProgressRecovery.GrantedCardAmount;
}

bool USRRunMilestoneSubsystem::ImportSaveData(
	const FSRRunMilestoneSaveData& SaveData,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	const UEnum* FamilyEnum = StaticEnum<ESRResourceFamily>();
	if (!SaveData.IsSupportedVersion())
	{
		OutFailureReason = FString::Printf(
			TEXT("Unsupported Run Milestone save version %d."),
			SaveData.Version);
		return false;
	}
	if (!FamilyEnum
		|| !FamilyEnum->IsValidEnumValue(
			static_cast<int64>(SaveData.FirstResourceFamily))
		|| (SaveData.bEmergencyRecoveryApplied
			&& (!SaveData.bEmergencyRecoveryAttempted
				|| SaveData.EmergencyRecoveryBodyId.IsNone()
				|| SaveData.EmergencyRecoveryDepositOccupantId.IsNone()
				|| SaveData.EmergencyRecoveryResourceId.IsNone()
				|| SaveData.EmergencyRecoveryGrantedCardAmount <= 0)))
	{
		OutFailureReason = TEXT("Run Milestone save contains an invalid recovery state.");
		return false;
	}

	AActor* RecoveryBody = nullptr;
	if (!SaveData.EmergencyRecoveryBodyId.IsNone())
	{
		USRCelestialBodyRegistrySubsystem* Registry = GetWorld()
			? GetWorld()->GetSubsystem<USRCelestialBodyRegistrySubsystem>()
			: nullptr;
		TArray<AActor*> Bodies;
		if (IsValid(Registry))
		{
			Registry->GetCelestialBodies(Bodies);
		}
		for (AActor* Body : Bodies)
		{
			if (IsValid(Body)
				&& Body->GetFName() == SaveData.EmergencyRecoveryBodyId)
			{
				RecoveryBody = Body;
				break;
			}
		}
		if (SaveData.bEmergencyRecoveryApplied && !IsValid(RecoveryBody))
		{
			OutFailureReason = FString::Printf(
				TEXT("Run Milestone save cannot resolve recovery body %s."),
				*SaveData.EmergencyRecoveryBodyId.ToString());
			return false;
		}
	}

	ResetFirstFuelMilestone();
	ObservedFacts = SaveData.Facts;
	FSRFirstFuelMilestoneModel::ApplyConsistency(ObservedFacts);
	FirstResourceFamily = SaveData.FirstResourceFamily;
	InitialProgressRecovery.bAttempted =
		SaveData.bEmergencyRecoveryAttempted;
	InitialProgressRecovery.bApplied = SaveData.bEmergencyRecoveryApplied;
	InitialProgressRecovery.BodyActor = RecoveryBody;
	InitialProgressRecovery.DepositOccupantId =
		SaveData.EmergencyRecoveryDepositOccupantId;
	InitialProgressRecovery.ResourceId = SaveData.EmergencyRecoveryResourceId;
	InitialProgressRecovery.GrantedCardAmount =
		SaveData.EmergencyRecoveryGrantedCardAmount;
	RefreshFromWorld();
	return true;
}
