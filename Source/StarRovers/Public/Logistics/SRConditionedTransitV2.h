#pragma once

#include "CoreMinimal.h"
#include "Automation/SRRefinementResistanceV2.h"
#include "Automation/SRResourceProcessingKernel.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "SRConditionedTransitV2.generated.h"

UENUM(BlueprintType)
enum class ESRConditionedTransitOutcomeV2 : uint8
{
	StateNeutral UMETA(DisplayName = "State-Neutral Transit"),
	Applied UMETA(DisplayName = "Conditioned Process Applied"),
	InvalidTransitEndpoints UMETA(DisplayName = "Invalid Transit Endpoints"),
	InvalidModuleProfile UMETA(DisplayName = "Invalid Module Profile"),
	LockedModule UMETA(DisplayName = "Locked Module"),
	IncompatibleCargo UMETA(DisplayName = "Incompatible Cargo"),
	ProcessFailed UMETA(DisplayName = "Process Failed"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRConditionedTransitModuleRulesV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	ESRConditionedTransitModuleV2 Module = ESRConditionedTransitModuleV2::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	FName UnlockModuleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	ESRResourceFamily CompatibleFamily = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	FName ProcessArchetype = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	ESRResourceProcessTemperatureState Temperature = ESRResourceProcessTemperatureState::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	ESRResourceFamilyAction FamilyAction = ESRResourceFamilyAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	double BaseEnergyDelta = 0.0;

	// A conditioned arrival is a real process, not a free side effect of travel.
	// This base duration is multiplied by the shared refinement-resistance curve.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	float BaseConditioningSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	FText PreviewText;

	bool IsConditionedModule() const
	{
		return Module != ESRConditionedTransitModuleV2::None
			&& !UnlockModuleId.IsNone()
			&& CompatibleFamily != ESRResourceFamily::None
			&& !ProcessArchetype.IsNone();
	}
};

struct STARROVERS_API FSRConditioningDwellResultV2
{
	bool bRequired = false;
	FSRRefinementResistanceResultV2 RefinementResistance;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRConditionedTransitResultV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	ESRConditionedTransitOutcomeV2 Outcome = ESRConditionedTransitOutcomeV2::StateNeutral;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	FSRResourceInstance OutputResource;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	FSRResourceProcessResult ProcessResult;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	bool bTransitRecorded = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Space Logistics|Conditioned Transit")
	bool bProcessApplied = false;

	bool IsSuccessful() const
	{
		return Outcome == ESRConditionedTransitOutcomeV2::StateNeutral
			|| Outcome == ESRConditionedTransitOutcomeV2::Applied;
	}
};

class STARROVERS_API FSRConditionedTransitV2 final
{
public:
	static FSRConditionedTransitModuleRulesV2 GetModuleRules(ESRConditionedTransitModuleV2 Module);
	static void GetConditionedModules(TArray<ESRConditionedTransitModuleV2>& OutModules);
	static bool IsKnownModuleId(FName ModuleId);
	static bool IsCargoCompatible(ESRConditionedTransitModuleV2 Module, const FSRResourceInstance& Cargo);

	// Pure preview of the dock-side process time. The same Energy-to-time curve as
	// normal Family processing prevents short shuttle routes from becoming free processors.
	static FSRConditioningDwellResultV2 EvaluateConditioningDwell(
		ESRSpaceLogisticsRouteProfileV2 RouteProfile,
		ESRConditionedTransitModuleV2 Module,
		const FSRResourceInstance& Cargo,
		double RefinementEnergyScale = 40.0);

	// Route-state helpers are intentionally shared by runtime and automation tests.
	// TryBegin captures an immutable per-leg duration; Advance never applies the
	// arrival effect by itself, so the route processor retains exactly-once ownership.
	static bool TryBeginConditioningDwell(
		FSRSpaceLogisticsHubRoute& Route,
		ESRSpaceLogisticsHubRouteDockSide ArrivalDockSide,
		bool bResourceV2RulesActive,
		double RefinementEnergyScale = 40.0);
	static bool AdvanceConditioningDwell(FSRSpaceLogisticsHubRoute& Route, float DeltaTime);
	static void ClearConditioningDwell(FSRSpaceLogisticsHubRoute& Route);

	// Pure and preview-safe. It records one transit in the returned copy, then applies
	// at most one explicit Family process at the destination. InputResource is untouched.
	static FSRConditionedTransitResultV2 EvaluateArrival(
		const FSRResourceInstance& InputResource,
		ESRSpaceLogisticsRouteProfileV2 RouteProfile,
		ESRConditionedTransitModuleV2 Module,
		FName SourceBodyId,
		FName DestinationBodyId,
		bool bModuleUnlocked,
		const FSRResourceProcessingRules& ProcessingRules = FSRResourceProcessingRules());
};
