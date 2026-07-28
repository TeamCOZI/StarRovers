#include "Automation/SRResourceProcessingKernel.h"

#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceProcessTagEvaluator.h"

namespace
{
	int32 IncrementCounter(int32 Value)
	{
		const int32 SafeValue = FMath::Max(0, Value);
		return SafeValue < MAX_int32 ? SafeValue + 1 : MAX_int32;
	}

	bool HasFamilyState(int32 StateFlags, ESRResourceFamilyState FamilyState)
	{
		return (StateFlags & StarRovers::Resources::GetFamilyStateBit(FamilyState)) != 0;
	}

	void SetFamilyState(int32& StateFlags, ESRResourceFamilyState FamilyState, bool bActive)
	{
		const int32 StateBit = StarRovers::Resources::GetFamilyStateBit(FamilyState);
		if (bActive)
		{
			StateFlags |= StateBit;
		}
		else
		{
			StateFlags &= ~StateBit;
		}
	}

	int32 GetPositiveFamilyStateMask(ESRResourceFamily Family)
	{
		switch (Family)
		{
		case ESRResourceFamily::Metal:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered);
		case ESRResourceFamily::Crystal:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Resonant);
		case ESRResourceFamily::Organic:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Matured);
		case ESRResourceFamily::Plasma:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Energized);
		case ESRResourceFamily::Void:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Echoing);
		case ESRResourceFamily::None:
		default:
			return 0;
		}
	}

	int32 GetNegativeFamilyStateMask(ESRResourceFamily Family)
	{
		switch (Family)
		{
		case ESRResourceFamily::Metal:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Fatigued);
		case ESRResourceFamily::Crystal:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Fractured);
		case ESRResourceFamily::Organic:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Depleted);
		case ESRResourceFamily::Plasma:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Overloaded);
		case ESRResourceFamily::Void:
			return StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Collapsed);
		case ESRResourceFamily::None:
		default:
			return 0;
		}
	}

	ESRResourceFamilyAction ResolveEffectiveFamilyAction(
		ESRResourceFamily Family,
		const FSRResourceProcessSpec& ProcessSpec)
	{
		if (Family == ESRResourceFamily::Void
			&& ProcessSpec.FamilyAction == ESRResourceFamilyAction::None
			&& ProcessSpec.FacilityEnergyDelta > 0.0)
		{
			return ESRResourceFamilyAction::EnergyGain;
		}
		return ProcessSpec.FamilyAction;
	}

	bool IsFamilyActionAllowed(ESRResourceFamily Family, ESRResourceFamilyAction FamilyAction)
	{
		switch (Family)
		{
		case ESRResourceFamily::Metal:
			return FamilyAction == ESRResourceFamilyAction::None
				|| FamilyAction == ESRResourceFamilyAction::Anneal;
		case ESRResourceFamily::Crystal:
			return FamilyAction == ESRResourceFamilyAction::None;
		case ESRResourceFamily::Organic:
			return FamilyAction == ESRResourceFamilyAction::None
				|| FamilyAction == ESRResourceFamilyAction::Growth;
		case ESRResourceFamily::Plasma:
			return FamilyAction == ESRResourceFamilyAction::None
				|| FamilyAction == ESRResourceFamilyAction::Amplification
				|| FamilyAction == ESRResourceFamilyAction::Discharge;
		case ESRResourceFamily::Void:
			return FamilyAction == ESRResourceFamilyAction::None
				|| FamilyAction == ESRResourceFamilyAction::VoidSacrifice
				|| FamilyAction == ESRResourceFamilyAction::EnergyGain;
		case ESRResourceFamily::None:
		default:
			return false;
		}
	}

	bool AreRuleEnergyValuesFinite(const FSRResourceProcessingRules& Rules)
	{
		return FMath::IsFinite(Rules.MetalTemperedEnergyBonus)
			&& FMath::IsFinite(Rules.MetalFatiguedEnergyPenalty)
			&& FMath::IsFinite(Rules.CrystalResonantEnergyBonus)
			&& FMath::IsFinite(Rules.CrystalFracturedEnergyPenalty)
			&& FMath::IsFinite(Rules.OrganicMaturedEnergyBonus)
			&& FMath::IsFinite(Rules.OrganicDepletedEnergyPenalty)
			&& FMath::IsFinite(Rules.PlasmaEnergizedEnergyBonus)
			&& FMath::IsFinite(Rules.PlasmaOverloadedEnergyPenalty)
			&& FMath::IsFinite(Rules.VoidEchoEnergyMultiplier)
			&& FMath::IsFinite(Rules.VoidMaximumEchoEnergyBonus)
			&& FMath::IsFinite(Rules.VoidCollapsedEnergyPenalty);
	}

	void UpdateArchetypeMemory(
		FSRResourceProcessingMemory& Memory,
		const FSRResourceProcessingMemory& PreviousMemory,
		FName ProcessArchetype,
		bool bTrackConsecutiveCount)
	{
		const bool bSameArchetype = !ProcessArchetype.IsNone()
			&& PreviousMemory.LastProcessArchetype == ProcessArchetype;
		Memory.LastProcessArchetype = ProcessArchetype;
		Memory.ConsecutiveSameArchetypeCount = bTrackConsecutiveCount
			? (bSameArchetype ? IncrementCounter(PreviousMemory.ConsecutiveSameArchetypeCount) : 1)
			: 0;
	}

	void ApplyMetalRules(
		FSRResourceInstance& Resource,
		const FSRResourceProcessingMemory& PreviousMemory,
		const FSRResourceProcessSpec& ProcessSpec,
		const FSRResourceProcessingRules& Rules,
		FSRResourceProcessResult& Result)
	{
		FSRResourceProcessingMemory& Memory = Resource.ProcessingMemory;
		UpdateArchetypeMemory(Memory, PreviousMemory, ProcessSpec.ProcessArchetype, true);
		Memory.LastTemperature = ProcessSpec.Temperature;
		Memory.LastFamilyAction = Result.EffectiveFamilyAction;
		Memory.ConsecutiveSameFamilyActionCount = 0;
		Memory.StoredFamilyMagnitude = 0.0;
		if (Result.EffectiveFamilyAction == ESRResourceFamilyAction::Anneal)
		{
			Memory.GeneralProcessesSinceReset = 0;
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Tempered, false);
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Fatigued, false);
			return;
		}

		Memory.GeneralProcessesSinceReset = IncrementCounter(PreviousMemory.GeneralProcessesSinceReset);
		const bool bFatigued = HasFamilyState(
			Resource.ActiveFamilyStateFlags,
			ESRResourceFamilyState::Fatigued)
			|| Memory.GeneralProcessesSinceReset
			>= FMath::Max(1, Rules.MetalFatiguedRepeatThreshold);
		const bool bTempered = ProcessSpec.bIsFamilySpecialist
			&& !bFatigued
			&& PreviousMemory.LastTemperature == ESRResourceProcessTemperatureState::Hot
			&& ProcessSpec.Temperature == ESRResourceProcessTemperatureState::Cold;
		SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Tempered, bTempered);
		SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Fatigued, bFatigued);
		if (bTempered)
		{
			Result.FamilyEnergyDelta += FMath::Max(0.0, Rules.MetalTemperedEnergyBonus);
		}
		if (bFatigued)
		{
			Result.FamilyEnergyDelta -= FMath::Max(0.0, Rules.MetalFatiguedEnergyPenalty);
		}
	}

	void ApplyCrystalRules(
		FSRResourceInstance& Resource,
		const FSRResourceProcessingMemory& PreviousMemory,
		const FSRResourceProcessSpec& ProcessSpec,
		const FSRResourceProcessingRules& Rules,
		FSRResourceProcessResult& Result)
	{
		FSRResourceProcessingMemory& Memory = Resource.ProcessingMemory;
		UpdateArchetypeMemory(Memory, PreviousMemory, ProcessSpec.ProcessArchetype, true);
		Memory.LastTemperature = ESRResourceProcessTemperatureState::None;
		Memory.LastFamilyAction = ESRResourceFamilyAction::None;
		Memory.ConsecutiveSameFamilyActionCount = 0;
		Memory.GeneralProcessesSinceReset = 0;
		Memory.StoredFamilyMagnitude = 0.0;

		const bool bResonant = ProcessSpec.bIsFamilySpecialist
			&& Memory.ConsecutiveSameArchetypeCount
			>= FMath::Max(1, Rules.CrystalResonantRepeatThreshold);
		const bool bFractured = Memory.ConsecutiveSameArchetypeCount
			>= FMath::Max(1, Rules.CrystalFracturedRepeatThreshold);
		SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Resonant, bResonant);
		SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Fractured, bFractured);
		if (bResonant)
		{
			Result.FamilyEnergyDelta += FMath::Max(0.0, Rules.CrystalResonantEnergyBonus);
		}
		if (bFractured)
		{
			Result.FamilyEnergyDelta -= FMath::Max(0.0, Rules.CrystalFracturedEnergyPenalty);
		}
	}

	void ApplyOrganicRules(
		FSRResourceInstance& Resource,
		const FSRResourceProcessingMemory& PreviousMemory,
		const FSRResourceProcessSpec& ProcessSpec,
		const FSRResourceProcessingRules& Rules,
		FSRResourceProcessResult& Result)
	{
		FSRResourceProcessingMemory& Memory = Resource.ProcessingMemory;
		UpdateArchetypeMemory(Memory, PreviousMemory, ProcessSpec.ProcessArchetype, false);
		Memory.LastTemperature = ESRResourceProcessTemperatureState::None;
		Memory.LastFamilyAction = Result.EffectiveFamilyAction;
		Memory.ConsecutiveSameFamilyActionCount = 0;
		Memory.StoredFamilyMagnitude = 0.0;

		if (Result.EffectiveFamilyAction == ESRResourceFamilyAction::Growth)
		{
			Memory.GeneralProcessesSinceReset = 0;
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Matured, true);
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Depleted, false);
			return;
		}

		const bool bHasMatured = HasFamilyState(
			Resource.ActiveFamilyStateFlags,
			ESRResourceFamilyState::Matured);
		const bool bConsumesMatured = bHasMatured && ProcessSpec.bIsFamilySpecialist;
		Memory.GeneralProcessesSinceReset = IncrementCounter(PreviousMemory.GeneralProcessesSinceReset);
		const bool bDepleted = HasFamilyState(
			Resource.ActiveFamilyStateFlags,
			ESRResourceFamilyState::Depleted)
			|| Memory.GeneralProcessesSinceReset >= FMath::Max(1, Rules.OrganicDepletedProcessThreshold);
		SetFamilyState(
			Resource.ActiveFamilyStateFlags,
			ESRResourceFamilyState::Matured,
			bHasMatured && !bConsumesMatured);
		SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Depleted, bDepleted);
		if (bConsumesMatured)
		{
			Result.FamilyEnergyDelta += FMath::Max(0.0, Rules.OrganicMaturedEnergyBonus);
		}
		if (bDepleted)
		{
			Result.FamilyEnergyDelta -= FMath::Max(0.0, Rules.OrganicDepletedEnergyPenalty);
		}
	}

	void ApplyPlasmaRules(
		FSRResourceInstance& Resource,
		const FSRResourceProcessingMemory& PreviousMemory,
		const FSRResourceProcessSpec& ProcessSpec,
		const FSRResourceProcessingRules& Rules,
		FSRResourceProcessResult& Result)
	{
		FSRResourceProcessingMemory& Memory = Resource.ProcessingMemory;
		UpdateArchetypeMemory(Memory, PreviousMemory, ProcessSpec.ProcessArchetype, false);
		Memory.LastTemperature = ESRResourceProcessTemperatureState::None;
		Memory.LastFamilyAction = Result.EffectiveFamilyAction;
		Memory.GeneralProcessesSinceReset = 0;
		Memory.StoredFamilyMagnitude = 0.0;

		if (Result.EffectiveFamilyAction == ESRResourceFamilyAction::Discharge)
		{
			Memory.ConsecutiveSameFamilyActionCount = 0;
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Energized, false);
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Overloaded, false);
			return;
		}

		if (Result.EffectiveFamilyAction != ESRResourceFamilyAction::Amplification)
		{
			Memory.ConsecutiveSameFamilyActionCount = 0;
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Energized, false);
			// Overloaded is durable until an explicit Discharge/Grounding action.
			return;
		}

		Memory.ConsecutiveSameFamilyActionCount =
			PreviousMemory.LastFamilyAction == ESRResourceFamilyAction::Amplification
				? IncrementCounter(PreviousMemory.ConsecutiveSameFamilyActionCount)
				: 1;
		const bool bReceivesEnergizedBonus = ProcessSpec.bIsFamilySpecialist
			&& Memory.ConsecutiveSameFamilyActionCount >= 2;
		const bool bOverloaded = HasFamilyState(
			Resource.ActiveFamilyStateFlags,
			ESRResourceFamilyState::Overloaded)
			|| Memory.ConsecutiveSameFamilyActionCount
				>= FMath::Max(1, Rules.PlasmaOverloadedAmplificationThreshold);
		SetFamilyState(
			Resource.ActiveFamilyStateFlags,
			ESRResourceFamilyState::Energized,
			ProcessSpec.bIsFamilySpecialist);
		SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Overloaded, bOverloaded);
		if (bReceivesEnergizedBonus)
		{
			Result.FamilyEnergyDelta += FMath::Max(0.0, Rules.PlasmaEnergizedEnergyBonus);
		}
		if (bOverloaded)
		{
			Result.FamilyEnergyDelta -= FMath::Max(0.0, Rules.PlasmaOverloadedEnergyPenalty);
		}
	}

	void ApplyVoidRules(
		FSRResourceInstance& Resource,
		const FSRResourceProcessingMemory& PreviousMemory,
		const FSRResourceProcessSpec& ProcessSpec,
		const FSRResourceProcessingRules& Rules,
		FSRResourceProcessResult& Result)
	{
		FSRResourceProcessingMemory& Memory = Resource.ProcessingMemory;
		UpdateArchetypeMemory(Memory, PreviousMemory, ProcessSpec.ProcessArchetype, false);
		Memory.LastTemperature = ESRResourceProcessTemperatureState::None;
		Memory.LastFamilyAction = Result.EffectiveFamilyAction;
		Memory.ConsecutiveSameFamilyActionCount = 0;

		if (Result.EffectiveFamilyAction == ESRResourceFamilyAction::VoidSacrifice)
		{
			const double ActualSacrifice = FMath::Max(0.0, -Result.FacilityEnergyDelta);
			Memory.GeneralProcessesSinceReset = 0;
			Memory.StoredFamilyMagnitude = ActualSacrifice;
			SetFamilyState(
				Resource.ActiveFamilyStateFlags,
				ESRResourceFamilyState::Echoing,
				ActualSacrifice > UE_DOUBLE_SMALL_NUMBER);
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Collapsed, false);
			return;
		}

		if (Result.EffectiveFamilyAction != ESRResourceFamilyAction::EnergyGain)
		{
			return;
		}

		Memory.GeneralProcessesSinceReset = IncrementCounter(PreviousMemory.GeneralProcessesSinceReset);
		const bool bConsumesEcho = ProcessSpec.bIsFamilySpecialist
			&& HasFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Echoing);
		if (bConsumesEcho)
		{
			Result.FamilyEnergyDelta += FMath::Min(
				FMath::Max(0.0, Rules.VoidMaximumEchoEnergyBonus),
				FMath::Max(0.0, PreviousMemory.StoredFamilyMagnitude)
					* FMath::Max(0.0, Rules.VoidEchoEnergyMultiplier));
		}
		if (bConsumesEcho)
		{
			Memory.StoredFamilyMagnitude = 0.0;
			SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Echoing, false);
		}

		const bool bCollapsed = HasFamilyState(
			Resource.ActiveFamilyStateFlags,
			ESRResourceFamilyState::Collapsed)
			|| Memory.GeneralProcessesSinceReset >= FMath::Max(1, Rules.VoidCollapsedGainThreshold);
		SetFamilyState(Resource.ActiveFamilyStateFlags, ESRResourceFamilyState::Collapsed, bCollapsed);
		if (bCollapsed)
		{
			Result.FamilyEnergyDelta -= FMath::Max(0.0, Rules.VoidCollapsedEnergyPenalty);
		}
	}

	FSRResourceProcessResult MakeFailure(
		const FSRResourceInstance& InputResource,
		const FSRResourceProcessSpec& ProcessSpec,
		ESRResourceProcessOutcome Outcome,
		const TCHAR* FailureReason)
	{
		FSRResourceProcessResult Result;
		Result.Outcome = Outcome;
		Result.FailureReason = FailureReason;
		Result.OutputResource = InputResource;
		Result.InputEnergy = InputResource.CurrentEnergy;
		Result.RequestedFacilityEnergyDelta = ProcessSpec.FacilityEnergyDelta;
		Result.OutputEnergy = InputResource.CurrentEnergy;
		Result.UnclampedOutputEnergy = InputResource.CurrentEnergy;
		return Result;
	}
}

FSRResourceProcessResult FSRResourceProcessingKernel::Evaluate(
	const FSRResourceInstance& InputResource,
	const FSRResourceProcessSpec& ProcessSpec,
	const FSRResourceProcessingRules& Rules)
{
	if (InputResource.ResourceId.IsNone())
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidResource, TEXT("ResourceId is missing."));
	}
	if (InputResource.ResourceSchemaVersion != StarRovers::Resources::CurrentResourceSchemaVersion)
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::UnsupportedSchema, TEXT("Resource schema must be upgraded before V2 processing."));
	}
	if (InputResource.ResourceClass == ESRResourceClass::StellarFuel)
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::TerminalResource, TEXT("Stellar Fuel is a terminal resource and cannot be processed again."));
	}
	if (InputResource.ResourceClass != ESRResourceClass::Card)
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::UnsupportedResourceClass, TEXT("The Family processing kernel accepts Card resources only."));
	}
	if (InputResource.Family == ESRResourceFamily::None)
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::MissingFamily, TEXT("Card resource has no Family."));
	}
	const int32 AllowedFamilyStateMask = GetPositiveFamilyStateMask(InputResource.Family)
		| GetNegativeFamilyStateMask(InputResource.Family);
	if ((InputResource.ActiveFamilyStateFlags & ~AllowedFamilyStateMask) != 0)
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidResource, TEXT("Resource contains Family State flags owned by another Family."));
	}
	if (ProcessSpec.ProcessArchetype.IsNone())
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::MissingProcessArchetype, TEXT("Process Archetype is required for deterministic history."));
	}
	if (InputResource.Family == ESRResourceFamily::Metal
		&& ProcessSpec.Temperature == ESRResourceProcessTemperatureState::None)
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidTemperature, TEXT("Metal processing requires an explicit temperature context."));
	}
	if (!FMath::IsFinite(InputResource.CurrentEnergy)
		|| !FMath::IsFinite(ProcessSpec.FacilityEnergyDelta)
		|| !AreRuleEnergyValuesFinite(Rules)
		|| (Rules.bClampCurrentEnergyAtZero && InputResource.CurrentEnergy < 0.0))
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidEnergy, TEXT("Processing energy values must be finite and satisfy the active clamp invariant."));
	}

	const ESRResourceFamilyAction EffectiveFamilyAction = ResolveEffectiveFamilyAction(
		InputResource.Family,
		ProcessSpec);
	if (!ProcessSpec.bIsFamilySpecialist
		&& ProcessSpec.FamilyAction != ESRResourceFamilyAction::None)
	{
		return MakeFailure(
			InputResource,
			ProcessSpec,
			ESRResourceProcessOutcome::InvalidFamilyAction,
			TEXT("Universal Bridge processing cannot author a Family-specific action."));
	}
	if (!IsFamilyActionAllowed(InputResource.Family, EffectiveFamilyAction))
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidFamilyAction, TEXT("Family Action is not valid for this resource Family."));
	}
	if ((EffectiveFamilyAction == ESRResourceFamilyAction::Amplification
			|| EffectiveFamilyAction == ESRResourceFamilyAction::EnergyGain)
		&& ProcessSpec.FacilityEnergyDelta <= 0.0)
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidEnergy, TEXT("Amplification and Energy Gain require a positive Facility Energy Delta."));
	}
	if (EffectiveFamilyAction == ESRResourceFamilyAction::VoidSacrifice
		&& ProcessSpec.FacilityEnergyDelta > 0.0)
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidEnergy, TEXT("Void Sacrifice cannot request a positive Facility Energy Delta."));
	}
	if (EffectiveFamilyAction == ESRResourceFamilyAction::Anneal
		&& !FMath::IsNearlyZero(ProcessSpec.FacilityEnergyDelta))
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidEnergy, TEXT("Anneal must be an explicit zero-Energy recovery action."));
	}

	FSRResourceProcessResult Result;
	Result.Outcome = ESRResourceProcessOutcome::Success;
	Result.OutputResource = InputResource;
	StarRovers::Resources::EnsureResourceSeedEnergySnapshot(Result.OutputResource);
	Result.EffectiveFamilyAction = EffectiveFamilyAction;
	Result.InputEnergy = InputResource.CurrentEnergy;
	Result.RequestedFacilityEnergyDelta = ProcessSpec.FacilityEnergyDelta;
	Result.FacilityEnergyDelta = ProcessSpec.FacilityEnergyDelta;
	Result.bProcessArchetypeChanged = !InputResource.ProcessingMemory.LastProcessArchetype.IsNone()
		&& InputResource.ProcessingMemory.LastProcessArchetype != ProcessSpec.ProcessArchetype;
	Result.bFamilyActionChanged = InputResource.ProcessingMemory.LastFamilyAction != EffectiveFamilyAction;

	if (EffectiveFamilyAction == ESRResourceFamilyAction::VoidSacrifice)
	{
		const double EnergyAfterSacrifice = FMath::Max(
			0.0,
			InputResource.CurrentEnergy + ProcessSpec.FacilityEnergyDelta);
		Result.FacilityEnergyDelta = EnergyAfterSacrifice - InputResource.CurrentEnergy;
	}

	const FSRResourceProcessingMemory PreviousMemory = InputResource.ProcessingMemory;
	switch (InputResource.Family)
	{
	case ESRResourceFamily::Metal:
		ApplyMetalRules(Result.OutputResource, PreviousMemory, ProcessSpec, Rules, Result);
		break;
	case ESRResourceFamily::Crystal:
		ApplyCrystalRules(Result.OutputResource, PreviousMemory, ProcessSpec, Rules, Result);
		break;
	case ESRResourceFamily::Organic:
		ApplyOrganicRules(Result.OutputResource, PreviousMemory, ProcessSpec, Rules, Result);
		break;
	case ESRResourceFamily::Plasma:
		ApplyPlasmaRules(Result.OutputResource, PreviousMemory, ProcessSpec, Rules, Result);
		break;
	case ESRResourceFamily::Void:
		ApplyVoidRules(Result.OutputResource, PreviousMemory, ProcessSpec, Rules, Result);
		break;
	case ESRResourceFamily::None:
	default:
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::MissingFamily, TEXT("Card resource has no supported Family."));
	}

	const int32 PreviousStateFlags = InputResource.ActiveFamilyStateFlags;
	const int32 OutputStateFlags = Result.OutputResource.ActiveFamilyStateFlags;
	Result.ActivatedFamilyStateFlags = OutputStateFlags & ~PreviousStateFlags;
	Result.ClearedFamilyStateFlags = PreviousStateFlags & ~OutputStateFlags;
	Result.bPositiveFamilyStateActivated =
		(Result.ActivatedFamilyStateFlags & GetPositiveFamilyStateMask(InputResource.Family)) != 0;
	Result.bNegativeFamilyStateCleared =
		(Result.ClearedFamilyStateFlags & GetNegativeFamilyStateMask(InputResource.Family)) != 0;

	const double PreTagUnclampedEnergy = Result.InputEnergy
		+ Result.FacilityEnergyDelta
		+ Result.FamilyEnergyDelta;
	if (!FMath::IsFinite(PreTagUnclampedEnergy))
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidEnergy, TEXT("Additive processing overflowed before Process Tag evaluation."));
	}
	const double PreTagOutputEnergy = Rules.bClampCurrentEnergyAtZero
		? FMath::Max(0.0, PreTagUnclampedEnergy)
		: PreTagUnclampedEnergy;
	Result.bPreTagEnergyChanged = !FMath::IsNearlyEqual(Result.InputEnergy, PreTagOutputEnergy);

	FSRProcessTagTriggerContextV2 TagTriggerContext;
	TagTriggerContext.bPositiveFamilyStateActivated = Result.bPositiveFamilyStateActivated;
	TagTriggerContext.bNegativeFamilyStateCleared = Result.bNegativeFamilyStateCleared;
	TagTriggerContext.bProcessArchetypeChanged = Result.bProcessArchetypeChanged;
	TagTriggerContext.bPreTagEnergyChanged = Result.bPreTagEnergyChanged;
	TagTriggerContext.ProcessingBodyId = ProcessSpec.ProcessingBodyId;
	const FSRProcessTagEvaluationV2 TagEvaluation = FSRResourceProcessTagEvaluator::Evaluate(
		InputResource,
		TagTriggerContext);
	if (!TagEvaluation.bValid)
	{
		return MakeFailure(
			InputResource,
			ProcessSpec,
			ESRResourceProcessOutcome::InvalidProcessTag,
			TagEvaluation.FailureReason.IsEmpty()
				? TEXT("Primed Process Tag could not be evaluated.")
				: *TagEvaluation.FailureReason);
	}
	Result.EvaluatedProcessTagId = TagEvaluation.EvaluatedTagId;
	Result.bProcessTagTriggered = TagEvaluation.bTriggered;
	Result.ProcessTagEnergyDelta = TagEvaluation.EnergyDelta;
	Result.OutputResource.ProcessTagSlot = TagEvaluation.OutputSlot;

	Result.UnclampedOutputEnergy = Result.InputEnergy
		+ Result.FacilityEnergyDelta
		+ Result.FamilyEnergyDelta
		+ Result.ProcessTagEnergyDelta;
	if (!FMath::IsFinite(Result.UnclampedOutputEnergy))
	{
		return MakeFailure(InputResource, ProcessSpec, ESRResourceProcessOutcome::InvalidEnergy, TEXT("Additive processing overflowed to a non-finite Energy value."));
	}
	Result.OutputEnergy = Rules.bClampCurrentEnergyAtZero
		? FMath::Max(0.0, Result.UnclampedOutputEnergy)
		: Result.UnclampedOutputEnergy;
	Result.ClampEnergyDelta = Result.OutputEnergy - Result.UnclampedOutputEnergy;
	Result.bEnergyClamped = !FMath::IsNearlyZero(Result.ClampEnergyDelta);
	Result.AppliedEnergyDelta = Result.OutputEnergy - Result.InputEnergy;
	Result.OutputResource.CurrentEnergy = Result.OutputEnergy;

	FSRResourceProcessingMemory& OutputMemory = Result.OutputResource.ProcessingMemory;
	OutputMemory.ProcessCount = IncrementCounter(PreviousMemory.ProcessCount);
	OutputMemory.EnergyChangeCount = FMath::IsNearlyEqual(Result.InputEnergy, Result.OutputEnergy)
		? FMath::Max(0, PreviousMemory.EnergyChangeCount)
		: IncrementCounter(PreviousMemory.EnergyChangeCount);
	OutputMemory.TransitCountAtLastEnergyChange = FMath::IsNearlyEqual(Result.InputEnergy, Result.OutputEnergy)
		? FMath::Max(0, PreviousMemory.TransitCountAtLastEnergyChange)
		: FMath::Max(0, InputResource.LogisticsMetadata.TransitCount);
	StarRovers::Resources::RecordResourceProcessedOnBody(Result.OutputResource, ProcessSpec.ProcessingBodyId);
	StarRovers::Resources::SynchronizeResourceV2RuntimeStateToLegacy(Result.OutputResource);
	return Result;
}
