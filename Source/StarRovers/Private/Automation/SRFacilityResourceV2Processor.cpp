#include "Automation/SRFacilityResourceV2Processor.h"

#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceSystemContent.h"
#include "Simulation/SRSimulationSettings.h"

namespace
{
	FSRFacilityResourceV2Evaluation MakeFailure(
		ESRFacilityResourceV2Outcome Outcome,
		const FString& FailureReason)
	{
		FSRFacilityResourceV2Evaluation Evaluation;
		Evaluation.Outcome = Outcome;
		Evaluation.FailureReason = FailureReason;
		return Evaluation;
	}

	FString BuildFamilyStateList(int32 StateFlags)
	{
		TArray<FString> Labels;
		const UEnum* StateEnum = StaticEnum<ESRResourceFamilyState>();
		for (int32 StateIndex = 0; StateIndex <= static_cast<int32>(ESRResourceFamilyState::Collapsed); ++StateIndex)
		{
			const int32 StateBit = 1 << StateIndex;
			if ((StateFlags & StateBit) == 0)
			{
				continue;
			}

			Labels.Add(StateEnum
				? StateEnum->GetDisplayNameTextByValue(StateIndex).ToString()
				: FString::FromInt(StateIndex));
		}
		return Labels.IsEmpty() ? TEXT("None") : FString::Join(Labels, TEXT(", "));
	}

	FString BuildFamilyStateChanges(const FSRResourceProcessResult& Result)
	{
		TArray<FString> Changes;
		const UEnum* StateEnum = StaticEnum<ESRResourceFamilyState>();
		for (int32 StateIndex = 0; StateIndex <= static_cast<int32>(ESRResourceFamilyState::Collapsed); ++StateIndex)
		{
			const int32 StateBit = 1 << StateIndex;
			const FString Label = StateEnum
				? StateEnum->GetDisplayNameTextByValue(StateIndex).ToString()
				: FString::FromInt(StateIndex);
			if ((Result.ActivatedFamilyStateFlags & StateBit) != 0)
			{
				Changes.Add(FString::Printf(TEXT("+%s"), *Label));
			}
			if ((Result.ClearedFamilyStateFlags & StateBit) != 0)
			{
				Changes.Add(FString::Printf(TEXT("-%s"), *Label));
			}
		}
		return Changes.IsEmpty() ? TEXT("None") : FString::Join(Changes, TEXT(", "));
	}

	FString GetEnumDisplayName(const UEnum* Enum, int64 Value)
	{
		return Enum ? Enum->GetDisplayNameTextByValue(Value).ToString() : FString::FromInt(Value);
	}

	ESRFacilityResourceV2Outcome ResolveDefinitionFailureOutcome(
		const USRFacilityDataAsset* FacilityDataAsset)
	{
		if (!IsValid(FacilityDataAsset))
		{
			return ESRFacilityResourceV2Outcome::InvalidFacility;
		}
		if (FacilityDataAsset->FacilityDefinitionVersion
			!= StarRovers::Facilities::CurrentFacilityDefinitionVersion)
		{
			return ESRFacilityResourceV2Outcome::UnsupportedFacilityDefinition;
		}

		const FSRFacilityProcessDefinitionV2& Definition = FacilityDataAsset->ResourceV2Process;
		if (Definition.ProcessRole == ESRFacilityProcessRoleV2::FamilyProcess
			&& Definition.ProcessArchetype.IsNone())
		{
			return ESRFacilityResourceV2Outcome::MissingProcessArchetype;
		}
		if (!FMath::IsFinite(Definition.FacilityEnergyDelta))
		{
			return ESRFacilityResourceV2Outcome::InvalidEnergy;
		}
		if (Definition.ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag)
		{
			return ESRFacilityResourceV2Outcome::InvalidProcessTag;
		}
		if (Definition.ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint)
		{
			return ESRFacilityResourceV2Outcome::InvalidFuelImprint;
		}
		return ESRFacilityResourceV2Outcome::UnsupportedOperation;
	}

	bool ValidateMutationInput(
		const FSRResourceInstance& InputResource,
		const FSRResourceProcessingRules& Rules,
		FString& OutFailureReason)
	{
		if (InputResource.ResourceId.IsNone())
		{
			OutFailureReason = TEXT("ResourceId is missing.");
			return false;
		}
		if (InputResource.ResourceSchemaVersion != StarRovers::Resources::CurrentResourceSchemaVersion)
		{
			OutFailureReason = TEXT("Resource schema must be upgraded before V2 slot processing.");
			return false;
		}
		if (InputResource.ResourceClass != ESRResourceClass::Card
			|| InputResource.Family == ESRResourceFamily::None)
		{
			OutFailureReason = TEXT("Process Tag and Fuel Imprint facilities accept Family Card resources only.");
			return false;
		}
		if (!FMath::IsFinite(InputResource.CurrentEnergy)
			|| (Rules.bClampCurrentEnergyAtZero && InputResource.CurrentEnergy < 0.0))
		{
			OutFailureReason = TEXT("Resource Current Energy violates the active V2 invariant.");
			return false;
		}
		return true;
	}

	FSRFacilityResourceV2Evaluation MakeMutationSuccess(
		ESRFacilityProcessRoleV2 ProcessRole,
		const FSRResourceInstance& InputResource)
	{
		FSRFacilityResourceV2Evaluation Evaluation;
		Evaluation.Outcome = ESRFacilityResourceV2Outcome::Success;
		Evaluation.ProcessRole = ProcessRole;
		Evaluation.ResourceProcessResult.Outcome = ESRResourceProcessOutcome::Success;
		Evaluation.ResourceProcessResult.OutputResource = InputResource;
		Evaluation.ResourceProcessResult.InputEnergy = InputResource.CurrentEnergy;
		Evaluation.ResourceProcessResult.UnclampedOutputEnergy = InputResource.CurrentEnergy;
		Evaluation.ResourceProcessResult.OutputEnergy = InputResource.CurrentEnergy;
		return Evaluation;
	}
}

bool FSRFacilityResourceV2Processor::IsResourceV2RulesetActive()
{
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	return IsValid(Settings)
		&& Settings->ResourceRulesetVersion == ESRResourceRulesetVersion::ResourceV2;
}

bool FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(
	const USRFacilityDataAsset* FacilityDataAsset)
{
	return IsResourceV2RulesetActive()
		&& IsValid(FacilityDataAsset)
		&& FacilityDataAsset->FacilityKind == ESRFacilityKind::Standard
		&& FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process;
}

bool FSRFacilityResourceV2Processor::ValidateProcessDefinition(
	const USRFacilityDataAsset* FacilityDataAsset,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!IsValid(FacilityDataAsset))
	{
		OutFailureReason = TEXT("Facility Data Asset is invalid.");
		return false;
	}
	if (FacilityDataAsset->FacilityDefinitionVersion
		!= StarRovers::Facilities::CurrentFacilityDefinitionVersion)
	{
		OutFailureReason = FString::Printf(
			TEXT("Facility Definition Version %d is not supported by Resource V2."),
			FacilityDataAsset->FacilityDefinitionVersion);
		return false;
	}
	if (FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Process)
	{
		OutFailureReason = TEXT("Resource V2 facility processing currently supports Standard Process facilities only.");
		return false;
	}
	const FSRFacilityProcessDefinitionV2& Definition = FacilityDataAsset->ResourceV2Process;
	if (!FMath::IsFinite(Definition.FacilityEnergyDelta))
	{
		OutFailureReason = TEXT("Resource V2 Facility Energy Delta must be finite.");
		return false;
	}

	switch (Definition.ProcessRole)
	{
	case ESRFacilityProcessRoleV2::FamilyProcess:
		if (Definition.ProcessArchetype.IsNone())
		{
			OutFailureReason = TEXT("Resource V2 Family Process Archetype is required.");
			return false;
		}
		if (Definition.AcceptedFamily == ESRResourceFamily::None
			&& Definition.FamilyAction != ESRResourceFamilyAction::None)
		{
			OutFailureReason = TEXT("Universal Bridge facilities cannot author a Family-specific action.");
			return false;
		}
		if ((Definition.LineRole == ESRFacilityLineRoleV2::UniversalBridge
				&& Definition.AcceptedFamily != ESRResourceFamily::None)
			|| (Definition.LineRole != ESRFacilityLineRoleV2::None
				&& Definition.LineRole != ESRFacilityLineRoleV2::UniversalBridge
				&& Definition.AcceptedFamily == ESRResourceFamily::None))
		{
			OutFailureReason = TEXT("Facility Line Role and accepted Family do not describe the same affinity.");
			return false;
		}
		if (Definition.FamilyAction == ESRResourceFamilyAction::Anneal
			&& (Definition.AcceptedFamily != ESRResourceFamily::Metal
				|| !FMath::IsNearlyZero(Definition.FacilityEnergyDelta)))
		{
			OutFailureReason = TEXT("Anneal must accept Metal and use a zero Facility Energy Delta.");
			return false;
		}
		return true;

	case ESRFacilityProcessRoleV2::ApplyProcessTag:
	{
		FSRProcessTagDefinitionV2 ProcessTagDefinition;
		if (!FSRResourceSystemContent::TryGetProcessTagDefinition(
			Definition.ProcessTagId,
			ProcessTagDefinition))
		{
			OutFailureReason = TEXT("Tag Imprinter requires a known Process Tag content id.");
			return false;
		}
		if (!FMath::IsNearlyZero(Definition.FacilityEnergyDelta)
			|| Definition.FamilyAction != ESRResourceFamilyAction::None)
		{
			OutFailureReason = TEXT("Tag Imprinter cannot change Energy or advance a Family Action.");
			return false;
		}
		return true;
	}

	case ESRFacilityProcessRoleV2::ApplyFuelImprint:
	{
		FSRFuelImprintDefinitionV2 FuelImprintDefinition;
		if (!FSRResourceSystemContent::TryGetFuelImprintDefinition(
			Definition.FuelImprintId,
			FuelImprintDefinition))
		{
			OutFailureReason = TEXT("Fuel Imprinter requires a known Fuel Imprint content id.");
			return false;
		}
		if (!FMath::IsNearlyZero(Definition.FacilityEnergyDelta)
			|| Definition.FamilyAction != ESRResourceFamilyAction::None)
		{
			OutFailureReason = TEXT("Fuel Imprinter cannot change Energy or advance a Family Action.");
			return false;
		}
		return true;
	}

	case ESRFacilityProcessRoleV2::ClearProcessTag:
		if (!FMath::IsNearlyZero(Definition.FacilityEnergyDelta)
			|| Definition.FamilyAction != ESRResourceFamilyAction::None)
		{
			OutFailureReason = TEXT("Tag Scrubber cannot change Energy, Family State, or Family Action history.");
			return false;
		}
		return true;

	default:
		OutFailureReason = TEXT("Unknown Resource V2 Process Role.");
		return false;
	}
}

FName FSRFacilityResourceV2Processor::ResolveProcessTagRecipeId(
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset)
		|| FacilityDataAsset->ResourceV2Process.ProcessRole != ESRFacilityProcessRoleV2::ApplyProcessTag)
	{
		return NAME_None;
	}

	return FacilityInstance.SelectedProcessTagRecipeId.IsNone()
		? FacilityDataAsset->ResourceV2Process.ProcessTagId
		: FacilityInstance.SelectedProcessTagRecipeId;
}

FName FSRFacilityResourceV2Processor::ResolveFuelImprintRecipeId(
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset)
		|| FacilityDataAsset->ResourceV2Process.ProcessRole != ESRFacilityProcessRoleV2::ApplyFuelImprint)
	{
		return NAME_None;
	}

	return FacilityInstance.SelectedFuelImprintRecipeId.IsNone()
		? FacilityDataAsset->ResourceV2Process.FuelImprintId
		: FacilityInstance.SelectedFuelImprintRecipeId;
}

ESRResourceProcessTemperatureState FSRFacilityResourceV2Processor::ConvertTemperature(
	ESRFacilityTemperatureState TemperatureState)
{
	switch (TemperatureState)
	{
	case ESRFacilityTemperatureState::Frozen:
		return ESRResourceProcessTemperatureState::Frozen;
	case ESRFacilityTemperatureState::Cold:
		return ESRResourceProcessTemperatureState::Cold;
	case ESRFacilityTemperatureState::Normal:
		return ESRResourceProcessTemperatureState::Normal;
	case ESRFacilityTemperatureState::Hot:
		return ESRResourceProcessTemperatureState::Hot;
	case ESRFacilityTemperatureState::Overheated:
		return ESRResourceProcessTemperatureState::Overheated;
	default:
		return ESRResourceProcessTemperatureState::None;
	}
}

FSRFacilityResourceV2Evaluation FSRFacilityResourceV2Processor::Evaluate(
	const FSRFacilityInstance& FacilityInstance,
	const FSRResourceInstance& InputResource,
	FName ProcessingBodyId,
	const FSRResourceProcessingRules& Rules)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	FString DefinitionFailure;
	if (!ValidateProcessDefinition(FacilityDataAsset, DefinitionFailure))
	{
		return MakeFailure(ResolveDefinitionFailureOutcome(FacilityDataAsset), DefinitionFailure);
	}

	const FSRFacilityProcessDefinitionV2& Definition = FacilityDataAsset->ResourceV2Process;
	const FName ProcessTagRecipeId = ResolveProcessTagRecipeId(FacilityInstance);
	const FName FuelImprintRecipeId = ResolveFuelImprintRecipeId(FacilityInstance);
	if (Definition.AcceptedFamily != ESRResourceFamily::None
		&& InputResource.Family != Definition.AcceptedFamily)
	{
		return MakeFailure(
			ESRFacilityResourceV2Outcome::FamilyMismatch,
			FString::Printf(
				TEXT("Facility accepts %s but input belongs to %s."),
				*GetEnumDisplayName(StaticEnum<ESRResourceFamily>(), static_cast<int64>(Definition.AcceptedFamily)),
				*GetEnumDisplayName(StaticEnum<ESRResourceFamily>(), static_cast<int64>(InputResource.Family))));
	}
	if (Definition.ProcessRole != ESRFacilityProcessRoleV2::FamilyProcess)
	{
		FString ResourceFailure;
		if (!ValidateMutationInput(InputResource, Rules, ResourceFailure))
		{
			return MakeFailure(ESRFacilityResourceV2Outcome::InvalidResource, ResourceFailure);
		}

		FSRFacilityResourceV2Evaluation Evaluation = MakeMutationSuccess(
			Definition.ProcessRole,
			InputResource);
		switch (Definition.ProcessRole)
		{
		case ESRFacilityProcessRoleV2::ApplyProcessTag:
		{
			if (InputResource.ProcessTagSlot.Lifecycle != ESRResourceSlotLifecycle::Empty
				|| !InputResource.ProcessTagSlot.TagId.IsNone())
			{
				return MakeFailure(
					ESRFacilityResourceV2Outcome::ProcessTagSlotOccupied,
					TEXT("Process Tag slot must be scrubbed before another Tag can be imprinted."));
			}

			FSRProcessTagDefinitionV2 ProcessTagDefinition;
			if (!FSRResourceSystemContent::TryGetProcessTagDefinition(
				ProcessTagRecipeId,
				ProcessTagDefinition))
			{
				return MakeFailure(
					ESRFacilityResourceV2Outcome::InvalidProcessTag,
					TEXT("The selected Process Tag recipe is not valid V2 content."));
			}
			Evaluation.ResourceProcessResult.OutputResource.ProcessTagSlot.TagId = ProcessTagDefinition.TagId;
			Evaluation.ResourceProcessResult.OutputResource.ProcessTagSlot.Lifecycle = ESRResourceSlotLifecycle::Primed;
			Evaluation.ResourceProcessResult.OutputResource.ProcessTagSlot.RemainingTriggers =
				FMath::Max(1, ProcessTagDefinition.TriggerCount);
			Evaluation.ResourceProcessResult.EvaluatedProcessTagId = ProcessTagDefinition.TagId;
			break;
		}

		case ESRFacilityProcessRoleV2::ApplyFuelImprint:
		{
			if (!InputResource.FuelImprintSlot.ImprintId.IsNone())
			{
				return MakeFailure(
					ESRFacilityResourceV2Outcome::FuelImprintSlotOccupied,
					TEXT("Fuel Imprint slot is permanent for this Card and is already occupied."));
			}
			FSRFuelImprintDefinitionV2 FuelImprintDefinition;
			if (!FSRResourceSystemContent::TryGetFuelImprintDefinition(
				FuelImprintRecipeId,
				FuelImprintDefinition))
			{
				return MakeFailure(
					ESRFacilityResourceV2Outcome::InvalidFuelImprint,
					TEXT("The selected Fuel Imprint recipe is not valid V2 content."));
			}
			Evaluation.ResourceProcessResult.OutputResource.FuelImprintSlot.ImprintId =
				FuelImprintDefinition.ImprintId;
			break;
		}

		case ESRFacilityProcessRoleV2::ClearProcessTag:
			Evaluation.ResourceProcessResult.OutputResource.ProcessTagSlot = FSRResourceProcessTagSlot();
			break;

		case ESRFacilityProcessRoleV2::FamilyProcess:
		default:
			return MakeFailure(
				ESRFacilityResourceV2Outcome::UnsupportedOperation,
				TEXT("Unexpected Resource V2 mutation role."));
		}

		StarRovers::Resources::SynchronizeResourceV2RuntimeStateToLegacy(
			Evaluation.ResourceProcessResult.OutputResource);
		return Evaluation;
	}

	FSRFacilityResourceV2Evaluation Evaluation;
	Evaluation.ProcessRole = ESRFacilityProcessRoleV2::FamilyProcess;
	Evaluation.ProcessSpec.ProcessArchetype = Definition.ProcessArchetype;
	Evaluation.ProcessSpec.Temperature = ConvertTemperature(FacilityInstance.TemperatureState);
	Evaluation.ProcessSpec.FamilyAction = Definition.FamilyAction;
	Evaluation.ProcessSpec.bIsFamilySpecialist = Definition.AcceptedFamily != ESRResourceFamily::None;
	Evaluation.ProcessSpec.FacilityEnergyDelta = Definition.FacilityEnergyDelta;
	Evaluation.ProcessSpec.ProcessingBodyId = ProcessingBodyId;
	Evaluation.InputGeneralProcessesSinceReset = FMath::Max(
		0,
		InputResource.ProcessingMemory.GeneralProcessesSinceReset);
	Evaluation.RefinementResistance = FSRRefinementResistanceV2::MakeInactive(
		FacilityDataAsset->BaseProcessSeconds);
	if (!FMath::IsNearlyZero(Definition.FacilityEnergyDelta))
	{
		const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
		Evaluation.RefinementResistance = FSRRefinementResistanceV2::Evaluate(
			InputResource,
			FacilityDataAsset->BaseProcessSeconds,
			IsValid(Settings) ? Settings->RefinementResistanceEnergyScaleV2 : 40.0);
	}
	Evaluation.ResourceProcessResult = FSRResourceProcessingKernel::Evaluate(
		InputResource,
		Evaluation.ProcessSpec,
		Rules);
	if (!Evaluation.ResourceProcessResult.IsSuccess())
	{
		Evaluation.Outcome = ESRFacilityResourceV2Outcome::KernelRejected;
		Evaluation.FailureReason = Evaluation.ResourceProcessResult.FailureReason;
		return Evaluation;
	}

	Evaluation.Outcome = ESRFacilityResourceV2Outcome::Success;
	return Evaluation;
}

FString FSRFacilityResourceV2Processor::BuildPreviewSummary(
	const FSRFacilityResourceV2Evaluation& Evaluation)
{
	if (!Evaluation.IsSuccess())
	{
		return FString::Printf(
			TEXT("Resource V2 Process unavailable\n%s"),
			Evaluation.FailureReason.IsEmpty() ? TEXT("Unknown failure") : *Evaluation.FailureReason);
	}

	const FSRResourceProcessResult& Result = Evaluation.ResourceProcessResult;
	if (Evaluation.ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag)
	{
		return FString::Printf(
			TEXT("Resource V2 Tag Imprint\n")
			TEXT("Tag: %s -> Primed (%d trigger)\n")
			TEXT("Energy: %.1f unchanged\n")
			TEXT("Family State, process history, and location history unchanged"),
			*Result.OutputResource.ProcessTagSlot.TagId.ToString(),
			Result.OutputResource.ProcessTagSlot.RemainingTriggers,
			Result.OutputEnergy);
	}
	if (Evaluation.ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint)
	{
		return FString::Printf(
			TEXT("Resource V2 Fuel Imprint\n")
			TEXT("Imprint: %s\n")
			TEXT("Energy: %.1f unchanged\n")
			TEXT("Family State, process history, and location history unchanged"),
			*Result.OutputResource.FuelImprintSlot.ImprintId.ToString(),
			Result.OutputEnergy);
	}
	if (Evaluation.ProcessRole == ESRFacilityProcessRoleV2::ClearProcessTag)
	{
		return FString::Printf(
			TEXT("Resource V2 Tag Scrub\n")
			TEXT("Process Tag slot -> Empty\n")
			TEXT("Energy: %.1f unchanged\n")
			TEXT("Family State, process history, and location history unchanged"),
			Result.OutputEnergy);
	}

	const FString ProcessTagSummary = Result.EvaluatedProcessTagId.IsNone()
		? TEXT("None")
		: FString::Printf(
			TEXT("%s (%s, %s)"),
			*Result.EvaluatedProcessTagId.ToString(),
			Result.bProcessTagTriggered ? TEXT("Triggered") : TEXT("Waiting"),
			*GetEnumDisplayName(
				StaticEnum<ESRResourceSlotLifecycle>(),
				static_cast<int64>(Result.OutputResource.ProcessTagSlot.Lifecycle)));
	const FString TimingSummary = Evaluation.RefinementResistance.bApplied
		? FString::Printf(
			TEXT("\nCycle: %.2fs base * %.2f Refinement Resistance = %.2fs"),
			Evaluation.RefinementResistance.BaseProcessSeconds,
			Evaluation.RefinementResistance.CycleMultiplier,
			Evaluation.RefinementResistance.EffectiveProcessSeconds)
		: FString::Printf(
			TEXT("\nCycle: %.2fs (Refinement Resistance not applied)"),
			Evaluation.RefinementResistance.BaseProcessSeconds);
	const FString MetalMemorySummary = Result.OutputResource.Family == ESRResourceFamily::Metal
		? FString::Printf(
			TEXT("\nMetal Work Strain: %d -> %d"),
			Evaluation.InputGeneralProcessesSinceReset,
			Result.OutputResource.ProcessingMemory.GeneralProcessesSinceReset)
		: FString();
	return FString::Printf(
		TEXT("Resource V2 Additive Process\n")
		TEXT("%s | %s | %s | %s\n")
		TEXT("Energy: %.1f %+.1f Facility %+.1f Family %+.1f Tag %+.1f Clamp = %.1f\n")
		TEXT("State changes: %s\n")
		TEXT("Active states: %s\n")
		TEXT("Process Tag: %s%s%s"),
		*Evaluation.ProcessSpec.ProcessArchetype.ToString(),
		*GetEnumDisplayName(StaticEnum<ESRResourceFamily>(), static_cast<int64>(Result.OutputResource.Family)),
		*GetEnumDisplayName(StaticEnum<ESRResourceProcessTemperatureState>(), static_cast<int64>(Evaluation.ProcessSpec.Temperature)),
		Evaluation.ProcessSpec.bIsFamilySpecialist ? TEXT("Family Specialist") : TEXT("Universal Bridge"),
		Result.InputEnergy,
		Result.FacilityEnergyDelta,
		Result.FamilyEnergyDelta,
		Result.ProcessTagEnergyDelta,
		Result.ClampEnergyDelta,
		Result.OutputEnergy,
		*BuildFamilyStateChanges(Result),
		*BuildFamilyStateList(Result.OutputResource.ActiveFamilyStateFlags),
		*ProcessTagSummary,
		*TimingSummary,
		*MetalMemorySummary);
}
