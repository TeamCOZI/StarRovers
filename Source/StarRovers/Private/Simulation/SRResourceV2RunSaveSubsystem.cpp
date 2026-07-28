#include "Simulation/SRResourceV2RunSaveSubsystem.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRRunTelemetrySubsystem.h"
#include "Simulation/SRSolarSystemGenerator.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

DEFINE_LOG_CATEGORY_STATIC(LogSRRunSave, Log, All);

namespace
{
	bool IsGenerationInProgress(const UWorld& World)
	{
		for (TActorIterator<ASRSolarSystemGenerator> It(&World); It; ++It)
		{
			if (It->IsRuntimeSystemGenerationInProgress())
			{
				return true;
			}
		}
		return false;
	}

	bool BuildBodyMap(
		USRCelestialBodyRegistrySubsystem& Registry,
		TMap<FName, TObjectPtr<AActor>>& OutBodies,
		FString& OutFailureReason)
	{
		OutBodies.Reset();
		Registry.RefreshCelestialBodies();
		TArray<AActor*> Bodies;
		Registry.GetCelestialBodies(Bodies);
		for (AActor* Body : Bodies)
		{
			// Runtime display names can repeat when the same environment appears
			// twice. Actor FNames are unique inside the generated World and stable
			// for the same deterministic spawn topology.
			const FName BodyId = IsValid(Body) ? Body->GetFName() : NAME_None;
			if (!IsValid(Body) || BodyId.IsNone() || OutBodies.Contains(BodyId))
			{
				OutFailureReason = FString::Printf(
					TEXT("Run save requires unique celestial Body ids; duplicate or missing id: %s."),
					*BodyId.ToString());
				return false;
			}
			OutBodies.Add(BodyId, Body);
		}
		if (OutBodies.IsEmpty())
		{
			OutFailureReason = TEXT("Run save found no registered celestial bodies.");
			return false;
		}
		return true;
	}
}

bool FSRResourceV2RunSaveCodec::Encode(
	const FSRResourceV2RunSaveData& SaveData,
	TArray<uint8>& OutPayload,
	uint32& OutChecksum,
	FString& OutFailureReason)
{
	OutPayload.Reset();
	OutChecksum = 0;
	OutFailureReason.Reset();
	if (!SaveData.IsSupportedVersion())
	{
		OutFailureReason = TEXT("Cannot encode an unsupported Run save version.");
		return false;
	}

	FMemoryWriter MemoryWriter(OutPayload, true);
	FObjectAndNameAsStringProxyArchive WriterArchive(MemoryWriter, false);
	FSRResourceV2RunSaveData Copy = SaveData;
	FSRResourceV2RunSaveData::StaticStruct()->SerializeItem(
		WriterArchive,
		&Copy,
		nullptr);
	const bool bArchiveError = WriterArchive.IsError();
	WriterArchive.Close();
	if (bArchiveError || OutPayload.IsEmpty())
	{
		OutPayload.Reset();
		OutFailureReason = TEXT("Run save payload serialization failed.");
		return false;
	}
	OutChecksum = FCrc::MemCrc32(OutPayload.GetData(), OutPayload.Num());
	return true;
}

bool FSRResourceV2RunSaveCodec::Decode(
	TConstArrayView<uint8> Payload,
	uint32 ExpectedChecksum,
	FSRResourceV2RunSaveData& OutSaveData,
	FString& OutFailureReason)
{
	OutSaveData = FSRResourceV2RunSaveData();
	OutFailureReason.Reset();
	if (Payload.IsEmpty())
	{
		OutFailureReason = TEXT("Run save payload is empty.");
		return false;
	}
	const uint32 ActualChecksum = FCrc::MemCrc32(Payload.GetData(), Payload.Num());
	if (ActualChecksum != ExpectedChecksum)
	{
		OutFailureReason = TEXT("Run save payload checksum mismatch.");
		return false;
	}

	TArray<uint8> PayloadCopy;
	PayloadCopy.Append(Payload.GetData(), Payload.Num());
	FMemoryReader MemoryReader(PayloadCopy, true);
	FObjectAndNameAsStringProxyArchive ReaderArchive(MemoryReader, true);
	FSRResourceV2RunSaveData::StaticStruct()->SerializeItem(
		ReaderArchive,
		&OutSaveData,
		nullptr);
	const bool bArchiveError = ReaderArchive.IsError();
	ReaderArchive.Close();
	if (bArchiveError || !OutSaveData.IsSupportedVersion())
	{
		OutSaveData = FSRResourceV2RunSaveData();
		OutFailureReason = TEXT("Run save payload is corrupt or uses an unsupported schema.");
		return false;
	}
	return true;
}

bool USRResourceV2RunSaveSubsystem::CaptureRunState(
	FSRResourceV2RunSaveData& OutSaveData,
	FString& OutFailureReason) const
{
	OutSaveData = FSRResourceV2RunSaveData();
	OutFailureReason.Reset();
	UWorld* World = GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		OutFailureReason = TEXT("Run state can only be captured from a game or PIE World.");
		return false;
	}
	if (IsGenerationInProgress(*World))
	{
		OutFailureReason = TEXT("Run state cannot be captured while solar-system generation is in progress.");
		return false;
	}

	USRCelestialBodyRegistrySubsystem* Registry =
		World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	USRSpaceLogisticsSubsystem* Logistics =
		World->GetSubsystem<USRSpaceLogisticsSubsystem>();
	USRAugmentSubsystem* Augments = World->GetSubsystem<USRAugmentSubsystem>();
	USRTimeControlSubsystem* TimeControl =
		World->GetSubsystem<USRTimeControlSubsystem>();
	USRRunMilestoneSubsystem* Milestones =
		World->GetSubsystem<USRRunMilestoneSubsystem>();
	if (!IsValid(Registry)
		|| !IsValid(Logistics)
		|| !IsValid(Augments)
		|| !IsValid(TimeControl)
		|| !IsValid(Milestones))
	{
		OutFailureReason = TEXT("Run save could not resolve every authoritative World subsystem.");
		return false;
	}

	TMap<FName, TObjectPtr<AActor>> Bodies;
	if (!BuildBodyMap(*Registry, Bodies, OutFailureReason))
	{
		return false;
	}
	ASRStar* PrimaryStar = Cast<ASRStar>(Registry->GetPrimaryStarActor());
	if (!IsValid(PrimaryStar))
	{
		OutFailureReason = TEXT("Run save requires a registered ASRStar primary Star.");
		return false;
	}

	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	OutSaveData.SourceRulesetVersion = IsValid(Settings)
		? Settings->ResourceRulesetVersion
		: ESRResourceRulesetVersion::ResourceV2;
	OutSaveData.PrimaryStarBodyId = PrimaryStar->GetFName();

	TArray<FName> BodyIds;
	Bodies.GenerateKeyArray(BodyIds);
	BodyIds.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});
	OutSaveData.Bodies.Reserve(BodyIds.Num());
	for (const FName BodyId : BodyIds)
	{
		AActor* Body = Bodies.FindRef(BodyId);
		FSRResourceV2BodySaveData& SavedBody =
			OutSaveData.Bodies.AddDefaulted_GetRef();
		SavedBody.BodyId = BodyId;
		SavedBody.ActorName = Body->GetFName();
		if (USRStructureInstanceManagerComponent* StructureManager =
			Body->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			SavedBody.bHasStructureManager = true;
			StructureManager->ExportSaveData(SavedBody.StructureManager);
		}
		if (USRFacilityNetworkComponent* FacilityNetwork =
			Body->FindComponentByClass<USRFacilityNetworkComponent>())
		{
			SavedBody.bHasFacilityNetwork = true;
			FacilityNetwork->ExportSaveData(SavedBody.FacilityNetwork);
		}
	}

	Logistics->ExportSaveData(OutSaveData.SpaceLogistics);
	Augments->ExportSaveData(OutSaveData.Augments);
	TimeControl->ExportSaveData(OutSaveData.TimeControl);
	Milestones->ExportSaveData(OutSaveData.RunMilestones);
	PrimaryStar->ExportRuntimeSaveData(OutSaveData.PrimaryStar);
	return true;
}

bool USRResourceV2RunSaveSubsystem::RestoreRunState(
	const FSRResourceV2RunSaveData& SaveData,
	FSRResourceV2RunRestoreReport& OutReport)
{
	OutReport = FSRResourceV2RunRestoreReport();
	FSRResourceV2RunSaveData Backup;
	FString BackupFailureReason;
	if (!CaptureRunState(Backup, BackupFailureReason))
	{
		OutReport.FailureReason = FString::Printf(
			TEXT("Run restore could not capture its rollback checkpoint: %s"),
			*BackupFailureReason);
		return false;
	}

	if (RestoreRunStateInternal(SaveData, OutReport))
	{
		OutReport.bSucceeded = true;
		return true;
	}

	const FString OriginalFailureReason = OutReport.FailureReason;
	FSRResourceV2RunRestoreReport RollbackReport;
	OutReport.bRollbackSucceeded = RestoreRunStateInternal(Backup, RollbackReport);
	OutReport.FailureReason = OriginalFailureReason;
	if (!OutReport.bRollbackSucceeded)
	{
		OutReport.FailureReason += FString::Printf(
			TEXT(" Rollback also failed: %s"),
			*RollbackReport.FailureReason);
	}
	return false;
}

bool USRResourceV2RunSaveSubsystem::RestoreRunStateInternal(
	const FSRResourceV2RunSaveData& SaveData,
	FSRResourceV2RunRestoreReport& OutReport)
{
	OutReport = FSRResourceV2RunRestoreReport();
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !World->IsGameWorld()
		|| IsGenerationInProgress(*World))
	{
		OutReport.FailureReason = TEXT("Run restore requires a generated game or PIE World.");
		return false;
	}
	const UEnum* RulesetEnum = StaticEnum<ESRResourceRulesetVersion>();
	if (!SaveData.IsSupportedVersion()
		|| !RulesetEnum
		|| !RulesetEnum->IsValidEnumValue(
			static_cast<int64>(SaveData.SourceRulesetVersion))
		|| !SaveData.SpaceLogistics.IsSupportedVersion()
		|| !SaveData.Augments.IsSupportedVersion()
		|| !SaveData.TimeControl.IsSupportedVersion()
		|| !SaveData.RunMilestones.IsSupportedVersion()
		|| !SaveData.PrimaryStar.IsSupportedVersion())
	{
		OutReport.FailureReason = TEXT("Run restore rejected an unsupported parent or child schema.");
		return false;
	}
	if (!SaveData.Augments.CurrentChoices.IsEmpty()
		&& (!SaveData.TimeControl.bSimulationPaused
			|| !SaveData.Augments.bPausedSimulationForCurrentChoice))
	{
		OutReport.FailureReason = TEXT("A pending Augment choice must restore with its simulation pause.");
		return false;
	}

	USRCelestialBodyRegistrySubsystem* Registry =
		World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	USRSpaceLogisticsSubsystem* Logistics =
		World->GetSubsystem<USRSpaceLogisticsSubsystem>();
	USRAugmentSubsystem* Augments = World->GetSubsystem<USRAugmentSubsystem>();
	USRTimeControlSubsystem* TimeControl =
		World->GetSubsystem<USRTimeControlSubsystem>();
	USRRunMilestoneSubsystem* Milestones =
		World->GetSubsystem<USRRunMilestoneSubsystem>();
	if (!IsValid(Registry)
		|| !IsValid(Logistics)
		|| !IsValid(Augments)
		|| !IsValid(TimeControl)
		|| !IsValid(Milestones))
	{
		OutReport.FailureReason = TEXT("Run restore could not resolve every authoritative subsystem.");
		return false;
	}

	TMap<FName, TObjectPtr<AActor>> CurrentBodies;
	if (!BuildBodyMap(*Registry, CurrentBodies, OutReport.FailureReason))
	{
		return false;
	}
	if (SaveData.Version >= FSRResourceV2RunSaveData::FiniteResourceEconomyVersion
		&& SaveData.Bodies.Num() != CurrentBodies.Num())
	{
		OutReport.FailureReason = FString::Printf(
			TEXT("Run restore body topology mismatch: saved %d, current %d."),
			SaveData.Bodies.Num(),
			CurrentBodies.Num());
		return false;
	}

	TSet<FName> SavedBodyIds;
	for (const FSRResourceV2BodySaveData& SavedBody : SaveData.Bodies)
	{
		AActor* Body = CurrentBodies.FindRef(SavedBody.BodyId);
		if (SavedBody.BodyId.IsNone()
			|| SavedBodyIds.Contains(SavedBody.BodyId)
			|| !IsValid(Body))
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("Run restore cannot resolve unique body %s."),
				*SavedBody.BodyId.ToString());
			return false;
		}
		SavedBodyIds.Add(SavedBody.BodyId);
		if ((SavedBody.bHasStructureManager
				&& (!Body->FindComponentByClass<USRStructureInstanceManagerComponent>()
					|| !Body->FindComponentByClass<USRPlanetSurfaceGrid>()))
			|| (SavedBody.bHasFacilityNetwork
				&& !Body->FindComponentByClass<USRFacilityNetworkComponent>()))
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("Run restore body %s is missing a required runtime component."),
				*SavedBody.BodyId.ToString());
			return false;
		}
	}

	ASRStar* PrimaryStar = Cast<ASRStar>(Registry->GetPrimaryStarActor());
	if (!IsValid(PrimaryStar)
		|| SaveData.PrimaryStarBodyId.IsNone()
		|| PrimaryStar->GetFName() != SaveData.PrimaryStarBodyId)
	{
		OutReport.FailureReason = TEXT("Run restore primary Star identity mismatch.");
		return false;
	}

	TimeControl->PauseSimulation();
	FString ChildFailureReason;
	if (!Augments->ImportSaveData(SaveData.Augments, ChildFailureReason))
	{
		OutReport.FailureReason = ChildFailureReason;
		return false;
	}

	for (const FSRResourceV2BodySaveData& SavedBody : SaveData.Bodies)
	{
		AActor* Body = CurrentBodies.FindRef(SavedBody.BodyId);
		if (SavedBody.bHasStructureManager)
		{
			USRStructureInstanceManagerComponent* StructureManager =
				Body->FindComponentByClass<USRStructureInstanceManagerComponent>();
			USRPlanetSurfaceGrid* SurfaceGrid =
				Body->FindComponentByClass<USRPlanetSurfaceGrid>();
			FSRStructureSaveImportReport StructureReport;
			if (!StructureManager->ImportSaveData(
					SurfaceGrid,
					SavedBody.StructureManager,
					StructureReport,
					ChildFailureReason))
			{
				OutReport.FailureReason = FString::Printf(
					TEXT("Body %s Structure restore failed: %s"),
					*SavedBody.BodyId.ToString(),
					*ChildFailureReason);
				return false;
			}
			OutReport.RestoredStructureCount += StructureReport.RestoredStructureCount;
			OutReport.RestoredDepositCount += StructureReport.RestoredDepositCount;
			OutReport.MigratedLegacyPlaceholderCount +=
				StructureReport.MigratedLegacyPlaceholderCount;
			OutReport.MigratedLegacyInfiniteToFiniteCount +=
				StructureReport.MigratedLegacyInfiniteToFiniteCount;
		}
		++OutReport.RestoredBodyCount;
	}

	for (const FSRResourceV2BodySaveData& SavedBody : SaveData.Bodies)
	{
		if (!SavedBody.bHasFacilityNetwork)
		{
			continue;
		}
		AActor* Body = CurrentBodies.FindRef(SavedBody.BodyId);
		USRFacilityNetworkComponent* FacilityNetwork =
			Body->FindComponentByClass<USRFacilityNetworkComponent>();
		if (!FacilityNetwork->ImportSaveData(SavedBody.FacilityNetwork))
		{
			OutReport.FailureReason = FString::Printf(
				TEXT("Body %s Facility Network restore failed."),
				*SavedBody.BodyId.ToString());
			return false;
		}
	}

	Logistics->RefreshHubEndpoints();
	if (!PrimaryStar->ImportRuntimeSaveData(
			SaveData.PrimaryStar,
			ChildFailureReason))
	{
		OutReport.FailureReason = ChildFailureReason;
		return false;
	}
	if (!Logistics->ImportSaveData(SaveData.SpaceLogistics))
	{
		OutReport.FailureReason = TEXT("Space Logistics restore rejected its saved endpoints or cargo.");
		return false;
	}
	if (!Milestones->ImportSaveData(
			SaveData.RunMilestones,
			ChildFailureReason))
	{
		OutReport.FailureReason = ChildFailureReason;
		return false;
	}
	if (!TimeControl->ImportSaveData(
			SaveData.TimeControl,
			ChildFailureReason))
	{
		OutReport.FailureReason = ChildFailureReason;
		return false;
	}

	Milestones->RefreshFromWorld();
	if (USRRunTelemetrySubsystem* Telemetry =
		World->GetSubsystem<USRRunTelemetrySubsystem>())
	{
		Telemetry->ResetTelemetry();
		OutReport.bTelemetryRebuilt = Telemetry->CaptureSnapshotNow();
	}
	return true;
}

bool USRResourceV2RunSaveSubsystem::SaveRunToSlot(
	const FString& SlotName,
	int32 UserIndex,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (SlotName.TrimStartAndEnd().IsEmpty() || UserIndex < 0)
	{
		OutFailureReason = TEXT("Run save requires a non-empty slot name and non-negative user index.");
		return false;
	}
	FSRResourceV2RunSaveData SaveData;
	if (!CaptureRunState(SaveData, OutFailureReason))
	{
		return false;
	}
	USRResourceV2RunSaveGame* SaveGame = Cast<USRResourceV2RunSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USRResourceV2RunSaveGame::StaticClass()));
	if (!IsValid(SaveGame)
		|| !FSRResourceV2RunSaveCodec::Encode(
			SaveData,
			SaveGame->Payload,
			SaveGame->PayloadChecksum,
			OutFailureReason))
	{
		return false;
	}
	if (!UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex))
	{
		OutFailureReason = TEXT("Unreal SaveGameToSlot failed for the Run payload.");
		return false;
	}
	UE_LOG(LogSRRunSave, Display,
		TEXT("[RunSave] Saved slot %s user %d: Bodies=%d Bytes=%d Schema=%d"),
		*SlotName,
		UserIndex,
		SaveData.Bodies.Num(),
		SaveGame->Payload.Num(),
		SaveData.Version);
	return true;
}

bool USRResourceV2RunSaveSubsystem::LoadRunFromSlot(
	const FString& SlotName,
	int32 UserIndex,
	FSRResourceV2RunRestoreReport& OutReport)
{
	OutReport = FSRResourceV2RunRestoreReport();
	if (SlotName.TrimStartAndEnd().IsEmpty() || UserIndex < 0)
	{
		OutReport.FailureReason = TEXT("Run load requires a non-empty slot name and non-negative user index.");
		return false;
	}
	USRResourceV2RunSaveGame* SaveGame = Cast<USRResourceV2RunSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (!IsValid(SaveGame)
		|| SaveGame->PayloadFormatVersion
			!= USRResourceV2RunSaveGame::CurrentPayloadFormatVersion)
	{
		OutReport.FailureReason = TEXT("Run save slot is missing or uses an unsupported payload format.");
		return false;
	}
	FSRResourceV2RunSaveData SaveData;
	if (!FSRResourceV2RunSaveCodec::Decode(
			SaveGame->Payload,
			SaveGame->PayloadChecksum,
			SaveData,
			OutReport.FailureReason))
	{
		return false;
	}
	return RestoreRunState(SaveData, OutReport);
}
