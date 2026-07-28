#include "Automation/SRFacilityNetworkComponent.h"

#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SRResourceSystemContent.h"
#include "Engine/World.h"
#include "Simulation/SRAugmentPackageContent.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Utility/SRLog.h"

namespace
{
	bool IsProcessTagRecipeAvailable(const UWorld* World, FName TagId)
	{
		if (const USRAugmentSubsystem* AugmentSubsystem =
			World ? World->GetSubsystem<USRAugmentSubsystem>() : nullptr)
		{
			return AugmentSubsystem->IsProcessTagRecipeUnlockedV2(TagId);
		}

		const TArray<FName> NoPackages;
		return FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(TagId, NoPackages);
	}

	bool IsFuelImprintRecipeAvailable(const UWorld* World, FName ImprintId)
	{
		if (const USRAugmentSubsystem* AugmentSubsystem =
			World ? World->GetSubsystem<USRAugmentSubsystem>() : nullptr)
		{
			return AugmentSubsystem->IsFuelImprintRecipeUnlockedV2(ImprintId);
		}

		const TArray<FName> NoPackages;
		return FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(ImprintId, NoPackages);
	}
}

bool USRFacilityNetworkComponent::GetFacilityResourceV2RecipeState(
	FName OccupantId,
	FName& OutSelectedRecipeId,
	TArray<FName>& OutAvailableRecipeIds,
	FString& OutFailureReason) const
{
	OutSelectedRecipeId = NAME_None;
	OutAvailableRecipeIds.Reset();
	OutFailureReason.Reset();

	const FSRFacilityInstance* FacilityInstance =
		RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !IsValid(FacilityInstance->FacilityDataAsset.Get()))
	{
		OutFailureReason = TEXT("Facility instance is missing.");
		return false;
	}

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance->FacilityDataAsset.Get();
	if (!FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset))
	{
		OutFailureReason = TEXT("Facility does not use the Resource V2 Process route.");
		return false;
	}

	FString DefinitionFailure;
	if (!FSRFacilityResourceV2Processor::ValidateProcessDefinition(
		FacilityDataAsset,
		DefinitionFailure))
	{
		OutFailureReason = DefinitionFailure;
		return false;
	}

	const ESRFacilityProcessRoleV2 ProcessRole = FacilityDataAsset->ResourceV2Process.ProcessRole;
	if (ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag)
	{
		OutSelectedRecipeId =
			FSRFacilityResourceV2Processor::ResolveProcessTagRecipeId(*FacilityInstance);
		TArray<FSRProcessTagDefinitionV2> Definitions;
		FSRResourceSystemContent::GetAllProcessTagDefinitions(Definitions);
		for (const FSRProcessTagDefinitionV2& Definition : Definitions)
		{
			if (IsProcessTagRecipeAvailable(GetWorld(), Definition.TagId))
			{
				OutAvailableRecipeIds.Add(Definition.TagId);
			}
		}
	}
	else if (ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint)
	{
		OutSelectedRecipeId =
			FSRFacilityResourceV2Processor::ResolveFuelImprintRecipeId(*FacilityInstance);
		TArray<FSRFuelImprintDefinitionV2> Definitions;
		FSRResourceSystemContent::GetAllFuelImprintDefinitions(Definitions);
		for (const FSRFuelImprintDefinitionV2& Definition : Definitions)
		{
			if (IsFuelImprintRecipeAvailable(GetWorld(), Definition.ImprintId))
			{
				OutAvailableRecipeIds.Add(Definition.ImprintId);
			}
		}
	}
	else
	{
		OutFailureReason = TEXT("Facility has no selectable Resource V2 recipe.");
		return false;
	}

	if (OutAvailableRecipeIds.IsEmpty())
	{
		OutFailureReason = TEXT("No recipe is unlocked for this Facility yet.");
	}
	else if (!OutAvailableRecipeIds.Contains(OutSelectedRecipeId))
	{
		OutFailureReason = FString::Printf(
			TEXT("Selected recipe %s is locked."),
			OutSelectedRecipeId.IsNone() ? TEXT("None") : *OutSelectedRecipeId.ToString());
	}
	return true;
}

bool USRFacilityNetworkComponent::SetFacilityResourceV2Recipe(
	FName OccupantId,
	FName RecipeId)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !IsValid(FacilityInstance->FacilityDataAsset.Get()))
	{
		return false;
	}
	if (FacilityInstance->bProcessing || !FacilityInstance->ProcessingInventory.IsEmpty())
	{
		if (bLogFacilityNetworkEvents)
		{
			SR_LOG(FacilityNetwork, LogTemp, Warning,
				TEXT("[FacilityNetwork] Recipe change blocked: OccupantId=%s Reason=ProcessingActive"),
				*OccupantId.ToString());
		}
		return false;
	}

	const FSRFacilityProcessDefinitionV2& ProcessDefinition =
		FacilityInstance->FacilityDataAsset->ResourceV2Process;
	FName RequestedRecipeId = RecipeId;
	if (RequestedRecipeId.IsNone())
	{
		RequestedRecipeId = ProcessDefinition.ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag
			? ProcessDefinition.ProcessTagId
			: ProcessDefinition.ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint
				? ProcessDefinition.FuelImprintId
				: NAME_None;
	}

	FName CurrentRecipeId;
	TArray<FName> AvailableRecipeIds;
	FString FailureReason;
	if (!GetFacilityResourceV2RecipeState(
			OccupantId,
			CurrentRecipeId,
			AvailableRecipeIds,
			FailureReason)
		|| RequestedRecipeId.IsNone()
		|| !AvailableRecipeIds.Contains(RequestedRecipeId))
	{
		if (bLogFacilityNetworkEvents)
		{
			SR_LOG(FacilityNetwork, LogTemp, Warning,
				TEXT("[FacilityNetwork] Recipe change rejected: OccupantId=%s Recipe=%s Reason=%s"),
				*OccupantId.ToString(),
				RequestedRecipeId.IsNone() ? TEXT("None") : *RequestedRecipeId.ToString(),
				FailureReason.IsEmpty() ? TEXT("Recipe is unknown or locked") : *FailureReason);
		}
		return false;
	}

	FacilityInstance->SelectedProcessTagRecipeId = NAME_None;
	FacilityInstance->SelectedFuelImprintRecipeId = NAME_None;
	if (ProcessDefinition.ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag)
	{
		FacilityInstance->SelectedProcessTagRecipeId =
			RequestedRecipeId == ProcessDefinition.ProcessTagId ? NAME_None : RequestedRecipeId;
	}
	else if (ProcessDefinition.ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint)
	{
		FacilityInstance->SelectedFuelImprintRecipeId =
			RequestedRecipeId == ProcessDefinition.FuelImprintId ? NAME_None : RequestedRecipeId;
	}
	else
	{
		return false;
	}

	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork, LogTemp, Display,
			TEXT("[FacilityNetwork] Resource V2 recipe selected: OccupantId=%s Recipe=%s Owner=%s"),
			*OccupantId.ToString(),
			*RequestedRecipeId.ToString(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::CycleFacilityResourceV2Recipe(FName OccupantId)
{
	FName SelectedRecipeId;
	TArray<FName> AvailableRecipeIds;
	FString FailureReason;
	if (!GetFacilityResourceV2RecipeState(
			OccupantId,
			SelectedRecipeId,
			AvailableRecipeIds,
			FailureReason)
		|| AvailableRecipeIds.IsEmpty())
	{
		return false;
	}

	const int32 CurrentIndex = AvailableRecipeIds.IndexOfByKey(SelectedRecipeId);
	const int32 NextIndex = CurrentIndex == INDEX_NONE
		? 0
		: (CurrentIndex + 1) % AvailableRecipeIds.Num();
	return SetFacilityResourceV2Recipe(OccupantId, AvailableRecipeIds[NextIndex]);
}
