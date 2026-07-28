#pragma once

#include "CoreMinimal.h"
#include "Automation/SRStellarFuelBatchPlanner.h"
#include "UI/SRUITheme.h"

enum class ESRFacilityInspectorActivity : uint8
{
	Disabled,
	Blocked,
	WaitingForInput,
	Processing,
	WaitingForOutput,
	Throttled,
	Ready,
};

/** Runtime facts consumed by the Facility Inspector presentation layer. */
struct STARROVERS_API FSRFacilityInspectorPresentationInput
{
	bool bProcessEnabled = true;
	bool bCanOperate = true;
	bool bIsMiningFacility = false;
	bool bProcessing = false;
	bool bOutputBlocked = false;
	bool bPreviewResolved = false;
	bool bUsesFinalFuelFormula = false;
	bool bUsesStellarFuelBatch = false;
	ESRStellarFuelBatchStateV2 StellarFuelBatchState = ESRStellarFuelBatchStateV2::Empty;
	int32 StellarFuelValidCardCount = 0;
	int32 StellarFuelRequiredCardCount = StarRovers::StellarFuel::RequiredCardCount;
	int32 InputResourceCount = 0;
	int32 OutputResourceCount = 0;
	float ProgressRatio = 0.0f;
	float ProcessSeconds = 0.0f;
	float OperationalSpeedFactor = 1.0f;
	FString OperationReason;
	FString ProcessRuleSummary;
	FString FormulaSummary;
	FString StateTransitionSummary;
	FString StellarFuelBatchSummary;
	FString StellarFuelBatchDetail;
	bool bHasEnergyTransition = false;
	double InputEnergy = 0.0;
	double OutputEnergy = 0.0;
};

/** Player-facing, semantic view model for the Input -> Process -> Output Inspector. */
struct STARROVERS_API FSRFacilityInspectorPresentation
{
	ESRFacilityInspectorActivity Activity = ESRFacilityInspectorActivity::Ready;
	ESRUIVisualState StatusVisualState = ESRUIVisualState::Neutral;
	ESRUIVisualState InputVisualState = ESRUIVisualState::Neutral;
	ESRUIVisualState ProcessVisualState = ESRUIVisualState::Neutral;
	ESRUIVisualState OutputVisualState = ESRUIVisualState::Neutral;
	FText StatusLabel;
	FText StatusDetail;
	FText InputCaption;
	FText ProcessRule;
	FText EnergyTransition;
	FText StateTransition;
	FText OutputCaption;
};

class STARROVERS_API FSRFacilityInspectorPresentationBuilder
{
public:
	static FSRFacilityInspectorPresentation Build(
		const FSRFacilityInspectorPresentationInput& Input);
};
