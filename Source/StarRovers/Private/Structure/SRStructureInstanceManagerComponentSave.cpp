#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Automation/SRResourceDataAsset.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	struct FResolvedPlacedStructure
	{
		FSRPlacedStructureSaveData Saved;
		TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;
		TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	};

	void SetFailure(FString* OutFailureReason, const FString& FailureReason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
	}

	bool AreCellArraysEqual(
		const TArray<FSRPlanetSurfaceGridCellId>& Left,
		const TArray<FSRPlanetSurfaceGridCellId>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!(Left[Index] == Right[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool IsResourceV2Deposit(const FSRStructureData& StructureData)
	{
		const USRResourceDataAsset* Resource = StructureData.DepositResourceDataAsset.Get();
		return IsValid(Resource)
			&& Resource->ResourceDefinitionVersion
				>= StarRovers::Resources::CurrentResourceDefinitionVersion;
	}

	bool ResolveStructureEntries(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRStructureInstanceManagerSaveData& SaveData,
		TArray<FResolvedPlacedStructure>& OutResolvedStructures,
		FString& OutFailureReason)
	{
		OutResolvedStructures.Reset();
		if (!IsValid(SurfaceGrid))
		{
			OutFailureReason = TEXT("Structure save import requires a valid Surface Grid.");
			return false;
		}

		TSet<FName> OccupantIds;
		TSet<FSRPlanetSurfaceGridCellId> OccupiedCells;
		OutResolvedStructures.Reserve(SaveData.PlacedStructures.Num());
		for (const FSRPlacedStructureSaveData& SavedStructure : SaveData.PlacedStructures)
		{
			if (SavedStructure.OccupantId.IsNone()
				|| SavedStructure.StructureId.IsNone()
				|| OccupantIds.Contains(SavedStructure.OccupantId))
			{
				OutFailureReason = FString::Printf(
					TEXT("Structure save contains a missing or duplicate OccupantId: %s."),
					*SavedStructure.OccupantId.ToString());
				return false;
			}
			OccupantIds.Add(SavedStructure.OccupantId);

			USRStructureDataAsset* StructureDataAsset =
				SavedStructure.StructureDataAsset.LoadSynchronous();
			if (!IsValid(StructureDataAsset))
			{
				OutFailureReason = FString::Printf(
					TEXT("Structure %s cannot resolve its authored asset."),
					*SavedStructure.OccupantId.ToString());
				return false;
			}

			const FSRStructureData StructureData = StructureDataAsset->BuildData();
			if (StructureData.BuildKind != ESRStructureBuildKind::Structure
				|| StructureData.StructureId != SavedStructure.StructureId
				|| !IsValid(StructureData.StaticMesh.Get()))
			{
				OutFailureReason = FString::Printf(
					TEXT("Structure %s no longer matches its authored StructureId or mesh."),
					*SavedStructure.OccupantId.ToString());
				return false;
			}

			const int32 RotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(
				SavedStructure.PlacementRotationSteps);
			TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
			if (!SurfaceGrid->GetFootprintCellIds(
					SavedStructure.OriginCellId,
					StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, RotationSteps),
					StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, RotationSteps),
					FootprintCellIds)
				|| FootprintCellIds.IsEmpty()
				|| (!SavedStructure.FootprintCellIds.IsEmpty()
					&& !AreCellArraysEqual(SavedStructure.FootprintCellIds, FootprintCellIds)))
			{
				OutFailureReason = FString::Printf(
					TEXT("Structure %s has a stale or invalid footprint."),
					*SavedStructure.OccupantId.ToString());
				return false;
			}

			for (const FSRPlanetSurfaceGridCellId& CellId : FootprintCellIds)
			{
				FSRPlanetSurfaceGridCellInfo CellInfo;
				// bCanConstruct currently includes the live occupancy bit, so every
				// legitimately saved occupied cell reports false before the atomic
				// clear. Existence plus overlap validation is the correct preflight;
				// TryPlace validates constructibility again after the clear.
				if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo)
					|| OccupiedCells.Contains(CellId))
				{
					OutFailureReason = FString::Printf(
						TEXT("Structure %s targets a missing, blocked, or overlapping cell."),
						*SavedStructure.OccupantId.ToString());
					return false;
				}
				OccupiedCells.Add(CellId);
			}

			FResolvedPlacedStructure& Resolved = OutResolvedStructures.AddDefaulted_GetRef();
			Resolved.Saved = SavedStructure;
			Resolved.Saved.PlacementRotationSteps = RotationSteps;
			Resolved.StructureDataAsset = StructureDataAsset;
			Resolved.FootprintCellIds = MoveTemp(FootprintCellIds);
		}

		OutResolvedStructures.Sort(
			[](const FResolvedPlacedStructure& Left, const FResolvedPlacedStructure& Right)
			{
				return Left.Saved.OccupantId.LexicalLess(Right.Saved.OccupantId);
			});
		return true;
	}
}

bool FSRResourceDepositSaveMigration::MigrateAmount(
	int32 SourceVersion,
	const FSRResourceDepositSaveData& SavedDeposit,
	bool bRuntimeResourceV2,
	int32 RuntimeAuthoredTotalAmount,
	FSRResourceDepositSaveData& OutMigratedDeposit,
	bool& bOutMigratedLegacyPlaceholder,
	bool& bOutMigratedLegacyInfiniteToFinite,
	FString* OutFailureReason)
{
	OutMigratedDeposit = SavedDeposit;
	bOutMigratedLegacyPlaceholder = false;
	bOutMigratedLegacyInfiniteToFinite = false;
	if (SourceVersion < FSRStructureInstanceManagerSaveData::InitialVersion
		|| SourceVersion > FSRStructureInstanceManagerSaveData::CurrentVersion)
	{
		SetFailure(OutFailureReason, TEXT("Unsupported Structure save version."));
		return false;
	}

	ESRResourceDepositPersistenceKind SourceKind = SavedDeposit.PersistenceKind;
	if (SourceVersion < FSRStructureInstanceManagerSaveData::FiniteResourceEconomyVersion)
	{
		SourceKind = SavedDeposit.TotalAmount <= 0
			|| SavedDeposit.TotalAmount >= MAX_int32
			|| SavedDeposit.RemainingAmount >= MAX_int32
			? ESRResourceDepositPersistenceKind::LegacyInfinite
			: ESRResourceDepositPersistenceKind::Finite;
	}

	const int32 SafeRuntimeTotal = FMath::Clamp(
		RuntimeAuthoredTotalAmount,
		1,
		MAX_int32 - 1);
	if (SourceKind == ESRResourceDepositPersistenceKind::LegacyInfinite)
	{
		if (bRuntimeResourceV2)
		{
			OutMigratedDeposit.PersistenceKind = ESRResourceDepositPersistenceKind::Finite;
			OutMigratedDeposit.TotalAmount = SafeRuntimeTotal;
			OutMigratedDeposit.RemainingAmount = SafeRuntimeTotal;
			bOutMigratedLegacyInfiniteToFinite = true;
		}
		else
		{
			OutMigratedDeposit.PersistenceKind =
				ESRResourceDepositPersistenceKind::LegacyInfinite;
			OutMigratedDeposit.TotalAmount = MAX_int32;
			OutMigratedDeposit.RemainingAmount = MAX_int32;
		}
		if (OutFailureReason)
		{
			OutFailureReason->Reset();
		}
		return true;
	}

	if (SavedDeposit.TotalAmount <= 0
		|| SavedDeposit.TotalAmount >= MAX_int32
		|| SavedDeposit.RemainingAmount < 0
		|| SavedDeposit.RemainingAmount > SavedDeposit.TotalAmount)
	{
		SetFailure(OutFailureReason, FString::Printf(
			TEXT("Deposit %s has an invalid finite amount %d/%d."),
			*SavedDeposit.OccupantId.ToString(),
			SavedDeposit.RemainingAmount,
			SavedDeposit.TotalAmount));
		return false;
	}

	OutMigratedDeposit.PersistenceKind = ESRResourceDepositPersistenceKind::Finite;
	if (SourceVersion < FSRStructureInstanceManagerSaveData::FiniteResourceEconomyVersion
		&& bRuntimeResourceV2
		&& SavedDeposit.TotalAmount >= LegacyPlaceholderTotalAmount)
	{
		const double RemainingRatio = static_cast<double>(SavedDeposit.RemainingAmount)
			/ static_cast<double>(SavedDeposit.TotalAmount);
		OutMigratedDeposit.TotalAmount = SafeRuntimeTotal;
		OutMigratedDeposit.RemainingAmount = FMath::Clamp(
			FMath::RoundToInt(RemainingRatio * static_cast<double>(SafeRuntimeTotal)),
			0,
			SafeRuntimeTotal);
		bOutMigratedLegacyPlaceholder = true;
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}

void USRStructureInstanceManagerComponent::ExportSaveData(
	FSRStructureInstanceManagerSaveData& OutSaveData) const
{
	OutSaveData = FSRStructureInstanceManagerSaveData();
	OutSaveData.NextStructureInstanceSequence = FMath::Max(1, NextStructureInstanceSequence);

	TArray<FName> OccupantIds;
	PlacedStructuresByOccupantId.GenerateKeyArray(OccupantIds);
	OccupantIds.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});
	OutSaveData.PlacedStructures.Reserve(OccupantIds.Num());
	for (const FName OccupantId : OccupantIds)
	{
		const FSRPlacedStructureInstance* Placed =
			PlacedStructuresByOccupantId.Find(OccupantId);
		if (!Placed)
		{
			continue;
		}
		FSRPlacedStructureSaveData& Saved =
			OutSaveData.PlacedStructures.AddDefaulted_GetRef();
		Saved.OccupantId = Placed->OccupantId;
		Saved.StructureId = Placed->StructureId;
		Saved.StructureDataAsset = Placed->StructureDataAsset.Get();
		Saved.OriginCellId = Placed->OriginCellId;
		Saved.FootprintCellIds = Placed->FootprintCellIds;
		Saved.PlacementRotationSteps = Placed->PlacementRotationSteps;
		Saved.bNaturalStructure = Placed->bNaturalStructure;
		Saved.bUseStaticMeshMaterials = Placed->bUseStaticMeshMaterials;
	}

	TArray<FSRResourceDepositInstance> Deposits;
	GetResourceDepositInstances(Deposits);
	OutSaveData.ResourceDeposits.Reserve(Deposits.Num());
	for (const FSRResourceDepositInstance& Deposit : Deposits)
	{
		FSRResourceDepositSaveData& Saved =
			OutSaveData.ResourceDeposits.AddDefaulted_GetRef();
		Saved.OccupantId = Deposit.OccupantId;
		Saved.StructureId = Deposit.StructureId;
		Saved.ResourceId = Deposit.ResourceId;
		Saved.ResourceDataAsset = Deposit.ResourceDataAsset.Get();
		Saved.PersistenceKind = FSRResourceDepositAmountModel::IsInfinite(Deposit.TotalAmount)
			? ESRResourceDepositPersistenceKind::LegacyInfinite
			: ESRResourceDepositPersistenceKind::Finite;
		Saved.TotalAmount = Saved.PersistenceKind
			== ESRResourceDepositPersistenceKind::LegacyInfinite
			? MAX_int32
			: Deposit.TotalAmount;
		Saved.RemainingAmount = Saved.PersistenceKind
			== ESRResourceDepositPersistenceKind::LegacyInfinite
			? MAX_int32
			: Deposit.RemainingAmount;
	}
}

bool USRStructureInstanceManagerComponent::ImportSaveData(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRStructureInstanceManagerSaveData& SaveData,
	FSRStructureSaveImportReport& OutReport,
	FString& OutFailureReason)
{
	OutReport = FSRStructureSaveImportReport();
	OutReport.SourceVersion = SaveData.Version;
	OutFailureReason.Reset();
	if (!SaveData.IsSupportedVersion())
	{
		OutFailureReason = FString::Printf(
			TEXT("Unsupported Structure save version %d; supported range is %d-%d."),
			SaveData.Version,
			FSRStructureInstanceManagerSaveData::InitialVersion,
			FSRStructureInstanceManagerSaveData::CurrentVersion);
		return false;
	}
	if (SaveData.NextStructureInstanceSequence < 1)
	{
		OutFailureReason = TEXT("Structure save contains an invalid next-instance sequence.");
		return false;
	}

	TArray<FResolvedPlacedStructure> ResolvedStructures;
	if (!ResolveStructureEntries(
		SurfaceGrid,
		SaveData,
		ResolvedStructures,
		OutFailureReason))
	{
		return false;
	}

	TMap<FName, FSRStructureData> StructureDefinitions;
	if (ResolvedStructures.IsEmpty())
	{
		for (const TPair<FName, FSRPlacedStructureInstance>& Pair :
			PlacedStructuresByOccupantId)
		{
			if (IsValid(Pair.Value.StructureDataAsset.Get()))
			{
				StructureDefinitions.Add(
					Pair.Key,
					Pair.Value.StructureDataAsset->BuildData());
			}
		}
	}
	else
	{
		for (const FResolvedPlacedStructure& Resolved : ResolvedStructures)
		{
			StructureDefinitions.Add(
				Resolved.Saved.OccupantId,
				Resolved.StructureDataAsset->BuildData());
		}
	}

	TSet<FName> DepositOccupantIds;
	TArray<FSRResourceDepositSaveData> MigratedDeposits;
	MigratedDeposits.Reserve(SaveData.ResourceDeposits.Num());
	for (const FSRResourceDepositSaveData& SavedDeposit : SaveData.ResourceDeposits)
	{
		if (SavedDeposit.OccupantId.IsNone()
			|| DepositOccupantIds.Contains(SavedDeposit.OccupantId))
		{
			OutFailureReason = FString::Printf(
				TEXT("Deposit save contains a missing or duplicate OccupantId: %s."),
				*SavedDeposit.OccupantId.ToString());
			return false;
		}
		DepositOccupantIds.Add(SavedDeposit.OccupantId);

		const FSRStructureData* StructureData =
			StructureDefinitions.Find(SavedDeposit.OccupantId);
		const USRResourceDataAsset* RuntimeResource = StructureData
			? StructureData->DepositResourceDataAsset.Get()
			: nullptr;
		if (!StructureData
			|| !StructureData->bIsResourceDeposit
			|| !IsValid(RuntimeResource)
			|| StructureData->StructureId != SavedDeposit.StructureId
			|| RuntimeResource->ResourceId != SavedDeposit.ResourceId)
		{
			OutFailureReason = FString::Printf(
				TEXT("Deposit %s no longer matches the saved Structure or Resource."),
				*SavedDeposit.OccupantId.ToString());
			return false;
		}

		FSRResourceDepositSaveData Migrated;
		bool bMigratedPlaceholder = false;
		bool bMigratedInfinite = false;
		if (!FSRResourceDepositSaveMigration::MigrateAmount(
				SaveData.Version,
				SavedDeposit,
				IsResourceV2Deposit(*StructureData),
				FSRResourceDepositAmountModel::ResolveInitialAmount(
					StructureData->DepositTotalAmount),
				Migrated,
				bMigratedPlaceholder,
				bMigratedInfinite,
				&OutFailureReason))
		{
			return false;
		}
		OutReport.MigratedLegacyPlaceholderCount += bMigratedPlaceholder ? 1 : 0;
		OutReport.MigratedLegacyInfiniteToFiniteCount += bMigratedInfinite ? 1 : 0;
		MigratedDeposits.Add(MoveTemp(Migrated));
	}

	int32 ExpectedDepositCount = 0;
	for (const TPair<FName, FSRStructureData>& Pair : StructureDefinitions)
	{
		ExpectedDepositCount += Pair.Value.bIsResourceDeposit
			&& IsValid(Pair.Value.DepositResourceDataAsset.Get())
			? 1
			: 0;
	}
	if (ExpectedDepositCount != MigratedDeposits.Num())
	{
		OutFailureReason = FString::Printf(
			TEXT("Structure save must contain one amount entry for every deposit (%d/%d)."),
			MigratedDeposits.Num(),
			ExpectedDepositCount);
		return false;
	}

	FSRStructureInstanceManagerSaveData Backup;
	ExportSaveData(Backup);
	auto ApplyResolved = [this, SurfaceGrid](
		const FSRStructureInstanceManagerSaveData& Source,
		const TArray<FResolvedPlacedStructure>& Structures,
		const TArray<FSRResourceDepositSaveData>& Deposits) -> bool
	{
		const bool bContainsPlacementTopology =
			Source.Version >= FSRStructureInstanceManagerSaveData::FiniteResourceEconomyVersion
			|| !Source.PlacedStructures.IsEmpty();
		if (bContainsPlacementTopology)
		{
			ClearAllStructures(SurfaceGrid);
			NextStructureInstanceSequence = 1;
			for (const FResolvedPlacedStructure& Resolved : Structures)
			{
				FName RestoredOccupantId;
				if (!TryPlaceStructureOnSurfaceGridWithOccupantId(
						SurfaceGrid,
						Resolved.Saved.OriginCellId,
						Resolved.StructureDataAsset.Get(),
						Resolved.Saved.OccupantId,
						RestoredOccupantId,
						Resolved.Saved.bNaturalStructure,
						Resolved.Saved.bUseStaticMeshMaterials,
						Resolved.Saved.PlacementRotationSteps)
					|| RestoredOccupantId != Resolved.Saved.OccupantId)
				{
					return false;
				}
			}
			NextStructureInstanceSequence = FMath::Max(
				1,
				Source.NextStructureInstanceSequence);
		}

		for (const FSRResourceDepositSaveData& Deposit : Deposits)
		{
			FSRResourceDepositInstance Updated;
			if (!TryConfigureResourceDepositAmount(
					Deposit.OccupantId,
					Deposit.TotalAmount,
					Deposit.RemainingAmount,
					Updated))
			{
				return false;
			}
		}
		return true;
	};

	if (!ApplyResolved(SaveData, ResolvedStructures, MigratedDeposits))
	{
		TArray<FResolvedPlacedStructure> BackupStructures;
		FString BackupFailure;
		const bool bBackupResolved = ResolveStructureEntries(
			SurfaceGrid,
			Backup,
			BackupStructures,
			BackupFailure);
		const bool bBackupApplied = bBackupResolved
			&& ApplyResolved(Backup, BackupStructures, Backup.ResourceDeposits);
		OutFailureReason = bBackupApplied
			? TEXT("Structure save commit failed; the previous state was restored.")
			: FString::Printf(
				TEXT("Structure save commit failed and its local rollback failed: %s"),
				BackupFailure.IsEmpty() ? TEXT("placement commit rejected") : *BackupFailure);
		return false;
	}

	OutReport.RestoredStructureCount = ResolvedStructures.IsEmpty()
		? PlacedStructuresByOccupantId.Num()
		: ResolvedStructures.Num();
	OutReport.RestoredDepositCount = MigratedDeposits.Num();
	return true;
}
