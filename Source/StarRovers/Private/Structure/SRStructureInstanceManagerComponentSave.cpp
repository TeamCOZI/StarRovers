#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Engine/StaticMesh.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Utility/SRLog.h"

namespace
{
	bool ValidateDeposit(
		const FSRPlacedStructureSaveData& StructureSave,
		const FSRStructureData& StructureData,
		FString& OutFailureReason)
	{
		if (StructureData.bIsResourceDeposit != StructureSave.bHasResourceDeposit)
		{
			OutFailureReason = TEXT("Saved resource-deposit presence does not match its Structure Data Asset.");
			return false;
		}
		if (!StructureSave.bHasResourceDeposit)
		{
			return true;
		}

		const FSRResourceDepositInstance& Deposit = StructureSave.ResourceDeposit;
		if (Deposit.OccupantId != StructureSave.OccupantId
			|| Deposit.StructureId != StructureData.StructureId
			|| Deposit.ResourceDataAsset.Get() != StructureData.DepositResourceDataAsset.Get()
			|| !Deposit.IsPatternSourceValid()
			|| Deposit.TotalAmount < 1
			|| Deposit.RemainingAmount < 0
			|| Deposit.RemainingAmount > Deposit.TotalAmount)
		{
			OutFailureReason = TEXT("Saved resource deposit has invalid identity, Pattern, or remaining amount.");
			return false;
		}
		return true;
	}
}

void USRStructureInstanceManagerComponent::ExportSaveData(FSRStructureManagerSaveData& OutSaveData) const
{
	OutSaveData = FSRStructureManagerSaveData();
	OutSaveData.NextStructureInstanceSequence = FMath::Max(1, NextStructureInstanceSequence);
	TArray<FName> OccupantIds;
	PlacedStructuresByOccupantId.GetKeys(OccupantIds);
	OccupantIds.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});
	for (const FName OccupantId : OccupantIds)
	{
		const FSRPlacedStructureInstance& Placed = PlacedStructuresByOccupantId.FindChecked(OccupantId);
		FSRPlacedStructureSaveData& StructureSave = OutSaveData.Structures.AddDefaulted_GetRef();
		StructureSave.OccupantId = Placed.OccupantId;
		StructureSave.OriginCellId = Placed.OriginCellId;
		StructureSave.PlacementRotationSteps = Placed.PlacementRotationSteps;
		StructureSave.StructureDataAsset = Placed.StructureDataAsset;
		StructureSave.bNaturalStructure = Placed.bNaturalStructure;
		StructureSave.bUseStaticMeshMaterials = Placed.bUseStaticMeshMaterials;
		if (const FSRResourceDepositInstance* Deposit = ResourceDepositsByOccupantId.Find(OccupantId))
		{
			StructureSave.bHasResourceDeposit = true;
			StructureSave.ResourceDeposit = *Deposit;
		}
	}
}

bool USRStructureInstanceManagerComponent::CanImportSaveData(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRStructureManagerSaveData& SaveData,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!IsValid(SurfaceGrid))
	{
		OutFailureReason = TEXT("Structure save requires a valid surface grid.");
		return false;
	}
	if (!StarRovers::Save::Structures::IsSupportedVersion(SaveData.Version))
	{
		OutFailureReason = FString::Printf(TEXT("Unsupported structure save version %d."), SaveData.Version);
		return false;
	}
	if (SaveData.NextStructureInstanceSequence < 1)
	{
		OutFailureReason = TEXT("Next structure sequence must be positive.");
		return false;
	}

	TSet<FName> OccupantIds;
	TSet<FSRPlanetSurfaceGridCellId> OccupiedCells;
	for (const FSRPlacedStructureSaveData& StructureSave : SaveData.Structures)
	{
		bool bDuplicateOccupant = false;
		OccupantIds.Add(StructureSave.OccupantId, &bDuplicateOccupant);
		if (StructureSave.OccupantId.IsNone() || bDuplicateOccupant || !IsValid(StructureSave.StructureDataAsset.Get()))
		{
			OutFailureReason = TEXT("Structure save contains an empty, duplicate, or unresolved occupant.");
			return false;
		}
		const FSRStructureData StructureData = StructureSave.StructureDataAsset->BuildData();
		if (StructureData.BuildKind != ESRStructureBuildKind::Structure
			|| StructureData.StructureId.IsNone()
			|| !IsValid(StructureData.StaticMesh.Get())
			|| StructureSave.PlacementRotationSteps != StarRovers::Structure::NormalizePlacementRotationSteps(StructureSave.PlacementRotationSteps)
			|| !ValidateDeposit(StructureSave, StructureData, OutFailureReason))
		{
			if (OutFailureReason.IsEmpty())
			{
				OutFailureReason = TEXT("Structure save contains invalid asset or rotation data.");
			}
			return false;
		}

		TArray<FSRPlanetSurfaceGridCellId> FootprintCells;
		if (!SurfaceGrid->GetFootprintCellIds(
			StructureSave.OriginCellId,
			StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, StructureSave.PlacementRotationSteps),
			StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, StructureSave.PlacementRotationSteps),
			FootprintCells))
		{
			OutFailureReason = TEXT("Structure save footprint is outside the current surface topology.");
			return false;
		}
		for (const FSRPlanetSurfaceGridCellId& CellId : FootprintCells)
		{
			bool bDuplicateCell = false;
			OccupiedCells.Add(CellId, &bDuplicateCell);
			if (bDuplicateCell)
			{
				OutFailureReason = TEXT("Saved structures overlap on the same surface cell.");
				return false;
			}
		}
	}
	return true;
}

bool USRStructureInstanceManagerComponent::ApplySaveDataUnchecked(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRStructureManagerSaveData& SaveData)
{
	ClearAllStructures(SurfaceGrid);
	for (const FSRPlacedStructureSaveData& StructureSave : SaveData.Structures)
	{
		FName RestoredOccupantId;
		if (!TryPlaceStructureOnSurfaceGridInternal(
			SurfaceGrid,
			StructureSave.OriginCellId,
			StructureSave.StructureDataAsset,
			RestoredOccupantId,
			StructureSave.bNaturalStructure,
			StructureSave.bUseStaticMeshMaterials,
			StructureSave.PlacementRotationSteps,
			StructureSave.OccupantId)
			|| RestoredOccupantId != StructureSave.OccupantId)
		{
			return false;
		}
		if (StructureSave.bHasResourceDeposit)
		{
			ResourceDepositsByOccupantId.Add(StructureSave.OccupantId, StructureSave.ResourceDeposit);
		}
	}
	NextStructureInstanceSequence = SaveData.NextStructureInstanceSequence;
	return true;
}

bool USRStructureInstanceManagerComponent::ImportSaveData(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRStructureManagerSaveData& SaveData)
{
	FString FailureReason;
	if (!CanImportSaveData(SurfaceGrid, SaveData, FailureReason))
	{
		SR_LOG(FacilityNetwork, LogTemp, Error, TEXT("Structure save import rejected for '%s': %s"), *GetNameSafe(GetOwner()), *FailureReason);
		return false;
	}

	FSRStructureManagerSaveData RollbackData;
	ExportSaveData(RollbackData);
	if (ApplySaveDataUnchecked(SurfaceGrid, SaveData))
	{
		return true;
	}

	ApplySaveDataUnchecked(SurfaceGrid, RollbackData);
	SR_LOG(FacilityNetwork, LogTemp, Error, TEXT("Structure save import failed during commit for '%s'; previous state was restored."), *GetNameSafe(GetOwner()));
	return false;
}
