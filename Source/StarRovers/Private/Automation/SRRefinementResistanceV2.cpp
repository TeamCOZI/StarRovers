#include "Automation/SRRefinementResistanceV2.h"

#include "Automation/SRResourceInstanceOperations.h"

bool FSRRefinementResistanceV2::TryResolveSeedEnergy(
	const FSRResourceInstance& ResourceInstance,
	double& OutSeedEnergy)
{
	return StarRovers::Resources::TryResolveResourceSeedEnergy(ResourceInstance, OutSeedEnergy);
}

FSRRefinementResistanceResultV2 FSRRefinementResistanceV2::Evaluate(
	const FSRResourceInstance& ResourceInstance,
	float BaseProcessSeconds,
	double EnergyScale)
{
	FSRRefinementResistanceResultV2 Result = MakeInactive(BaseProcessSeconds);
	Result.EnergyScale = FMath::Max(0.01, EnergyScale);
	if (ResourceInstance.ResourceClass != ESRResourceClass::Card
		|| !FMath::IsFinite(ResourceInstance.CurrentEnergy)
		|| !TryResolveSeedEnergy(ResourceInstance, Result.SeedEnergy))
	{
		return Result;
	}

	Result.bApplied = true;
	Result.bSeedEnergyResolved = true;
	Result.RefinementEnergy = FMath::Max(0.0, ResourceInstance.CurrentEnergy - Result.SeedEnergy);
	Result.CycleMultiplier = 1.0 + Result.RefinementEnergy / Result.EnergyScale;
	const double EffectiveSeconds = static_cast<double>(Result.BaseProcessSeconds)
		* Result.CycleMultiplier;
	Result.EffectiveProcessSeconds = FMath::IsFinite(EffectiveSeconds)
		? static_cast<float>(FMath::Clamp(EffectiveSeconds, 0.01, static_cast<double>(MAX_flt) / 4.0))
		: MAX_flt / 4.0f;
	return Result;
}

FSRRefinementResistanceResultV2 FSRRefinementResistanceV2::MakeInactive(float BaseProcessSeconds)
{
	FSRRefinementResistanceResultV2 Result;
	Result.BaseProcessSeconds = FMath::Max(0.01f, BaseProcessSeconds);
	Result.EffectiveProcessSeconds = Result.BaseProcessSeconds;
	return Result;
}
