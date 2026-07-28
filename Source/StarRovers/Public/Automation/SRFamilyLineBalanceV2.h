#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceProcessingKernel.h"
#include "Automation/SRResourceSystemContent.h"

/** One physical processing step in a deterministic Family-line balance probe. */
struct STARROVERS_API FSRFamilyLineBalanceStepV2
{
	ESRFacilityContentPresetV2 FacilityPreset = ESRFacilityContentPresetV2::Custom;
	ESRFacilityTemperatureState Temperature = ESRFacilityTemperatureState::Normal;
};

/**
 * Single-card result used to compare Family identities without World, Tick, or
 * random state. Effective time includes Refinement Resistance.
 */
struct STARROVERS_API FSRFamilyLineBalanceResultV2
{
	bool bValid = false;
	FString FailureReason;
	ESRResourceFamily Family = ESRResourceFamily::None;
	ESRResourceContentPresetV2 ResourcePreset = ESRResourceContentPresetV2::Custom;
	FSRResourceInstance OutputResource;
	double InputEnergy = 0.0;
	double OutputEnergy = 0.0;
	double EnergyGain = 0.0;
	double TotalBaseSeconds = 0.0;
	double TotalEffectiveSeconds = 0.0;
	double TotalLoadSeconds = 0.0;
	int32 CommittedOperationalLoad = 0;
	int32 StepCount = 0;

	double GetEnergyPerEffectiveSecond() const;
	double GetEnergyPerLoadSecond() const;
};

/** Pure evaluator and canonical one-cycle definitions for balance regression. */
class STARROVERS_API FSRFamilyLineBalanceV2 final
{
public:
	static bool BuildReferenceCycle(
		ESRResourceFamily Family,
		ESRResourceContentPresetV2& OutResourcePreset,
		TArray<FSRFamilyLineBalanceStepV2>& OutSteps);

	static FSRFamilyLineBalanceResultV2 Evaluate(
		ESRResourceContentPresetV2 ResourcePreset,
		const TArray<FSRFamilyLineBalanceStepV2>& Steps,
		double RefinementResistanceEnergyScale = 40.0,
		const FSRResourceProcessingRules& Rules = FSRResourceProcessingRules());
};
