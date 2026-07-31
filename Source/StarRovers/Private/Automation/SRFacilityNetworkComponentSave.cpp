#include "Automation/SRFacilityNetworkComponent.h"

#include "Automation/SRFacilityResourceOperations.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "Simulation/SRRunModifierTypes.h"
#include "Structure/SRStructureDataAsset.h"
#include "Utility/SRLog.h"

namespace
{
	bool ValidateResourceArray(const TArray<FSRResourceInstance>& Resources, FString& OutFailureReason)
	{
		for (const FSRResourceInstance& Resource : Resources)
		{
			if (!StarRovers::PatternRouting::IsValidPatternPayload(Resource))
			{
				OutFailureReason = TEXT("Facility save contains an invalid Pattern resource payload.");
				return false;
			}
		}
		return true;
	}

	bool ValidatePortInventories(
		const TArray<FSRFacilityPortInventory>& PortInventories,
		ESRFacilityPortKind ExpectedKind,
		FString& OutFailureReason)
	{
		TSet<FName> PortIds;
		TSet<int32> PortIndices;
		for (const FSRFacilityPortInventory& Port : PortInventories)
		{
			bool bDuplicateId = false;
			bool bDuplicateIndex = false;
			if (!Port.PortId.IsNone())
			{
				PortIds.Add(Port.PortId, &bDuplicateId);
			}
			PortIndices.Add(Port.PortIndex, &bDuplicateIndex);
			if (Port.PortKind != ExpectedKind
				|| Port.PortIndex < 0
				|| Port.Capacity < 1
				|| bDuplicateId
				|| bDuplicateIndex
				|| !Port.PortSpec.RoutingFilter.IsCanonical()
				|| !ValidateResourceArray(Port.Inventory, OutFailureReason))
			{
				if (OutFailureReason.IsEmpty())
				{
					OutFailureReason = TEXT("Facility save contains malformed or duplicate port inventory metadata.");
				}
				return false;
			}
			if (StarRovers::FacilityResources::GetInventorySlotStackCount(Port) > Port.Capacity)
			{
				OutFailureReason = TEXT("Facility port inventory exceeds its capacity.");
				return false;
			}
			for (int32 ResourceIndex = 1; ResourceIndex < Port.Inventory.Num(); ++ResourceIndex)
			{
				if (!StarRovers::FacilityResources::AreResourceInstancesStackEquivalent(
					Port.Inventory[0],
					Port.Inventory[ResourceIndex]))
				{
					OutFailureReason = TEXT("Facility port inventory contains non-equivalent Pattern stacks.");
					return false;
				}
			}
		}
		return true;
	}

	bool ValidateRunModifierContext(const FSRRunModifierContext& Context, FString& OutFailureReason)
	{
		FSRRunModifierContext CanonicalContext;
		return FSRRunModifierResolver::BuildContext(
			Context.ActiveSources,
			Context.Revision,
			CanonicalContext,
			OutFailureReason);
	}

	bool ValidateFacility(const FSRFacilityInstance& Facility, FString& OutFailureReason)
	{
		if (Facility.OccupantId.IsNone()
			|| !IsValid(Facility.StructureDataAsset.Get())
			|| !IsValid(Facility.FacilityDataAsset.Get())
			|| Facility.FootprintCellIds.IsEmpty())
		{
			OutFailureReason = TEXT("Facility save contains unresolved identity, assets, or footprint cells.");
			return false;
		}
		const FSRStructureData StructureData = Facility.StructureDataAsset->BuildData();
		if (StructureData.FacilityDataAsset.Get() != Facility.FacilityDataAsset.Get())
		{
			OutFailureReason = TEXT("Facility save does not match its Structure Data Asset.");
			return false;
		}
		if (!StaticEnum<ESRFacilityTemperatureState>()->IsValidEnumValue(static_cast<int64>(Facility.TemperatureState))
			|| !FMath::IsFinite(Facility.ProcessProgressSeconds)
			|| Facility.ProcessProgressSeconds < 0.0f
			|| !FSRPatternEnvironmentResolver::IsValidEnvironmentSpec(Facility.PatternEnvironment)
			|| !ValidateRunModifierContext(Facility.RunModifierContext, OutFailureReason)
			|| !ValidatePortInventories(Facility.InputPortInventories, ESRFacilityPortKind::Input, OutFailureReason)
			|| !ValidatePortInventories(Facility.OutputPortInventories, ESRFacilityPortKind::Output, OutFailureReason)
			|| !ValidateResourceArray(Facility.InputInventory, OutFailureReason)
			|| !ValidateResourceArray(Facility.OutputInventory, OutFailureReason)
			|| !ValidateResourceArray(Facility.ProcessingInventory, OutFailureReason))
		{
			if (OutFailureReason.IsEmpty())
			{
				OutFailureReason = TEXT("Facility save contains invalid runtime state.");
			}
			return false;
		}
		if (Facility.bProcessing && Facility.ProcessingInventory.IsEmpty())
		{
			OutFailureReason = TEXT("A processing facility has no snapshotted input Pattern.");
			return false;
		}
		TSet<int32> AutoLaunchPorts;
		for (const int32 PortIndex : Facility.StarFuelMissileAutoLaunchInputPortIndices)
		{
			bool bDuplicate = false;
			AutoLaunchPorts.Add(PortIndex, &bDuplicate);
			if (PortIndex < 0 || PortIndex >= Facility.InputPortInventories.Num() || bDuplicate)
			{
				OutFailureReason = TEXT("Facility save contains an invalid star-missile auto-launch port.");
				return false;
			}
		}
		return true;
	}
}

void USRFacilityNetworkComponent::ExportSaveData(FSRFacilityNetworkSaveData& OutSaveData) const
{
	OutSaveData = FSRFacilityNetworkSaveData();
	RuntimeState.FacilityInstancesByOccupantId.GenerateValueArray(OutSaveData.Facilities);
	OutSaveData.Facilities.Sort([](const FSRFacilityInstance& Left, const FSRFacilityInstance& Right)
	{
		return Left.OccupantId.LexicalLess(Right.OccupantId);
	});
}

bool USRFacilityNetworkComponent::CanImportSaveData(
	const FSRFacilityNetworkSaveData& SaveData,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!StarRovers::Save::FacilityNetwork::IsSupportedVersion(SaveData.Version))
	{
		OutFailureReason = FString::Printf(TEXT("Unsupported facility-network save version %d."), SaveData.Version);
		return false;
	}
	TSet<FName> OccupantIds;
	for (const FSRFacilityInstance& Facility : SaveData.Facilities)
	{
		bool bDuplicate = false;
		OccupantIds.Add(Facility.OccupantId, &bDuplicate);
		if (bDuplicate || !ValidateFacility(Facility, OutFailureReason))
		{
			if (OutFailureReason.IsEmpty())
			{
				OutFailureReason = FString::Printf(TEXT("Duplicate facility occupant '%s'."), *Facility.OccupantId.ToString());
			}
			return false;
		}
	}
	return true;
}

bool USRFacilityNetworkComponent::ImportSaveData(const FSRFacilityNetworkSaveData& SaveData)
{
	FString FailureReason;
	if (!CanImportSaveData(SaveData, FailureReason))
	{
		SR_LOG(FacilityNetwork, LogTemp, Error, TEXT("Facility save import rejected for '%s': %s"), *GetNameSafe(GetOwner()), *FailureReason);
		return false;
	}

	TMap<FName, FSRFacilityInstance> ImportedFacilities;
	for (FSRFacilityInstance Facility : SaveData.Facilities)
	{
		FSRRunModifierContext CanonicalContext;
		FSRRunModifierResolver::BuildContext(
			Facility.RunModifierContext.ActiveSources,
			Facility.RunModifierContext.Revision,
			CanonicalContext,
			FailureReason);
		Facility.RunModifierContext = MoveTemp(CanonicalContext);
		ImportedFacilities.Add(Facility.OccupantId, MoveTemp(Facility));
	}
	RuntimeState.FacilityInstancesByOccupantId = MoveTemp(ImportedFacilities);
	SetComponentTickEnabled(bAutoProcessFacilities && !RuntimeState.FacilityInstancesByOccupantId.IsEmpty());
	return true;
}
