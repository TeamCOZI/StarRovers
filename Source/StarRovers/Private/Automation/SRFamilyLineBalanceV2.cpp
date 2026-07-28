#include "Automation/SRFamilyLineBalanceV2.h"

#include "Automation/SRRefinementResistanceV2.h"

namespace
{
	ESRResourceProcessTemperatureState ConvertTemperature(
		ESRFacilityTemperatureState Temperature)
	{
		switch (Temperature)
		{
		case ESRFacilityTemperatureState::Frozen:
			return ESRResourceProcessTemperatureState::Frozen;
		case ESRFacilityTemperatureState::Cold:
			return ESRResourceProcessTemperatureState::Cold;
		case ESRFacilityTemperatureState::Hot:
			return ESRResourceProcessTemperatureState::Hot;
		case ESRFacilityTemperatureState::Overheated:
			return ESRResourceProcessTemperatureState::Overheated;
		case ESRFacilityTemperatureState::Normal:
		default:
			return ESRResourceProcessTemperatureState::Normal;
		}
	}

	FSRFamilyLineBalanceStepV2 MakeStep(
		ESRFacilityContentPresetV2 Preset,
		ESRFacilityTemperatureState Temperature = ESRFacilityTemperatureState::Normal)
	{
		FSRFamilyLineBalanceStepV2 Step;
		Step.FacilityPreset = Preset;
		Step.Temperature = Temperature;
		return Step;
	}
}

double FSRFamilyLineBalanceResultV2::GetEnergyPerEffectiveSecond() const
{
	return bValid && TotalEffectiveSeconds > UE_DOUBLE_SMALL_NUMBER
		? EnergyGain / TotalEffectiveSeconds
		: 0.0;
}

double FSRFamilyLineBalanceResultV2::GetEnergyPerLoadSecond() const
{
	return bValid && TotalLoadSeconds > UE_DOUBLE_SMALL_NUMBER
		? EnergyGain / TotalLoadSeconds
		: 0.0;
}

bool FSRFamilyLineBalanceV2::BuildReferenceCycle(
	ESRResourceFamily Family,
	ESRResourceContentPresetV2& OutResourcePreset,
	TArray<FSRFamilyLineBalanceStepV2>& OutSteps)
{
	OutResourcePreset = ESRResourceContentPresetV2::Custom;
	OutSteps.Reset();
	switch (Family)
	{
	case ESRResourceFamily::Metal:
		OutResourcePreset = ESRResourceContentPresetV2::HeliosIron;
		OutSteps = {
			MakeStep(ESRFacilityContentPresetV2::InductionForge, ESRFacilityTemperatureState::Hot),
			MakeStep(ESRFacilityContentPresetV2::CryoPress, ESRFacilityTemperatureState::Cold),
			MakeStep(ESRFacilityContentPresetV2::AnnealingChamber),
		};
		return true;

	case ESRResourceFamily::Crystal:
		OutResourcePreset = ESRResourceContentPresetV2::EchoQuartz;
		OutSteps = {
			MakeStep(ESRFacilityContentPresetV2::ResonanceMill),
			MakeStep(ESRFacilityContentPresetV2::ResonanceMill),
			MakeStep(ESRFacilityContentPresetV2::ResonanceMill),
			MakeStep(ESRFacilityContentPresetV2::FacetShifter),
		};
		return true;

	case ESRResourceFamily::Organic:
		OutResourcePreset = ESRResourceContentPresetV2::VerdantSpore;
		OutSteps = {
			MakeStep(ESRFacilityContentPresetV2::GrowthVat),
			MakeStep(ESRFacilityContentPresetV2::EnzymeLoom),
		};
		return true;

	case ESRResourceFamily::Plasma:
		OutResourcePreset = ESRResourceContentPresetV2::AuroraPlasma;
		OutSteps = {
			MakeStep(ESRFacilityContentPresetV2::ArcAmplifier),
			MakeStep(ESRFacilityContentPresetV2::ArcAmplifier),
			MakeStep(ESRFacilityContentPresetV2::GroundingCoil),
		};
		return true;

	case ESRResourceFamily::Void:
		OutResourcePreset = ESRResourceContentPresetV2::NullPearl;
		OutSteps = {
			MakeStep(ESRFacilityContentPresetV2::NullSink),
			MakeStep(ESRFacilityContentPresetV2::EchoChamber),
		};
		return true;

	case ESRResourceFamily::None:
	default:
		return false;
	}
}

FSRFamilyLineBalanceResultV2 FSRFamilyLineBalanceV2::Evaluate(
	ESRResourceContentPresetV2 ResourcePreset,
	const TArray<FSRFamilyLineBalanceStepV2>& Steps,
	double RefinementResistanceEnergyScale,
	const FSRResourceProcessingRules& Rules)
{
	FSRFamilyLineBalanceResultV2 Result;
	Result.ResourcePreset = ResourcePreset;
	if (Steps.IsEmpty())
	{
		Result.FailureReason = TEXT("A Family-line balance probe requires at least one processing step.");
		return Result;
	}

	FSRResourceInstance Resource;
	if (!FSRResourceSystemContent::MakeReferenceResourceInstance(
		ResourcePreset,
		FName(TEXT("FamilyBalanceProbe")),
		Resource)
		|| Resource.ResourceClass != ESRResourceClass::Card
		|| Resource.Family == ESRResourceFamily::None)
	{
		Result.FailureReason = TEXT("The selected balance resource is not a reference Family Card.");
		return Result;
	}

	Result.Family = Resource.Family;
	Result.InputEnergy = Resource.CurrentEnergy;
	for (const FSRFamilyLineBalanceStepV2& Step : Steps)
	{
		FSRFacilityContentDefinitionV2 Definition;
		if (!FSRResourceSystemContent::TryGetFacilityDefinition(
			Step.FacilityPreset,
			Definition)
			|| Definition.OperationKind != ESRFacilityOperationKind::Process
			|| Definition.ProcessRole != ESRFacilityProcessRoleV2::FamilyProcess)
		{
			Result.FailureReason = TEXT("The balance sequence contains a non-Family processing facility.");
			return Result;
		}
		if (Definition.AcceptedFamily != ESRResourceFamily::None
			&& Definition.AcceptedFamily != Resource.Family)
		{
			Result.FailureReason = TEXT("The balance sequence contains a specialist for another Family.");
			return Result;
		}

		const FSRRefinementResistanceResultV2 Resistance =
			FMath::IsNearlyZero(Definition.FacilityEnergyDelta)
				? FSRRefinementResistanceV2::MakeInactive(Definition.CycleSeconds)
				: FSRRefinementResistanceV2::Evaluate(
					Resource,
					Definition.CycleSeconds,
					RefinementResistanceEnergyScale);
		Result.TotalBaseSeconds += Resistance.BaseProcessSeconds;
		Result.TotalEffectiveSeconds += Resistance.EffectiveProcessSeconds;
		Result.TotalLoadSeconds += Resistance.EffectiveProcessSeconds
			* static_cast<double>(FMath::Max(0, Definition.OperationalLoad));
		Result.CommittedOperationalLoad += FMath::Max(0, Definition.OperationalLoad);

		FSRResourceProcessSpec ProcessSpec;
		ProcessSpec.ProcessArchetype = Definition.ProcessArchetype;
		ProcessSpec.Temperature = ConvertTemperature(Step.Temperature);
		ProcessSpec.FamilyAction = Definition.FamilyAction;
		ProcessSpec.bIsFamilySpecialist = Definition.AcceptedFamily != ESRResourceFamily::None;
		ProcessSpec.FacilityEnergyDelta = Definition.FacilityEnergyDelta;
		ProcessSpec.ProcessingBodyId = FName(TEXT("FamilyBalanceProbe"));
		const FSRResourceProcessResult ProcessResult = FSRResourceProcessingKernel::Evaluate(
			Resource,
			ProcessSpec,
			Rules);
		if (!ProcessResult.IsSuccess())
		{
			Result.FailureReason = ProcessResult.FailureReason;
			return Result;
		}
		Resource = ProcessResult.OutputResource;
		++Result.StepCount;
	}

	Result.OutputResource = Resource;
	Result.OutputEnergy = Resource.CurrentEnergy;
	Result.EnergyGain = Result.OutputEnergy - Result.InputEnergy;
	Result.bValid = true;
	return Result;
}
