#include "Logistics/SRConditionedTransitV2.h"

#include "Automation/SRResourceInstanceOperations.h"

namespace
{
	FSRConditionedTransitModuleRulesV2 MakeRules(
		ESRConditionedTransitModuleV2 Module,
		const FText& DisplayName,
		const TCHAR* UnlockModuleId,
		ESRResourceFamily CompatibleFamily,
		const TCHAR* ProcessArchetype,
		ESRResourceProcessTemperatureState Temperature,
		ESRResourceFamilyAction FamilyAction,
		double BaseEnergyDelta,
		float BaseConditioningSeconds,
		const FText& PreviewText)
	{
		FSRConditionedTransitModuleRulesV2 Rules;
		Rules.Module = Module;
		Rules.DisplayName = DisplayName;
		Rules.UnlockModuleId = FName(UnlockModuleId);
		Rules.CompatibleFamily = CompatibleFamily;
		Rules.ProcessArchetype = FName(ProcessArchetype);
		Rules.Temperature = Temperature;
		Rules.FamilyAction = FamilyAction;
		Rules.BaseEnergyDelta = BaseEnergyDelta;
		Rules.BaseConditioningSeconds = BaseConditioningSeconds;
		Rules.PreviewText = PreviewText;
		return Rules;
	}
}

FSRConditionedTransitModuleRulesV2 FSRConditionedTransitV2::GetModuleRules(
	ESRConditionedTransitModuleV2 Module)
{
	switch (Module)
	{
	case ESRConditionedTransitModuleV2::CryogenicHold:
		return MakeRules(
			Module,
			NSLOCTEXT("StarRoversConditionedTransit", "CryogenicHold", "Cryogenic Hold"),
			TEXT("CryogenicHold"),
			ESRResourceFamily::Metal,
			TEXT("CryogenicTransit"),
			ESRResourceProcessTemperatureState::Cold,
			ESRResourceFamilyAction::None,
			3.0,
			6.0f,
			NSLOCTEXT("StarRoversConditionedTransit", "CryogenicHoldPreview", "Dock conditioning: 6s base, then Metal receives one Cold process with base Energy +3."));
	case ESRConditionedTransitModuleV2::BioCultureHold:
		return MakeRules(
			Module,
			NSLOCTEXT("StarRoversConditionedTransit", "BioCultureHold", "Bio-Culture Hold"),
			TEXT("BioCultureHold"),
			ESRResourceFamily::Organic,
			TEXT("BioCultureTransit"),
			ESRResourceProcessTemperatureState::Normal,
			ESRResourceFamilyAction::Growth,
			0.0,
			8.0f,
			NSLOCTEXT("StarRoversConditionedTransit", "BioCultureHoldPreview", "Dock conditioning: 8s base, then Organic completes one Growth cycle."));
	case ESRConditionedTransitModuleV2::GroundingHold:
		return MakeRules(
			Module,
			NSLOCTEXT("StarRoversConditionedTransit", "GroundingHold", "Grounding Hold"),
			TEXT("GroundingHold"),
			ESRResourceFamily::Plasma,
			TEXT("GroundingTransit"),
			ESRResourceProcessTemperatureState::Normal,
			ESRResourceFamilyAction::Discharge,
			1.0,
			4.0f,
			NSLOCTEXT("StarRoversConditionedTransit", "GroundingHoldPreview", "Dock conditioning: 4s base, then Plasma receives one Discharge process with base Energy +1."));
	case ESRConditionedTransitModuleV2::None:
	default:
		{
			FSRConditionedTransitModuleRulesV2 Rules;
			Rules.DisplayName = NSLOCTEXT("StarRoversConditionedTransit", "NoModule", "No Conditioned Module");
			Rules.PreviewText = NSLOCTEXT("StarRoversConditionedTransit", "NoModulePreview", "Transit is state-neutral; only logistics history changes.");
			return Rules;
		}
	}
}

void FSRConditionedTransitV2::GetConditionedModules(
	TArray<ESRConditionedTransitModuleV2>& OutModules)
{
	OutModules = {
		ESRConditionedTransitModuleV2::CryogenicHold,
		ESRConditionedTransitModuleV2::BioCultureHold,
		ESRConditionedTransitModuleV2::GroundingHold,
	};
}

bool FSRConditionedTransitV2::IsKnownModuleId(FName ModuleId)
{
	TArray<ESRConditionedTransitModuleV2> Modules;
	GetConditionedModules(Modules);
	for (const ESRConditionedTransitModuleV2 Module : Modules)
	{
		if (GetModuleRules(Module).UnlockModuleId == ModuleId)
		{
			return true;
		}
	}
	return false;
}

bool FSRConditionedTransitV2::IsCargoCompatible(
	ESRConditionedTransitModuleV2 Module,
	const FSRResourceInstance& Cargo)
{
	const FSRConditionedTransitModuleRulesV2 Rules = GetModuleRules(Module);
	return !Rules.IsConditionedModule()
		|| (Cargo.ResourceClass == ESRResourceClass::Card
			&& Cargo.Family == Rules.CompatibleFamily);
}

FSRConditioningDwellResultV2 FSRConditionedTransitV2::EvaluateConditioningDwell(
	ESRSpaceLogisticsRouteProfileV2 RouteProfile,
	ESRConditionedTransitModuleV2 Module,
	const FSRResourceInstance& Cargo,
	double RefinementEnergyScale)
{
	FSRConditioningDwellResultV2 Result;
	const FSRConditionedTransitModuleRulesV2 Rules = GetModuleRules(Module);
	Result.RefinementResistance = FSRRefinementResistanceV2::MakeInactive(
		FMath::Max(0.01f, Rules.BaseConditioningSeconds));
	if (RouteProfile != ESRSpaceLogisticsRouteProfileV2::ConditionedHold
		|| !Rules.IsConditionedModule()
		|| Rules.BaseConditioningSeconds <= 0.0f
		|| Cargo.ResourceId.IsNone()
		|| Cargo.StackCount <= 0
		|| !IsCargoCompatible(Module, Cargo))
	{
		return Result;
	}

	Result.bRequired = true;
	Result.RefinementResistance = FSRRefinementResistanceV2::Evaluate(
		Cargo,
		Rules.BaseConditioningSeconds,
		RefinementEnergyScale);
	return Result;
}

bool FSRConditionedTransitV2::TryBeginConditioningDwell(
	FSRSpaceLogisticsHubRoute& Route,
	ESRSpaceLogisticsHubRouteDockSide ArrivalDockSide,
	bool bResourceV2RulesActive,
	double RefinementEnergyScale)
{
	if (!bResourceV2RulesActive)
	{
		ClearConditioningDwell(Route);
		return false;
	}

	// Capture a legacy/dynamic Card's seed before resolving the immutable leg time.
	StarRovers::Resources::EnsureResourceSeedEnergySnapshot(Route.Cargo);
	const FSRConditioningDwellResultV2 Evaluation = EvaluateConditioningDwell(
		Route.RouteProfile,
		Route.ConditionedTransitModule,
		Route.Cargo,
		RefinementEnergyScale);
	if (!Evaluation.bRequired)
	{
		ClearConditioningDwell(Route);
		return false;
	}

	Route.CurrentDockSide = ArrivalDockSide;
	Route.ConditioningDurationSeconds = FMath::Max(
		0.01f,
		Evaluation.RefinementResistance.EffectiveProcessSeconds);
	Route.ConditioningProgressSeconds = 0.0f;
	Route.Phase = ArrivalDockSide == ESRSpaceLogisticsHubRouteDockSide::Destination
		? ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination
		: ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource;
	return true;
}

bool FSRConditionedTransitV2::AdvanceConditioningDwell(
	FSRSpaceLogisticsHubRoute& Route,
	float DeltaTime)
{
	if (Route.Phase != ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination
		&& Route.Phase != ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource)
	{
		return false;
	}

	const float DurationSeconds = FMath::Max(0.01f, Route.ConditioningDurationSeconds);
	Route.ConditioningProgressSeconds = FMath::Min(
		DurationSeconds,
		FMath::Max(0.0f, Route.ConditioningProgressSeconds) + FMath::Max(0.0f, DeltaTime));
	return Route.ConditioningProgressSeconds >= DurationSeconds;
}

void FSRConditionedTransitV2::ClearConditioningDwell(FSRSpaceLogisticsHubRoute& Route)
{
	Route.ConditioningDurationSeconds = 0.0f;
	Route.ConditioningProgressSeconds = 0.0f;
}

FSRConditionedTransitResultV2 FSRConditionedTransitV2::EvaluateArrival(
	const FSRResourceInstance& InputResource,
	ESRSpaceLogisticsRouteProfileV2 RouteProfile,
	ESRConditionedTransitModuleV2 Module,
	FName SourceBodyId,
	FName DestinationBodyId,
	bool bModuleUnlocked,
	const FSRResourceProcessingRules& ProcessingRules)
{
	FSRConditionedTransitResultV2 Result;
	Result.OutputResource = InputResource;
	if (SourceBodyId.IsNone() || DestinationBodyId.IsNone())
	{
		Result.Outcome = ESRConditionedTransitOutcomeV2::InvalidTransitEndpoints;
		Result.FailureReason = TEXT("Conditioned transit requires stable source and destination body ids.");
		return Result;
	}

	const int32 PreviousTransitCount = Result.OutputResource.LogisticsMetadata.TransitCount;
	StarRovers::Resources::RecordResourceTransit(Result.OutputResource, SourceBodyId, DestinationBodyId);
	Result.bTransitRecorded = Result.OutputResource.LogisticsMetadata.TransitCount > PreviousTransitCount;

	if (Module == ESRConditionedTransitModuleV2::None)
	{
		return Result;
	}

	const FSRConditionedTransitModuleRulesV2 ModuleRules = GetModuleRules(Module);
	if (RouteProfile != ESRSpaceLogisticsRouteProfileV2::ConditionedHold
		|| !ModuleRules.IsConditionedModule())
	{
		Result.Outcome = ESRConditionedTransitOutcomeV2::InvalidModuleProfile;
		Result.FailureReason = TEXT("A concrete conditioned module requires the Conditioned Hold route profile.");
		return Result;
	}
	if (!bModuleUnlocked)
	{
		Result.Outcome = ESRConditionedTransitOutcomeV2::LockedModule;
		Result.FailureReason = TEXT("The Augment Package for this conditioned module is not unlocked.");
		return Result;
	}
	if (!IsCargoCompatible(Module, Result.OutputResource))
	{
		Result.Outcome = ESRConditionedTransitOutcomeV2::IncompatibleCargo;
		Result.FailureReason = FString::Printf(
			TEXT("%s only accepts %s Card cargo."),
			*ModuleRules.DisplayName.ToString(),
			*StaticEnum<ESRResourceFamily>()->GetDisplayNameTextByValue(
				static_cast<int64>(ModuleRules.CompatibleFamily)).ToString());
		return Result;
	}

	FSRResourceProcessSpec ProcessSpec;
	ProcessSpec.ProcessArchetype = ModuleRules.ProcessArchetype;
	ProcessSpec.Temperature = ModuleRules.Temperature;
	ProcessSpec.FamilyAction = ModuleRules.FamilyAction;
	ProcessSpec.bIsFamilySpecialist = true;
	ProcessSpec.FacilityEnergyDelta = ModuleRules.BaseEnergyDelta;
	ProcessSpec.ProcessingBodyId = DestinationBodyId;
	Result.ProcessResult = FSRResourceProcessingKernel::Evaluate(
		Result.OutputResource,
		ProcessSpec,
		ProcessingRules);
	if (!Result.ProcessResult.IsSuccess())
	{
		Result.Outcome = ESRConditionedTransitOutcomeV2::ProcessFailed;
		Result.FailureReason = Result.ProcessResult.FailureReason;
		return Result;
	}

	Result.OutputResource = Result.ProcessResult.OutputResource;
	Result.Outcome = ESRConditionedTransitOutcomeV2::Applied;
	Result.bProcessApplied = true;
	return Result;
}
