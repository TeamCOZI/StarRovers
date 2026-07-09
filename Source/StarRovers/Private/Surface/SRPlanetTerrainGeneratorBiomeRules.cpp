#include "Surface/SRPlanetTerrainGeneratorBiomePlacement.h"

namespace StarRovers::Terrain
{
	float GetPlacementMetricValue(ESRBiomePlacementMetric Metric, const FSRDefaultBiomeMetrics& Metrics)
	{
		switch (Metric)
		{
		case ESRBiomePlacementMetric::HeightAlpha:
			return Metrics.HeightAlpha;
		case ESRBiomePlacementMetric::Continentalness:
			return Metrics.Continentalness;
		case ESRBiomePlacementMetric::LandMask:
			return Metrics.LandMask;
		case ESRBiomePlacementMetric::CoastMask:
			return Metrics.CoastMask;
		case ESRBiomePlacementMetric::OceanDepthMask:
			return Metrics.OceanDepthMask;
		case ESRBiomePlacementMetric::InlandMask:
			return Metrics.InlandMask;
		case ESRBiomePlacementMetric::MountainMask:
			return Metrics.MountainMask;
		case ESRBiomePlacementMetric::RiverMask:
			return Metrics.RiverMask;
		case ESRBiomePlacementMetric::LakeMask:
			return Metrics.LakeMask;
		case ESRBiomePlacementMetric::Erosion:
			return Metrics.Erosion;
		case ESRBiomePlacementMetric::Temperature:
			return Metrics.Temperature;
		case ESRBiomePlacementMetric::Moisture:
			return Metrics.Moisture;
		case ESRBiomePlacementMetric::AbsLatitudeDegrees:
			return Metrics.AbsLatitudeDegrees;
		case ESRBiomePlacementMetric::RareRegionNoise:
			return Metrics.RareRegionNoise;
		default:
			return 0.0f;
		}
	}

	bool PassesPlacementRule(const FSRBiomePlacementRule& Rule, const FSRDefaultBiomeMetrics& Metrics)
	{
		const float Value = GetPlacementMetricValue(Rule.Metric, Metrics);
		const float LowerThreshold = FMath::Min(Rule.Threshold, Rule.MaxThreshold);
		const float UpperThreshold = FMath::Max(Rule.Threshold, Rule.MaxThreshold);

		switch (Rule.Comparison)
		{
		case ESRBiomePlacementComparison::GreaterThan:
			return Value > Rule.Threshold;
		case ESRBiomePlacementComparison::GreaterOrEqual:
			return Value >= Rule.Threshold;
		case ESRBiomePlacementComparison::LessThan:
			return Value < Rule.Threshold;
		case ESRBiomePlacementComparison::LessOrEqual:
			return Value <= Rule.Threshold;
		case ESRBiomePlacementComparison::BetweenInclusive:
			return Value >= LowerThreshold && Value <= UpperThreshold;
		case ESRBiomePlacementComparison::OutsideInclusive:
			return Value <= LowerThreshold || Value >= UpperThreshold;
		default:
			return true;
		}
	}

	void GetPlacementMetricRange(ESRBiomePlacementMetric Metric, float& OutMinValue, float& OutMaxValue)
	{
		switch (Metric)
		{
		case ESRBiomePlacementMetric::HeightAlpha:
		case ESRBiomePlacementMetric::Continentalness:
			OutMinValue = -1.0f;
			OutMaxValue = 1.0f;
			break;
		case ESRBiomePlacementMetric::AbsLatitudeDegrees:
			OutMinValue = 0.0f;
			OutMaxValue = 90.0f;
			break;
		default:
			OutMinValue = 0.0f;
			OutMaxValue = 1.0f;
			break;
		}
	}

	float NormalizePlacementMetricValue(ESRBiomePlacementMetric Metric, float Value)
	{
		float MinValue = 0.0f;
		float MaxValue = 1.0f;
		GetPlacementMetricRange(Metric, MinValue, MaxValue);
		return FMath::GetMappedRangeValueClamped(FVector2D(MinValue, MaxValue), FVector2D(0.0f, 1.0f), Value);
	}

} // namespace StarRovers::Terrain
