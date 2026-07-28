#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"

struct STARROVERS_API FSRRefinementResistanceResultV2
{
	bool bApplied = false;
	bool bSeedEnergyResolved = false;
	double SeedEnergy = 0.0;
	double RefinementEnergy = 0.0;
	double EnergyScale = 40.0;
	double CycleMultiplier = 1.0;
	float BaseProcessSeconds = 0.01f;
	float EffectiveProcessSeconds = 0.01f;
};

class STARROVERS_API FSRRefinementResistanceV2 final
{
public:
	static bool TryResolveSeedEnergy(
		const FSRResourceInstance& ResourceInstance,
		double& OutSeedEnergy);

	// Pure and preview-safe. Eligibility is owned by the calling facility route;
	// this function only evaluates the shared Energy-to-cycle formula.
	static FSRRefinementResistanceResultV2 Evaluate(
		const FSRResourceInstance& ResourceInstance,
		float BaseProcessSeconds,
		double EnergyScale);

	static FSRRefinementResistanceResultV2 MakeInactive(float BaseProcessSeconds);
};
