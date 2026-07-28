#include "UI/SRFacilityInspectorPresentation.h"

namespace
{
	FText TextFromStringOrFallback(const FString& Value, const TCHAR* Fallback)
	{
		return FText::FromString(Value.IsEmpty() ? FString(Fallback) : Value);
	}

	FString FormatEnergy(double Value)
	{
		return FString::Printf(TEXT("%.1f"), Value);
	}
}

FSRFacilityInspectorPresentation FSRFacilityInspectorPresentationBuilder::Build(
	const FSRFacilityInspectorPresentationInput& Input)
{
	FSRFacilityInspectorPresentation Result;

	if (!Input.bProcessEnabled)
	{
		Result.Activity = ESRFacilityInspectorActivity::Disabled;
		Result.StatusVisualState = ESRUIVisualState::Disabled;
		Result.StatusLabel = NSLOCTEXT("StarRoversFacilityInspector", "StatusOffline", "OFFLINE");
		Result.StatusDetail = NSLOCTEXT(
			"StarRoversFacilityInspector",
			"StatusOfflineDetail",
			"Processing is disabled by the player.");
	}
	else if (Input.bUsesStellarFuelBatch
		&& Input.StellarFuelBatchState == ESRStellarFuelBatchStateV2::Contaminated)
	{
		Result.Activity = ESRFacilityInspectorActivity::Blocked;
		Result.StatusVisualState = ESRUIVisualState::Danger;
		Result.StatusLabel = NSLOCTEXT(
			"StarRoversFacilityInspector",
			"StatusBatchContaminated",
			"BATCH CONTAMINATED");
		Result.StatusDetail = TextFromStringOrFallback(
			Input.StellarFuelBatchDetail,
			TEXT("A blocked Card lane must be cleared or rerouted."));
	}
	else if (!Input.bCanOperate)
	{
		Result.Activity = ESRFacilityInspectorActivity::Blocked;
		Result.StatusVisualState = ESRUIVisualState::Danger;
		Result.StatusLabel = NSLOCTEXT("StarRoversFacilityInspector", "StatusBlocked", "BLOCKED");
		Result.StatusDetail = TextFromStringOrFallback(
			Input.OperationReason,
			TEXT("This facility cannot run under the current conditions."));
	}
	else if (Input.bProcessing && Input.bOutputBlocked)
	{
		Result.Activity = ESRFacilityInspectorActivity::WaitingForOutput;
		Result.StatusVisualState = ESRUIVisualState::Warning;
		Result.StatusLabel = NSLOCTEXT("StarRoversFacilityInspector", "StatusOutputBlocked", "OUTPUT BLOCKED");
		Result.StatusDetail = NSLOCTEXT(
			"StarRoversFacilityInspector",
			"StatusOutputBlockedDetail",
			"The cycle is complete, but its result has no compatible output space.");
	}
	else if (Input.bUsesStellarFuelBatch
		&& Input.bProcessing
		&& Input.StellarFuelBatchState == ESRStellarFuelBatchStateV2::Reserved)
	{
		Result.Activity = ESRFacilityInspectorActivity::Processing;
		Result.StatusVisualState = ESRUIVisualState::Positive;
		Result.StatusLabel = NSLOCTEXT(
			"StarRoversFacilityInspector",
			"StatusBatchReserved",
			"BATCH RESERVED");
		Result.StatusDetail = FText::FromString(FString::Printf(
			TEXT("Five Cards are locked for this cycle | %.0f%% complete."),
			FMath::Clamp(Input.ProgressRatio, 0.0f, 1.0f) * 100.0f));
	}
	else if (Input.bProcessing)
	{
		Result.Activity = ESRFacilityInspectorActivity::Processing;
		Result.StatusVisualState = ESRUIVisualState::Positive;
		Result.StatusLabel = NSLOCTEXT("StarRoversFacilityInspector", "StatusProcessing", "PROCESSING");
		Result.StatusDetail = FText::FromString(FString::Printf(
			TEXT("Cycle %.0f%% complete."),
			FMath::Clamp(Input.ProgressRatio, 0.0f, 1.0f) * 100.0f));
	}
	else if (Input.bUsesStellarFuelBatch
		&& (Input.StellarFuelBatchState == ESRStellarFuelBatchStateV2::Empty
			|| Input.StellarFuelBatchState == ESRStellarFuelBatchStateV2::Collecting))
	{
		Result.Activity = ESRFacilityInspectorActivity::WaitingForInput;
		Result.StatusVisualState = ESRUIVisualState::Warning;
		Result.StatusLabel = FText::FromString(FString::Printf(
			TEXT("ASSEMBLING %d/%d"),
			Input.StellarFuelValidCardCount,
			Input.StellarFuelRequiredCardCount));
		Result.StatusDetail = TextFromStringOrFallback(
			Input.StellarFuelBatchDetail,
			TEXT("Fill every Card lane to preview the final fuel."));
	}
	else if (!Input.bPreviewResolved && Input.InputResourceCount > 0)
	{
		Result.Activity = ESRFacilityInspectorActivity::Blocked;
		Result.StatusVisualState = ESRUIVisualState::Danger;
		Result.StatusLabel = NSLOCTEXT("StarRoversFacilityInspector", "StatusRecipeMismatch", "RECIPE MISMATCH");
		Result.StatusDetail = NSLOCTEXT(
			"StarRoversFacilityInspector",
			"StatusRecipeMismatchDetail",
			"Queued input does not satisfy the selected Recipe, Family, or Tag rule.");
	}
	else if (Input.InputResourceCount <= 0 && !Input.bIsMiningFacility)
	{
		Result.Activity = ESRFacilityInspectorActivity::WaitingForInput;
		Result.StatusVisualState = ESRUIVisualState::Warning;
		Result.StatusLabel = NSLOCTEXT("StarRoversFacilityInspector", "StatusWaitingInput", "WAITING INPUT");
		Result.StatusDetail = NSLOCTEXT(
			"StarRoversFacilityInspector",
			"StatusWaitingInputDetail",
			"A required input port is empty.");
	}
	else if (Input.OperationalSpeedFactor < 0.999f)
	{
		Result.Activity = ESRFacilityInspectorActivity::Throttled;
		Result.StatusVisualState = ESRUIVisualState::Warning;
		Result.StatusLabel = NSLOCTEXT("StarRoversFacilityInspector", "StatusThrottled", "THROTTLED");
		Result.StatusDetail = FText::FromString(FString::Printf(
			TEXT("Operational Capacity limits this facility to %.0f%% speed."),
			FMath::Max(0.0f, Input.OperationalSpeedFactor) * 100.0f));
	}
	else
	{
		Result.Activity = ESRFacilityInspectorActivity::Ready;
		Result.StatusVisualState = ESRUIVisualState::Info;
		Result.StatusLabel = NSLOCTEXT("StarRoversFacilityInspector", "StatusReady", "READY");
		Result.StatusDetail = TextFromStringOrFallback(Input.OperationReason, TEXT("Ready to begin the next cycle."));
	}

	if (Input.bUsesStellarFuelBatch
		&& Input.StellarFuelBatchState == ESRStellarFuelBatchStateV2::Contaminated)
	{
		Result.InputVisualState = ESRUIVisualState::Danger;
	}
	else if (Input.bUsesStellarFuelBatch
		&& Input.StellarFuelBatchState == ESRStellarFuelBatchStateV2::Collecting)
	{
		Result.InputVisualState = ESRUIVisualState::Info;
	}
	else
	{
		Result.InputVisualState = Input.InputResourceCount > 0
			? ESRUIVisualState::Positive
			: (Input.bIsMiningFacility && Input.bCanOperate
				? ESRUIVisualState::Info
				: ESRUIVisualState::Warning);
	}
	Result.ProcessVisualState = Result.StatusVisualState;
	Result.OutputVisualState = Input.bOutputBlocked
		? ESRUIVisualState::Danger
		: (Input.bPreviewResolved ? ESRUIVisualState::Positive : ESRUIVisualState::Neutral);

	if (Input.bUsesStellarFuelBatch)
	{
		const TCHAR* Prefix = Input.StellarFuelBatchState == ESRStellarFuelBatchStateV2::Reserved
			? TEXT("RESERVED CARD BATCH")
			: TEXT("CARD LANES");
		Result.InputCaption = FText::FromString(FString::Printf(
			TEXT("%s  |  %d/%d valid"),
			Prefix,
			Input.StellarFuelValidCardCount,
			Input.StellarFuelRequiredCardCount));
	}
	else if (Input.bProcessing)
	{
		Result.InputCaption = FText::FromString(FString::Printf(
			TEXT("RESERVED FOR CYCLE  |  %d resource%s"),
			Input.InputResourceCount,
			Input.InputResourceCount == 1 ? TEXT("") : TEXT("s")));
	}
	else if (Input.InputResourceCount > 0)
	{
		Result.InputCaption = FText::FromString(FString::Printf(
			TEXT("NEXT INPUT  |  %d resource%s"),
			Input.InputResourceCount,
			Input.InputResourceCount == 1 ? TEXT("") : TEXT("s")));
	}
	else
	{
		Result.InputCaption = Input.bIsMiningFacility
			? NSLOCTEXT("StarRoversFacilityInspector", "InputMiningTarget", "MINING TARGET")
			: NSLOCTEXT("StarRoversFacilityInspector", "InputEmpty", "NEXT INPUT  |  Empty");
	}

	Result.ProcessRule = TextFromStringOrFallback(
		Input.ProcessRuleSummary,
		TEXT("No process rule is available."));
	if (Input.bUsesStellarFuelBatch && !Input.StellarFuelBatchSummary.IsEmpty())
	{
		Result.ProcessRule = FText::FromString(FString::Printf(
			TEXT("%s\n%s"),
			*Result.ProcessRule.ToString(),
			*Input.StellarFuelBatchSummary));
	}

	if (!Input.bHasEnergyTransition)
	{
		Result.EnergyTransition = Input.bUsesStellarFuelBatch
			? FText::FromString(FString::Printf(
				TEXT("FINAL ENERGY  |  Waiting for complete batch (%d/%d)"),
				Input.StellarFuelValidCardCount,
				Input.StellarFuelRequiredCardCount))
			: NSLOCTEXT(
				"StarRoversFacilityInspector",
				"EnergyPending",
				"ENERGY  |  Awaiting valid input");
	}
	else if (Input.bUsesFinalFuelFormula)
	{
		Result.EnergyTransition = FText::FromString(FString::Printf(
			TEXT("FINAL ENERGY  |  %s  |  A + B x C"),
			*FormatEnergy(Input.OutputEnergy)));
	}
	else
	{
		const double EnergyDelta = Input.OutputEnergy - Input.InputEnergy;
		Result.EnergyTransition = FText::FromString(FString::Printf(
			TEXT("ENERGY  |  %s -> %s  (%+.1f additive)"),
			*FormatEnergy(Input.InputEnergy),
			*FormatEnergy(Input.OutputEnergy),
			EnergyDelta));
	}

	Result.StateTransition = TextFromStringOrFallback(
		Input.bUsesStellarFuelBatch && !Input.StellarFuelBatchDetail.IsEmpty()
			? Input.StellarFuelBatchDetail
			: Input.StateTransitionSummary,
		TEXT("STATE / TAG  |  No change"));

	if (!Input.bPreviewResolved)
	{
		if (Input.bUsesStellarFuelBatch
			&& Input.StellarFuelBatchState == ESRStellarFuelBatchStateV2::Contaminated)
		{
			Result.OutputCaption = NSLOCTEXT(
				"StarRoversFacilityInspector",
				"OutputBatchBlocked",
				"OUTPUT PREVIEW  |  Clear blocked Card lane");
		}
		else if (Input.bUsesStellarFuelBatch)
		{
			Result.OutputCaption = FText::FromString(FString::Printf(
				TEXT("OUTPUT PREVIEW  |  Complete Card lanes (%d/%d)"),
				Input.StellarFuelValidCardCount,
				Input.StellarFuelRequiredCardCount));
		}
		else
		{
			Result.OutputCaption = NSLOCTEXT(
				"StarRoversFacilityInspector",
				"OutputUnavailable",
				"OUTPUT PREVIEW  |  Unavailable");
		}
	}
	else if (Input.OutputResourceCount <= 0)
	{
		Result.OutputCaption = NSLOCTEXT(
			"StarRoversFacilityInspector",
			"OutputConsumed",
			"OUTPUT  |  Effect applied, no resource produced");
	}
	else
	{
		Result.OutputCaption = FText::FromString(FString::Printf(
			TEXT("PREDICTED OUTPUT  |  %d resource%s"),
			Input.OutputResourceCount,
			Input.OutputResourceCount == 1 ? TEXT("") : TEXT("s")));
	}

	return Result;
}
