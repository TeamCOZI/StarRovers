#include "Surface/SRPlanetBiomeDataAsset.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace
{
	FSRBiomePlacementRule MakeRule(
		ESRBiomePlacementMetric Metric,
		ESRBiomePlacementComparison Comparison,
		float Threshold,
		float MaxThreshold = 1.0f)
	{
		FSRBiomePlacementRule Rule;
		Rule.Metric = Metric;
		Rule.Comparison = Comparison;
		Rule.bUseMetricDefaultThresholds = false;
		Rule.Threshold = Threshold;
		Rule.MaxThreshold = MaxThreshold;
		Rule.RefreshMetricDefaults(false);
		return Rule;
	}

	bool AreEquivalentRules(const FSRBiomePlacementRule& A, const FSRBiomePlacementRule& B)
	{
		return A.Metric == B.Metric
			&& A.Comparison == B.Comparison
			&& FMath::IsNearlyEqual(A.Threshold, B.Threshold)
			&& FMath::IsNearlyEqual(A.MaxThreshold, B.MaxThreshold);
	}

	void AddRuleIfMissing(TArray<FSRBiomePlacementRule>& Rules, const FSRBiomePlacementRule& NewRule)
	{
		for (const FSRBiomePlacementRule& ExistingRule : Rules)
		{
			if (AreEquivalentRules(ExistingRule, NewRule))
			{
				return;
			}
		}

		Rules.Add(NewRule);
	}
}

USRPlanetBiomeDataAsset::USRPlanetBiomeDataAsset()
{
	MigratePlacementRestrictionsToRules();
	RefreshPlacementRuleDefaults(false);
}

void USRPlanetBiomeDataAsset::PostLoad()
{
	Super::PostLoad();
	MigratePlacementRestrictionsToRules();
	RefreshPlacementRuleDefaults(false);
}

#if WITH_EDITOR
void USRPlanetBiomeDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberPropertyName = PropertyChangedEvent.MemberProperty
		? PropertyChangedEvent.MemberProperty->GetFName()
		: NAME_None;
	const bool bPlacementRulesChanged = MemberPropertyName == GET_MEMBER_NAME_CHECKED(USRPlanetBiomeDataAsset, PlacementRules);
	MigratePlacementRestrictionsToRules();
	RefreshPlacementRuleDefaults(bPlacementRulesChanged);
}
#endif

void USRPlanetBiomeDataAsset::MigratePlacementRestrictionsToRules()
{
	if (PlacementRestrictions.IsEmpty())
	{
		return;
	}

	for (const ESRBiomePlacementRestriction Restriction : PlacementRestrictions)
	{
		switch (Restriction)
		{
		case ESRBiomePlacementRestriction::None:
			break;
		case ESRBiomePlacementRestriction::OceanOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::OceanDepthMask, ESRBiomePlacementComparison::GreaterOrEqual, 0.62f));
			if (WaterRole == ESRBiomeWaterRole::None)
			{
				WaterRole = ESRBiomeWaterRole::Ocean;
			}
			break;
		case ESRBiomePlacementRestriction::CoastOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::CoastMask, ESRBiomePlacementComparison::GreaterOrEqual, 0.48f));
			if (WaterRole == ESRBiomeWaterRole::None)
			{
				WaterRole = ESRBiomeWaterRole::Coast;
			}
			break;
		case ESRBiomePlacementRestriction::InlandOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::InlandMask, ESRBiomePlacementComparison::GreaterOrEqual, 0.5f));
			break;
		case ESRBiomePlacementRestriction::LowAltitudeOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::HeightAlpha, ESRBiomePlacementComparison::LessOrEqual, 0.18f));
			break;
		case ESRBiomePlacementRestriction::HighAltitudeOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::HeightAlpha, ESRBiomePlacementComparison::GreaterOrEqual, 0.34f));
			break;
		case ESRBiomePlacementRestriction::MountainOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::MountainMask, ESRBiomePlacementComparison::GreaterOrEqual, 0.42f));
			break;
		case ESRBiomePlacementRestriction::EquatorialOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::AbsLatitudeDegrees, ESRBiomePlacementComparison::LessOrEqual, 30.0f));
			break;
		case ESRBiomePlacementRestriction::TemperateLatitudeOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::AbsLatitudeDegrees, ESRBiomePlacementComparison::BetweenInclusive, 23.0f, 62.0f));
			break;
		case ESRBiomePlacementRestriction::PolarOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::AbsLatitudeDegrees, ESRBiomePlacementComparison::GreaterOrEqual, 58.0f));
			break;
		case ESRBiomePlacementRestriction::HotOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::Temperature, ESRBiomePlacementComparison::GreaterOrEqual, 0.62f));
			break;
		case ESRBiomePlacementRestriction::ColdOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::Temperature, ESRBiomePlacementComparison::LessOrEqual, 0.38f));
			break;
		case ESRBiomePlacementRestriction::DryOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::Moisture, ESRBiomePlacementComparison::LessOrEqual, 0.38f));
			break;
		case ESRBiomePlacementRestriction::HumidOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::Moisture, ESRBiomePlacementComparison::GreaterOrEqual, 0.62f));
			break;
		case ESRBiomePlacementRestriction::RiverOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::RiverMask, ESRBiomePlacementComparison::GreaterOrEqual, 0.5f));
			if (WaterRole == ESRBiomeWaterRole::None)
			{
				WaterRole = ESRBiomeWaterRole::River;
			}
			break;
		case ESRBiomePlacementRestriction::LakeOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::LakeMask, ESRBiomePlacementComparison::GreaterOrEqual, 0.34f));
			if (WaterRole == ESRBiomeWaterRole::None)
			{
				WaterRole = ESRBiomeWaterRole::Lake;
			}
			break;
		case ESRBiomePlacementRestriction::HighErosionOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::Erosion, ESRBiomePlacementComparison::GreaterOrEqual, 0.62f));
			break;
		case ESRBiomePlacementRestriction::LowErosionOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::Erosion, ESRBiomePlacementComparison::LessOrEqual, 0.38f));
			break;
		case ESRBiomePlacementRestriction::RareRegionOnly:
			AddRuleIfMissing(PlacementRules, MakeRule(ESRBiomePlacementMetric::RareRegionNoise, ESRBiomePlacementComparison::GreaterOrEqual, 0.68f));
			break;
		default:
			break;
		}
	}

	PlacementRestrictions.Reset();
}

void USRPlanetBiomeDataAsset::RefreshPlacementRuleDefaults(bool bApplyMetricDefaults)
{
	for (FSRBiomePlacementRule& PlacementRule : PlacementRules)
	{
		PlacementRule.RefreshMetricDefaults(bApplyMetricDefaults);
	}
}
