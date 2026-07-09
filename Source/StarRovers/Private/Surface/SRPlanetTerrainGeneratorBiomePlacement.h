#pragma once

#include "Surface/SRPlanetTerrainGeneratorSampling.h"

namespace StarRovers::Terrain
{
	float GetPlacementMetricValue(ESRBiomePlacementMetric Metric, const FSRDefaultBiomeMetrics& Metrics);
	bool PassesPlacementRule(const FSRBiomePlacementRule& Rule, const FSRDefaultBiomeMetrics& Metrics);
	void GetPlacementMetricRange(ESRBiomePlacementMetric Metric, float& OutMinValue, float& OutMaxValue);
	float NormalizePlacementMetricValue(ESRBiomePlacementMetric Metric, float Value);

	template <typename TBiomeData>
	bool PassesAllPlacementRules(const TBiomeData& BiomeDataAsset, const FSRDefaultBiomeMetrics& Metrics)
	{
		for (const FSRBiomePlacementRule& Rule : BiomeDataAsset.PlacementRules)
		{
			if (!PassesPlacementRule(Rule, Metrics))
			{
				return false;
			}
		}

		return true;
	}

	template <typename TBiomeData>
	float GetRuleTargetForMetric(
		const TBiomeData& BiomeDataAsset,
		ESRBiomePlacementMetric Metric,
		float FallbackTarget)
	{
		float MinValue = 0.0f;
		float MaxValue = 1.0f;
		GetPlacementMetricRange(Metric, MinValue, MaxValue);

		float TargetSum = 0.0f;
		int32 TargetCount = 0;
		for (const FSRBiomePlacementRule& Rule : BiomeDataAsset.PlacementRules)
		{
			if (Rule.Metric != Metric)
			{
				continue;
			}

			const float LowerThreshold = FMath::Clamp(FMath::Min(Rule.Threshold, Rule.MaxThreshold), MinValue, MaxValue);
			const float UpperThreshold = FMath::Clamp(FMath::Max(Rule.Threshold, Rule.MaxThreshold), MinValue, MaxValue);
			float TargetValue = FallbackTarget;
			bool bHasTarget = true;

			switch (Rule.Comparison)
			{
			case ESRBiomePlacementComparison::GreaterThan:
			case ESRBiomePlacementComparison::GreaterOrEqual:
				TargetValue = (FMath::Clamp(Rule.Threshold, MinValue, MaxValue) + MaxValue) * 0.5f;
				break;
			case ESRBiomePlacementComparison::LessThan:
			case ESRBiomePlacementComparison::LessOrEqual:
				TargetValue = (MinValue + FMath::Clamp(Rule.Threshold, MinValue, MaxValue)) * 0.5f;
				break;
			case ESRBiomePlacementComparison::BetweenInclusive:
				TargetValue = (LowerThreshold + UpperThreshold) * 0.5f;
				break;
			case ESRBiomePlacementComparison::OutsideInclusive:
				TargetValue = HashBiomeUnit(BiomeDataAsset.BiomeId, static_cast<int32>(Metric) + 211) < 0.5f
								  ? (MinValue + LowerThreshold) * 0.5f
								  : (UpperThreshold + MaxValue) * 0.5f;
				break;
			default:
				bHasTarget = false;
				break;
			}

			if (bHasTarget)
			{
				TargetSum += NormalizePlacementMetricValue(Metric, TargetValue);
				++TargetCount;
			}
		}

		return TargetCount > 0 ? TargetSum / static_cast<float>(TargetCount) : FallbackTarget;
	}

	template <typename TBiomeData>
	bool HasMetricRule(const TBiomeData& BiomeDataAsset, ESRBiomePlacementMetric Metric)
	{
		for (const FSRBiomePlacementRule& Rule : BiomeDataAsset.PlacementRules)
		{
			if (Rule.Metric == Metric)
			{
				return true;
			}
		}
		return false;
	}

	template <typename TBiomeData>
	float GetRuleBasedTargetTemperature(const TBiomeData& BiomeDataAsset)
	{
		const float FallbackTarget = HashBiomeUnit(BiomeDataAsset.BiomeId, 17);
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::Temperature))
		{
			return GetRuleTargetForMetric(BiomeDataAsset, ESRBiomePlacementMetric::Temperature, FallbackTarget);
		}

		const float LatitudeTarget = GetRuleTargetForMetric(BiomeDataAsset, ESRBiomePlacementMetric::AbsLatitudeDegrees, -1.0f);
		return LatitudeTarget >= 0.0f ? 1.0f - LatitudeTarget : FallbackTarget;
	}

	template <typename TBiomeData>
	float GetRuleBasedTargetMoisture(const TBiomeData& BiomeDataAsset)
	{
		const float FallbackTarget = HashBiomeUnit(BiomeDataAsset.BiomeId, 23);
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::Moisture))
		{
			return GetRuleTargetForMetric(BiomeDataAsset, ESRBiomePlacementMetric::Moisture, FallbackTarget);
		}
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::RiverMask) || HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::LakeMask))
		{
			return 0.82f;
		}

		return FallbackTarget;
	}

	template <typename TBiomeData>
	float GetRuleBasedTargetHeight(const TBiomeData& BiomeDataAsset)
	{
		const float FallbackTarget = HashBiomeUnit(BiomeDataAsset.BiomeId, 31);
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::HeightAlpha))
		{
			return GetRuleTargetForMetric(BiomeDataAsset, ESRBiomePlacementMetric::HeightAlpha, FallbackTarget);
		}

		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::OceanDepthMask))
		{
			return 0.08f;
		}
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::CoastMask))
		{
			return 0.38f;
		}
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::MountainMask))
		{
			return 0.82f;
		}

		return FallbackTarget;
	}

	template <typename TBiomeData>
	float GetRuleBasedTargetContinentalness(const TBiomeData& BiomeDataAsset)
	{
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::Continentalness))
		{
			return GetRuleTargetForMetric(BiomeDataAsset, ESRBiomePlacementMetric::Continentalness, HashBiomeUnit(BiomeDataAsset.BiomeId, 43));
		}
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::OceanDepthMask))
		{
			return 0.08f;
		}
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::CoastMask))
		{
			return 0.46f;
		}
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::LandMask) || HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::InlandMask))
		{
			return 0.76f;
		}

		return HashBiomeUnit(BiomeDataAsset.BiomeId, 43);
	}

} // namespace StarRovers::Terrain
