#include "SRFacilityNetworkSaveAdapter.h"

#include "Automation/SRResourceInstanceOperations.h"
#include "SRFacilityPortInventoryBuilder.h"
#include "SRFacilityProcessingRuleEvaluator.h"
#include "Structure/SRStructureDataAsset.h"

namespace
{
	void PrepareResourceArrayForSave(TArray<FSRResourceInstance>& Resources)
	{
		for (FSRResourceInstance& Resource : Resources)
		{
			StarRovers::Resources::PrepareResourceInstanceForSave(Resource);
		}
	}

	void PreparePortInventoriesForSave(TArray<FSRFacilityPortInventory>& PortInventories)
	{
		for (FSRFacilityPortInventory& PortInventory : PortInventories)
		{
			PortInventory.Capacity = FMath::Max(1, PortInventory.Capacity);
			PrepareResourceArrayForSave(PortInventory.Inventory);
		}
	}

	bool NormalizeImportedResources(TArray<FSRResourceInstance>& Resources)
	{
		for (FSRResourceInstance& Resource : Resources)
		{
			if (Resource.ResourceId.IsNone() || Resource.StackCount <= 0)
			{
				return false;
			}
			StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(Resource);
			Resource.StackCount = FMath::Max(1, Resource.StackCount);
		}
		return true;
	}

	bool NormalizeImportedPortInventories(TArray<FSRFacilityPortInventory>& PortInventories)
	{
		TSet<int32> PortIndices;
		for (FSRFacilityPortInventory& PortInventory : PortInventories)
		{
			if (PortInventory.PortIndex < 0
				|| PortIndices.Contains(PortInventory.PortIndex)
				|| PortInventory.Capacity <= 0
				|| !NormalizeImportedResources(PortInventory.Inventory))
			{
				return false;
			}
			PortIndices.Add(PortInventory.PortIndex);
		}
		return true;
	}

	bool ImportFacility(
		const FSRFacilityInstanceSaveData& SavedFacility,
		FSRFacilityInstance& OutFacility,
		FString& OutFailureReason)
	{
		if (SavedFacility.OccupantId.IsNone())
		{
			OutFailureReason = TEXT("Facility save entry has no OccupantId.");
			return false;
		}

		USRStructureDataAsset* StructureDataAsset = SavedFacility.StructureDataAsset.LoadSynchronous();
		USRFacilityDataAsset* FacilityDataAsset = SavedFacility.FacilityDataAsset.LoadSynchronous();
		if (IsValid(StructureDataAsset))
		{
			const FSRStructureData StructureData = StructureDataAsset->BuildData();
			if (IsValid(StructureData.FacilityDataAsset.Get()))
			{
				FacilityDataAsset = StructureData.FacilityDataAsset.Get();
			}
		}
		if (!IsValid(StructureDataAsset) || !IsValid(FacilityDataAsset))
		{
			OutFailureReason = FString::Printf(
				TEXT("Facility %s cannot resolve its authored Structure/Facility assets."),
				*SavedFacility.OccupantId.ToString());
			return false;
		}

		OutFacility = FSRFacilityInstance();
		OutFacility.OccupantId = SavedFacility.OccupantId;
		OutFacility.StructureDataAsset = StructureDataAsset;
		OutFacility.FacilityDataAsset = FacilityDataAsset;
		OutFacility.OriginCellId = SavedFacility.OriginCellId;
		OutFacility.FootprintCellIds = SavedFacility.FootprintCellIds;
		OutFacility.PlacementRotationSteps =
			StarRovers::Structure::NormalizePlacementRotationSteps(SavedFacility.PlacementRotationSteps);
		OutFacility.InputPortInventories = SavedFacility.InputPortInventories;
		OutFacility.OutputPortInventories = SavedFacility.OutputPortInventories;
		if (!NormalizeImportedPortInventories(OutFacility.InputPortInventories)
			|| !NormalizeImportedPortInventories(OutFacility.OutputPortInventories))
		{
			OutFailureReason = FString::Printf(
				TEXT("Facility %s contains an invalid port inventory."),
				*SavedFacility.OccupantId.ToString());
			return false;
		}
		OutFacility.StarFuelMissileAutoLaunchInputPortIndices =
			SavedFacility.StarFuelMissileAutoLaunchInputPortIndices;
		OutFacility.StarFuelMissileAutoLaunchInputPortIndices.RemoveAll(
			[&OutFacility](int32 InputPortIndex)
			{
				return !OutFacility.InputPortInventories.IsValidIndex(InputPortIndex);
			});
		OutFacility.StarFuelMissileAutoLaunchInputPortIndices.Sort();
		OutFacility.ProcessingInventory = SavedFacility.ProcessingInventory;
		if (!NormalizeImportedResources(OutFacility.ProcessingInventory))
		{
			OutFailureReason = FString::Printf(
				TEXT("Facility %s contains an invalid processing inventory."),
				*SavedFacility.OccupantId.ToString());
			return false;
		}
		OutFacility.OperationalPriority = SavedFacility.OperationalPriority;
		OutFacility.OperationalSpeedFactor = 1.0f;
		OutFacility.SelectedProcessTagRecipeId = SavedFacility.SelectedProcessTagRecipeId;
		OutFacility.SelectedFuelImprintRecipeId = SavedFacility.SelectedFuelImprintRecipeId;
		OutFacility.MiningTargetDepositOccupantId = SavedFacility.MiningTargetDepositOccupantId;
		OutFacility.TemperatureState = SavedFacility.TemperatureState;
		OutFacility.ProcessProgressSeconds = SavedFacility.ProcessProgressSeconds;
		OutFacility.ResolvedProcessSeconds = SavedFacility.ResolvedProcessSeconds;
		OutFacility.bHasResolvedProcessSeconds = SavedFacility.bHasResolvedProcessSeconds;
		OutFacility.bProcessing = SavedFacility.bProcessing;
		OutFacility.bProcessEnabled = SavedFacility.bProcessEnabled;
		OutFacility.bDeliverEnabled = SavedFacility.bDeliverEnabled;
		if (!FMath::IsFinite(OutFacility.ProcessProgressSeconds)
			|| OutFacility.ProcessProgressSeconds < 0.0f)
		{
			OutFailureReason = FString::Printf(
				TEXT("Facility %s has invalid process progress."),
				*SavedFacility.OccupantId.ToString());
			return false;
		}
		if (OutFacility.bProcessing)
		{
			if (OutFacility.ProcessingInventory.IsEmpty()
				|| !OutFacility.bHasResolvedProcessSeconds
				|| !FMath::IsFinite(OutFacility.ResolvedProcessSeconds)
				|| OutFacility.ResolvedProcessSeconds <= 0.0f)
			{
				OutFailureReason = FString::Printf(
					TEXT("Facility %s has an incomplete in-flight process snapshot."),
					*SavedFacility.OccupantId.ToString());
				return false;
			}
		}
		else
		{
			OutFacility.ProcessingInventory.Reset();
			OutFacility.ProcessProgressSeconds = 0.0f;
			FSRFacilityProcessingRuleEvaluator::ClearProcessSecondsSnapshot(OutFacility);
		}

		FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(OutFacility);
		return true;
	}
}

void FSRFacilityNetworkSaveAdapter::ExportSaveData(
	const FSRFacilityNetworkRuntimeState& RuntimeState,
	FSRFacilityNetworkSaveData& OutSaveData)
{
	OutSaveData = FSRFacilityNetworkSaveData();
	OutSaveData.NextFacilitySchedulerOccupantId = RuntimeState.NextFacilitySchedulerOccupantId;
	TArray<FName> OccupantIds;
	RuntimeState.FacilityInstancesByOccupantId.GenerateKeyArray(OccupantIds);
	OccupantIds.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});
	OutSaveData.Facilities.Reserve(OccupantIds.Num());
	for (const FName OccupantId : OccupantIds)
	{
		const FSRFacilityInstance* Facility =
			RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
		if (!Facility)
		{
			continue;
		}

		FSRFacilityInstanceSaveData& SavedFacility = OutSaveData.Facilities.AddDefaulted_GetRef();
		SavedFacility.OccupantId = OccupantId;
		SavedFacility.StructureDataAsset = Facility->StructureDataAsset.Get();
		SavedFacility.FacilityDataAsset = Facility->FacilityDataAsset.Get();
		SavedFacility.OriginCellId = Facility->OriginCellId;
		SavedFacility.FootprintCellIds = Facility->FootprintCellIds;
		SavedFacility.PlacementRotationSteps = Facility->PlacementRotationSteps;
		SavedFacility.InputPortInventories = Facility->InputPortInventories;
		SavedFacility.OutputPortInventories = Facility->OutputPortInventories;
		PreparePortInventoriesForSave(SavedFacility.InputPortInventories);
		PreparePortInventoriesForSave(SavedFacility.OutputPortInventories);
		SavedFacility.StarFuelMissileAutoLaunchInputPortIndices =
			Facility->StarFuelMissileAutoLaunchInputPortIndices;
		SavedFacility.ProcessingInventory = Facility->ProcessingInventory;
		PrepareResourceArrayForSave(SavedFacility.ProcessingInventory);
		SavedFacility.OperationalPriority = Facility->OperationalPriority;
		SavedFacility.SelectedProcessTagRecipeId = Facility->SelectedProcessTagRecipeId;
		SavedFacility.SelectedFuelImprintRecipeId = Facility->SelectedFuelImprintRecipeId;
		SavedFacility.MiningTargetDepositOccupantId = Facility->MiningTargetDepositOccupantId;
		SavedFacility.TemperatureState = Facility->TemperatureState;
		SavedFacility.ProcessProgressSeconds = Facility->ProcessProgressSeconds;
		SavedFacility.ResolvedProcessSeconds = Facility->ResolvedProcessSeconds;
		SavedFacility.bHasResolvedProcessSeconds = Facility->bHasResolvedProcessSeconds;
		SavedFacility.bProcessing = Facility->bProcessing;
		SavedFacility.bProcessEnabled = Facility->bProcessEnabled;
		SavedFacility.bDeliverEnabled = Facility->bDeliverEnabled;
	}
}

bool FSRFacilityNetworkSaveAdapter::ImportSaveData(
	const FSRFacilityNetworkSaveData& SaveData,
	FSRFacilityNetworkRuntimeState& OutRuntimeState,
	FString* OutFailureReason)
{
	FString FailureReason;
	if (!SaveData.IsSupportedVersion())
	{
		FailureReason = FString::Printf(
			TEXT("Unsupported Facility Network save version %d; supported range is %d-%d."),
			SaveData.Version,
			FSRFacilityNetworkSaveData::InitialVersion,
			FSRFacilityNetworkSaveData::CurrentVersion);
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}
		return false;
	}

	FSRFacilityNetworkRuntimeState ImportedState;
	ImportedState.NextFacilitySchedulerOccupantId = SaveData.NextFacilitySchedulerOccupantId;
	ImportedState.bFacilitySchedulerOrderDirty = true;
	for (const FSRFacilityInstanceSaveData& SavedFacility : SaveData.Facilities)
	{
		if (ImportedState.FacilityInstancesByOccupantId.Contains(SavedFacility.OccupantId))
		{
			FailureReason = FString::Printf(
				TEXT("Facility save contains duplicate OccupantId %s."),
				*SavedFacility.OccupantId.ToString());
			if (OutFailureReason)
			{
				*OutFailureReason = FailureReason;
			}
			return false;
		}

		FSRFacilityInstance ImportedFacility;
		if (!ImportFacility(SavedFacility, ImportedFacility, FailureReason))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FailureReason;
			}
			return false;
		}
		ImportedState.FacilityInstancesByOccupantId.Add(
			ImportedFacility.OccupantId,
			MoveTemp(ImportedFacility));
	}

	if (!ImportedState.NextFacilitySchedulerOccupantId.IsNone()
		&& !ImportedState.FacilityInstancesByOccupantId.Contains(
			ImportedState.NextFacilitySchedulerOccupantId))
	{
		ImportedState.NextFacilitySchedulerOccupantId = NAME_None;
	}
	OutRuntimeState = MoveTemp(ImportedState);
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}
