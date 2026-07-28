#pragma once

#include "CoreMinimal.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "UI/SRUITheme.h"

/** One immediately scannable rule on an Augment card. */
struct STARROVERS_API FSRAugmentConditionEffectPresentation
{
	FText ConditionText;
	FText EffectText;
	FText DetailText;
	ESRUIVisualState ConditionState = ESRUIVisualState::Positive;
	ESRUIVisualState EffectState = ESRUIVisualState::Selected;
};

/**
 * Read-only copy for one Augment card. The builder deliberately consumes the
 * same Package definition and Run context as offer generation so UI promises
 * cannot drift away from the actual unlock rules.
 */
struct STARROVERS_API FSRAugmentChoicePresentation
{
	FText TitleText;
	FText DescriptionText;
	FText OfferBadgeText;
	FText RarityBadgeText;
	FText RunFitBadgeText;
	FText StrategyBadgeText;
	FText UnlockSectionText;
	FText UnlockDetailText;
	FText FitSectionText;
	FText FitDetailText;
	FText ImpactDetailText;
	FText RiskDetailText;
	FText WatchSummaryText;
	FText FullDetailText;
	FText SelectActionText;
	TArray<FSRAugmentConditionEffectPresentation> ConditionEffectRows;

	ESRUIVisualState CardState = ESRUIVisualState::Neutral;
	ESRUIVisualState OfferState = ESRUIVisualState::Info;
	ESRUIVisualState RunFitState = ESRUIVisualState::Positive;
	ESRUIVisualState RiskState = ESRUIVisualState::Neutral;

	int32 NewUnlockCount = 0;
	int32 AlreadyAvailableCount = 0;
	int32 SatisfiedRequirementGroupCount = 0;
	int32 TotalRequirementGroupCount = 0;
	bool bIsResourceV2Package = false;
	bool bIsMacroDoctrine = false;
	bool bEligibleInContext = true;
};

class STARROVERS_API FSRAugmentChoicePresentationBuilder final
{
public:
	static FSRAugmentChoicePresentation Build(
		const FSRAugmentChoice& Choice,
		const FSRAugmentBuildContextV2& Context);
};
