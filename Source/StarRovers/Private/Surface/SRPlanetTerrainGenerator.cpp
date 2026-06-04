#include "Surface/SRPlanetTerrainGenerator.h"

#include "Misc/Crc.h"
#include "Surface/SRPlanetBiomeDataAsset.h"

namespace
{
	struct FSRDefaultBiomeMetrics
	{
		float HeightAlpha = 0.0f;
		float Continentalness = 0.0f;
		float Erosion = 0.0f;
		float Weirdness = 0.0f;
		float LandMask = 0.0f;
		float CoastMask = 0.0f;
		float OceanDepthMask = 0.0f;
		float InlandMask = 0.0f;
		float MountainMask = 0.0f;
		float RiverMask = 0.0f;
		float LakeMask = 0.0f;
		float Temperature = 0.0f;
		float Moisture = 0.0f;
		float AbsLatitudeDegrees = 0.0f;
		float RareRegionNoise = 0.0f;
	};

	uint32 HashBiomeValue(FName BiomeId, int32 Salt)
	{
		const FString HashInput = FString::Printf(TEXT("%s:%d"), *BiomeId.ToString(), Salt);
		return FCrc::StrCrc32(*HashInput);
	}

	float HashBiomeUnit(FName BiomeId, int32 Salt)
	{
		return static_cast<float>(HashBiomeValue(BiomeId, Salt) & 0x00ffffff) / static_cast<float>(0x00ffffff);
	}

	FVector BuildBiomeAnchorDirection(FName BiomeId, int32 Salt)
	{
		const float Z = (HashBiomeUnit(BiomeId, Salt) * 2.0f) - 1.0f;
		const float Angle = HashBiomeUnit(BiomeId, Salt + 97) * 2.0f * PI;
		const float Radius = FMath::Sqrt(FMath::Max(0.0f, 1.0f - (Z * Z)));
		return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Z).GetSafeNormal();
	}

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

	bool PassesAllPlacementRules(const USRPlanetBiomeDataAsset& BiomeDataAsset, const FSRDefaultBiomeMetrics& Metrics)
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

	float GetRuleTargetForMetric(
		const USRPlanetBiomeDataAsset& BiomeDataAsset,
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

	bool HasMetricRule(const USRPlanetBiomeDataAsset& BiomeDataAsset, ESRBiomePlacementMetric Metric)
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

	float GetRuleBasedTargetTemperature(const USRPlanetBiomeDataAsset& BiomeDataAsset)
	{
		const float FallbackTarget = HashBiomeUnit(BiomeDataAsset.BiomeId, 17);
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::Temperature))
		{
			return GetRuleTargetForMetric(BiomeDataAsset, ESRBiomePlacementMetric::Temperature, FallbackTarget);
		}

		const float LatitudeTarget = GetRuleTargetForMetric(BiomeDataAsset, ESRBiomePlacementMetric::AbsLatitudeDegrees, -1.0f);
		return LatitudeTarget >= 0.0f ? 1.0f - LatitudeTarget : FallbackTarget;
	}

	float GetRuleBasedTargetMoisture(const USRPlanetBiomeDataAsset& BiomeDataAsset)
	{
		const float FallbackTarget = HashBiomeUnit(BiomeDataAsset.BiomeId, 23);
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::Moisture))
		{
			return GetRuleTargetForMetric(BiomeDataAsset, ESRBiomePlacementMetric::Moisture, FallbackTarget);
		}
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::RiverMask)
			|| HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::LakeMask))
		{
			return 0.82f;
		}

		return FallbackTarget;
	}

	float GetRuleBasedTargetHeight(const USRPlanetBiomeDataAsset& BiomeDataAsset)
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

	float GetRuleBasedTargetContinentalness(const USRPlanetBiomeDataAsset& BiomeDataAsset)
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
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::LandMask)
			|| HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::InlandMask))
		{
			return 0.76f;
		}

		return HashBiomeUnit(BiomeDataAsset.BiomeId, 43);
	}

	float ScoreBiomeCandidate(
		const USRPlanetBiomeDataAsset& BiomeDataAsset,
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGeneration& Settings,
		const FSRDefaultBiomeMetrics& Metrics)
	{
		if (BiomeDataAsset.BiomeId.IsNone())
		{
			return -FLT_MAX;
		}

		if (!PassesAllPlacementRules(BiomeDataAsset, Metrics))
		{
			return -FLT_MAX;
		}

		const FVector Direction = Context.LocalUnitDirection.GetSafeNormal();
		const float TemperatureScore = 1.0f - FMath::Abs(Metrics.Temperature - GetRuleBasedTargetTemperature(BiomeDataAsset));
		const float MoistureScore = 1.0f - FMath::Abs(Metrics.Moisture - GetRuleBasedTargetMoisture(BiomeDataAsset));
		const float HeightScore = 1.0f - FMath::Abs(((Metrics.HeightAlpha + 1.0f) * 0.5f) - GetRuleBasedTargetHeight(BiomeDataAsset));
		const float ContinentScore = 1.0f - FMath::Abs(((Metrics.Continentalness + 1.0f) * 0.5f) - GetRuleBasedTargetContinentalness(BiomeDataAsset));
		const float ClimateScore = FMath::Clamp((TemperatureScore + MoistureScore + HeightScore + ContinentScore) * 0.25f, 0.0f, 1.0f);

		const float SafeRegionSize = FMath::Clamp(BiomeDataAsset.RegionSize, 0.01f, 1.0f);
		const float AnchorThreshold = FMath::Lerp(0.98f, -0.12f, SafeRegionSize);
		float AnchorScore = 0.0f;
		for (int32 AnchorIndex = 0; AnchorIndex < 2; ++AnchorIndex)
		{
			const FVector AnchorDirection = BuildBiomeAnchorDirection(BiomeDataAsset.BiomeId, Settings.GenerationSeed + (AnchorIndex * 131));
			const float AnchorDot = FVector::DotProduct(Direction, AnchorDirection);
			AnchorScore = FMath::Max(AnchorScore, FSRPlanetTerrainGenerator::SmoothStep(AnchorThreshold, 1.0f, AnchorDot));
		}

		const float PatchFrequency = FMath::Lerp(1.5f, 8.5f, HashBiomeUnit(BiomeDataAsset.BiomeId, 59));
		const float PatchNoise = FMath::Clamp(
			(FSRPlanetTerrainGenerator::SampleFractalNoise(Direction, Settings.GenerationSeed + static_cast<int32>(HashBiomeValue(BiomeDataAsset.BiomeId, 67) % 100000), PatchFrequency, 3, 0.52f) + 1.0f) * 0.5f,
			0.0f,
			1.0f);
		const float Weight = FMath::Max(0.01f, BiomeDataAsset.SpawnWeight);

		return Weight * ((ClimateScore * 0.42f) + (AnchorScore * 0.48f) + (PatchNoise * 0.10f));
	}

	const USRPlanetBiomeDataAsset* SelectBiomeDataAsset(
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGeneration& Settings,
		const FSRDefaultBiomeMetrics& Metrics)
	{
		const USRPlanetBiomeDataAsset* BestBiome = nullptr;
		float BestScore = -FLT_MAX;
		const USRPlanetBiomeDataAsset* BestPriorityOverrideBiome = nullptr;
		float BestPriorityOverrideScore = -FLT_MAX;
		for (const TObjectPtr<USRPlanetBiomeDataAsset>& BiomeDataAsset : Settings.BiomeDataAssets)
		{
			if (!IsValid(BiomeDataAsset.Get()))
			{
				continue;
			}

			const USRPlanetBiomeDataAsset& CandidateBiome = *BiomeDataAsset;
			const float Score = ScoreBiomeCandidate(CandidateBiome, Context, Settings, Metrics);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestBiome = &CandidateBiome;
			}

			if (CandidateBiome.bCanOverrideLowerPriorityBiomes
				&& Score >= FMath::Max(0.0f, CandidateBiome.OverrideMinScore)
				&& (!BestPriorityOverrideBiome
					|| CandidateBiome.Priority > BestPriorityOverrideBiome->Priority
					|| (CandidateBiome.Priority == BestPriorityOverrideBiome->Priority && Score > BestPriorityOverrideScore)))
			{
				BestPriorityOverrideBiome = &CandidateBiome;
				BestPriorityOverrideScore = Score;
			}
		}

		if (BestPriorityOverrideBiome
			&& (!BestBiome || BestPriorityOverrideBiome->Priority > BestBiome->Priority))
		{
			return BestPriorityOverrideBiome;
		}

		return BestBiome;
	}

	ESRBiomeWaterRole GetWaterRoleForBiomeDataAsset(const USRPlanetBiomeDataAsset& BiomeDataAsset)
	{
		return BiomeDataAsset.WaterRole;
	}

	bool IsOpenWaterRole(ESRBiomeWaterRole WaterRole)
	{
		return WaterRole == ESRBiomeWaterRole::Ocean
			|| WaterRole == ESRBiomeWaterRole::River
			|| WaterRole == ESRBiomeWaterRole::Lake;
	}

	FLinearColor GetBiomeDataAssetColor(const USRPlanetBiomeDataAsset& BiomeDataAsset, float HeightAlpha, float Moisture, float Temperature)
	{
		const ESRBiomeWaterRole WaterRole = GetWaterRoleForBiomeDataAsset(BiomeDataAsset);
		if (IsOpenWaterRole(WaterRole))
		{
			return FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome::Ocean, HeightAlpha, Moisture, Temperature);
		}
		if (WaterRole == ESRBiomeWaterRole::Coast)
		{
			return FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome::Coast, HeightAlpha, Moisture, Temperature);
		}
		const uint8 Hue = static_cast<uint8>(HashBiomeValue(BiomeDataAsset.BiomeId, 701) % 255);
		FLinearColor BaseColor = FLinearColor::MakeFromHSV8(Hue, 112, 158);
		const float HeightShade = FMath::Lerp(0.92f, 1.08f, FMath::Clamp((HeightAlpha + 1.0f) * 0.5f, 0.0f, 1.0f));
		const float MoistureShade = FMath::Lerp(0.96f, 1.06f, FMath::Clamp(Moisture, 0.0f, 1.0f));
		const float TemperatureShade = FMath::Lerp(0.97f, 1.03f, FMath::Clamp(Temperature, 0.0f, 1.0f));
		BaseColor *= HeightShade * MoistureShade * TemperatureShade;
		BaseColor.A = 1.0f;
		return BaseColor;
	}

	ESRPlanetBiome GetRuntimeBiomeForBiomeDataAsset(const USRPlanetBiomeDataAsset& BiomeDataAsset)
	{
		const ESRBiomeWaterRole WaterRole = GetWaterRoleForBiomeDataAsset(BiomeDataAsset);
		if (IsOpenWaterRole(WaterRole))
		{
			return ESRPlanetBiome::Ocean;
		}
		if (WaterRole == ESRBiomeWaterRole::Coast)
		{
			return ESRPlanetBiome::Coast;
		}
		return ESRPlanetBiome::Plains;
	}
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleTerrain(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings)
{
	return SampleTerrain(BuildSampleContextFromDirection(LocalUnitDirection), Settings);
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGeneration& Settings)
{
	if (!Settings.bDynamicMeshGeneration || Settings.DynamicMeshHeight <= KINDA_SMALL_NUMBER)
	{
		FSRPlanetTerrainSample Sample;
		Sample.Biome = ESRPlanetBiome::Plains;
		Sample.BiomeId = FName(TEXT("Plains"));
		return Sample;
	}

	return SampleDefaultTerrain(Context, Settings);
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleDefaultTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGeneration& Settings)
{
	FSRPlanetTerrainSample Sample;

	const FVector Direction = Context.LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return Sample;
	}

	const int32 SafeOctaves = FMath::Clamp(Settings.NoiseOctaves, 1, 8);
	const float SafePersistence = FMath::Clamp(Settings.NoisePersistence, 0.0f, 1.0f);
	const float SafeDynamicMeshHeight = FMath::Max(0.0f, Settings.DynamicMeshHeight);
	const FVector ClimateDirection = ApplyDomainWarp(Direction, Settings, Settings.NoiseStrength * 0.55f);
	const FVector TerrainDirection = ApplyDomainWarp(Direction, Settings, Settings.NoiseStrength);

	const float Continentalness = SampleFractalNoise(
		ClimateDirection,
		Settings.GenerationSeed + 10001,
		FMath::Max(0.01f, Settings.ContinentFrequency * 0.58f),
		FMath::Max(3, SafeOctaves),
		0.56f);
	const float ErosionNoise = SampleFractalNoise(
		ClimateDirection,
		Settings.GenerationSeed + 10037,
		FMath::Max(0.01f, Settings.ContinentFrequency * 1.18f),
		FMath::Max(3, SafeOctaves - 1),
		0.52f);
	const float Weirdness = SampleFractalNoise(
		TerrainDirection,
		Settings.GenerationSeed + 10061,
		FMath::Max(0.01f, Settings.MountainFrequency * 0.42f),
		FMath::Max(3, SafeOctaves),
		0.50f);
	const float Ridges = SampleRidgedNoise(
		TerrainDirection,
		Settings.GenerationSeed + 10091,
		FMath::Max(0.01f, Settings.MountainFrequency * 0.72f),
		FMath::Max(3, SafeOctaves - 1));
	const float Detail = SampleFractalNoise(
		TerrainDirection,
		Settings.GenerationSeed + 10111,
		FMath::Max(0.01f, Settings.DetailFrequency),
		FMath::Max(2, SafeOctaves - 2),
		SafePersistence);

	const float TemperatureNoise = SampleFractalNoise(
		ClimateDirection,
		Settings.GenerationSeed + 10141,
		FMath::Max(0.01f, Settings.TemperatureFrequency),
		3,
		0.5f);
	const float HumidityNoise = SampleFractalNoise(
		ClimateDirection,
		Settings.GenerationSeed + 10163,
		FMath::Max(0.01f, Settings.MoistureFrequency),
		3,
		0.5f);

	const float ContinentalnessBias = 0.18f;
	const float EffectiveContinentalness = FMath::Clamp(Continentalness + ContinentalnessBias - (Settings.OceanThreshold * 0.62f), -1.0f, 1.0f);
	const float Erosion = FMath::Clamp((ErosionNoise + 1.0f) * 0.5f, 0.0f, 1.0f);
	const float PeaksAndValleys = ComputeMinecraftPeaksAndValleys(Weirdness);
	const float LandMask = SmoothStep(-0.12f, 0.02f, EffectiveContinentalness);
	const float CoastMask = 1.0f - FMath::Abs(FMath::Clamp(EffectiveContinentalness / 0.14f, -1.0f, 1.0f));
	const float OceanDepthMask = 1.0f - SmoothStep(-0.58f, -0.12f, EffectiveContinentalness);
	const float InlandMask = SmoothStep(0.00f, 0.36f, EffectiveContinentalness);
	const float MountainSuppressionByErosion = 1.0f - SmoothStep(0.36f, 0.86f, Erosion);
	const float MountainPotential = FMath::Clamp(
		FMath::Max(SmoothStep(0.18f, 0.90f, PeaksAndValleys), Ridges * 0.72f)
		* MountainSuppressionByErosion
		* LandMask,
		0.0f,
		1.0f);
	const float ValleyMask = SmoothStep(-0.95f, -0.18f, -PeaksAndValleys) * LandMask;
	const float PlateauMask = SmoothStep(0.18f, 0.72f, EffectiveContinentalness) * SmoothStep(0.28f, 0.68f, Erosion);

	const float OceanFloorHeight = -OceanDepthMask * OceanDepthMask * SafeDynamicMeshHeight * 0.42f;
	const float LandLift = LandMask * SafeDynamicMeshHeight * 0.10f;
	const float CoastalShelfHeight = CoastMask * SafeDynamicMeshHeight * 0.08f;
	const float PlainsHeight = LandMask * SafeDynamicMeshHeight * FMath::Lerp(0.14f, 0.28f, InlandMask) * SmoothStep(0.12f, 0.78f, Erosion);
	const float PlateauHeight = PlateauMask * SafeDynamicMeshHeight * 0.24f;
	const float MountainHeight = MountainPotential
		* SafeDynamicMeshHeight
		* FMath::Lerp(0.48f, 0.92f, FMath::Clamp(PeaksAndValleys, 0.0f, 1.0f))
		* FMath::Pow(FMath::Clamp(Settings.MountainStrength / 2.0f, 0.25f, 2.0f), 0.45f);
	const float ValleyCarve = ValleyMask * SafeDynamicMeshHeight * FMath::Lerp(0.10f, 0.31f, 1.0f - Erosion) * FMath::Clamp(Settings.ValleyStrength, 0.0f, 1.0f);
	const float DetailHeight = Detail * SafeDynamicMeshHeight * 0.035f * FMath::Clamp(Settings.DetailStrength, 0.0f, 1.0f) * FMath::Lerp(0.35f, 1.0f, LandMask);
	const float RidgeBonus = FMath::Square(Ridges) * MountainPotential * SafeDynamicMeshHeight * 0.16f;

	const float HeightBeforeSurfaceRules = OceanFloorHeight + LandLift + CoastalShelfHeight + PlainsHeight + PlateauHeight + MountainHeight + RidgeBonus + DetailHeight - ValleyCarve;
	const float HeightAlphaBeforeRules = FMath::Clamp(HeightBeforeSurfaceRules / FMath::Max(SafeDynamicMeshHeight, KINDA_SMALL_NUMBER), -1.0f, 1.0f);
	const float RiverMask = SampleRiverMask(TerrainDirection, Settings, LandMask, MountainPotential);
	const float LakeMask = SampleLakeMask(TerrainDirection, Settings, LandMask, HeightAlphaBeforeRules);
	const float SurfaceRuleCarve = ((RiverMask * 0.13f) + (LakeMask * 0.08f)) * SafeDynamicMeshHeight;

	Sample.HeightOffset = HeightBeforeSurfaceRules - SurfaceRuleCarve;
	Sample.Continent = EffectiveContinentalness;
	Sample.MountainMask = MountainPotential;
	Sample.RiverMask = RiverMask;
	Sample.LakeMask = LakeMask;
	Sample.PlateBeltMask = Ridges * MountainPotential;

	const float LatitudeTemperature = 1.0f - FMath::Abs(Direction.Z);
	const float HeightTemperaturePenalty = FMath::Max(0.0f, Sample.HeightOffset / FMath::Max(SafeDynamicMeshHeight, KINDA_SMALL_NUMBER)) * 0.28f;
	Sample.Temperature = FMath::Clamp((LatitudeTemperature * 0.78f) + (TemperatureNoise * 0.18f) + 0.11f - HeightTemperaturePenalty, 0.0f, 1.0f);
	Sample.Moisture = FMath::Clamp((HumidityNoise + 1.0f) * 0.5f, 0.0f, 1.0f);

	const float HeightAlpha = FMath::Clamp(Sample.HeightOffset / FMath::Max(SafeDynamicMeshHeight, KINDA_SMALL_NUMBER), -1.0f, 1.0f);
	const float AbsLatitudeSin = static_cast<float>(FMath::Clamp(FMath::Abs(Direction.Z), 0.0, 1.0));
	FSRDefaultBiomeMetrics Metrics;
	Metrics.HeightAlpha = HeightAlpha;
	Metrics.Continentalness = EffectiveContinentalness;
	Metrics.Erosion = Erosion;
	Metrics.Weirdness = Weirdness;
	Metrics.LandMask = LandMask;
	Metrics.CoastMask = CoastMask;
	Metrics.OceanDepthMask = OceanDepthMask;
	Metrics.InlandMask = InlandMask;
	Metrics.MountainMask = MountainPotential;
	Metrics.RiverMask = RiverMask;
	Metrics.LakeMask = LakeMask;
	Metrics.Temperature = Sample.Temperature;
	Metrics.Moisture = Sample.Moisture;
	Metrics.AbsLatitudeDegrees = FMath::RadiansToDegrees(FMath::Asin(AbsLatitudeSin));
	Metrics.RareRegionNoise = FMath::Clamp((SampleFractalNoise(Direction, Settings.GenerationSeed + 503, Settings.ContinentFrequency * 3.75f, 3, 0.54f) + 1.0f) * 0.5f, 0.0f, 1.0f);

	if (const USRPlanetBiomeDataAsset* BiomeDataAsset = SelectBiomeDataAsset(Context, Settings, Metrics))
	{
		Sample.Biome = GetRuntimeBiomeForBiomeDataAsset(*BiomeDataAsset);
		Sample.BiomeId = BiomeDataAsset->BiomeId;
		Sample.WaterRole = GetWaterRoleForBiomeDataAsset(*BiomeDataAsset);
		Sample.SurfaceColor = GetBiomeDataAssetColor(*BiomeDataAsset, HeightAlpha, Sample.Moisture, Sample.Temperature);
	}
	else
	{
		Sample.BiomeId = NAME_None;
		UE_LOG(LogTemp, Error, TEXT("Terrain generation requires at least one Profile BiomeDataAsset whose placement filters pass for the sampled cell."));
	}

	if (Sample.WaterRole == ESRBiomeWaterRole::Coast && (RiverMask > 0.58f || LakeMask > 0.38f))
	{
		const float WaterMask = FMath::Clamp(FMath::Max(RiverMask, LakeMask), 0.0f, 1.0f);
		Sample.SurfaceColor = FLinearColor::LerpUsingHSV(Sample.SurfaceColor, FLinearColor(0.03f, 0.18f, 0.34f, 1.0f), WaterMask * 0.74f);
	}

	Sample.SurfaceColor.A = 1.0f;
	return Sample;
}

FSRBiomeSampleContext FSRPlanetTerrainGenerator::BuildSampleContextFromDirection(const FVector& LocalUnitDirection)
{
	FSRBiomeSampleContext Context;
	Context.LocalUnitDirection = LocalUnitDirection.GetSafeNormal();
	if (Context.LocalUnitDirection.IsNearlyZero())
	{
		Context.LocalUnitDirection = FVector::UpVector;
		Context.Face = ESRCubeSphereFace::PositiveZ;
		return Context;
	}

	const FVector AbsDirection = Context.LocalUnitDirection.GetAbs();
	if (AbsDirection.X >= AbsDirection.Y && AbsDirection.X >= AbsDirection.Z)
	{
		Context.Face = Context.LocalUnitDirection.X >= 0.0f ? ESRCubeSphereFace::PositiveX : ESRCubeSphereFace::NegativeX;
	}
	else if (AbsDirection.Y >= AbsDirection.X && AbsDirection.Y >= AbsDirection.Z)
	{
		Context.Face = Context.LocalUnitDirection.Y >= 0.0f ? ESRCubeSphereFace::PositiveY : ESRCubeSphereFace::NegativeY;
	}
	else
	{
		Context.Face = Context.LocalUnitDirection.Z >= 0.0f ? ESRCubeSphereFace::PositiveZ : ESRCubeSphereFace::NegativeZ;
	}

	return Context;
}

float FSRPlanetTerrainGenerator::ComputeMinecraftPeaksAndValleys(float Weirdness)
{
	const float AbsWeirdness = FMath::Abs(FMath::Clamp(Weirdness, -1.0f, 1.0f));
	return -(FMath::Abs(AbsWeirdness - (2.0f / 3.0f)) - (1.0f / 3.0f)) * 3.0f;
}

float FSRPlanetTerrainGenerator::SampleFractalNoise(const FVector& LocalUnitDirection, int32 Seed, float Frequency, int32 Octaves, float Persistence)
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	float CurrentFrequency = FMath::Max(0.01f, Frequency);
	float CurrentAmplitude = 1.0f;
	float NoiseSum = 0.0f;
	float AmplitudeSum = 0.0f;
	const FVector SeedOffset = BuildSeedOffset(Seed);
	const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 8);
	const float SafePersistence = FMath::Clamp(Persistence, 0.0f, 1.0f);

	for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
	{
		const float NoiseValue = FMath::PerlinNoise3D((Direction * CurrentFrequency) + SeedOffset);
		NoiseSum += NoiseValue * CurrentAmplitude;
		AmplitudeSum += CurrentAmplitude;
		CurrentFrequency *= 2.0f;
		CurrentAmplitude *= SafePersistence;
	}

	return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
}

float FSRPlanetTerrainGenerator::SampleRidgedNoise(const FVector& LocalUnitDirection, int32 Seed, float Frequency, int32 Octaves)
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	float CurrentFrequency = FMath::Max(0.01f, Frequency);
	float CurrentAmplitude = 1.0f;
	float NoiseSum = 0.0f;
	float AmplitudeSum = 0.0f;
	const FVector SeedOffset = BuildSeedOffset(Seed);
	const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 8);

	for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
	{
		const float NoiseValue = FMath::PerlinNoise3D((Direction * CurrentFrequency) + SeedOffset);
		const float RidgedValue = 1.0f - FMath::Abs(NoiseValue);
		NoiseSum += FMath::Clamp(RidgedValue, 0.0f, 1.0f) * CurrentAmplitude;
		AmplitudeSum += CurrentAmplitude;
		CurrentFrequency *= 2.0f;
		CurrentAmplitude *= 0.5f;
	}

	return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
}

FVector FSRPlanetTerrainGenerator::ApplyDomainWarp(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float Strength)
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}

	const float SafeStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	if (SafeStrength <= KINDA_SMALL_NUMBER)
	{
		return Direction;
	}

	const float WarpFrequency = FMath::Max(0.01f, Settings.ContinentFrequency * 2.0f);
	const FVector Warp(
		SampleFractalNoise(Direction, Settings.GenerationSeed + 211, WarpFrequency, 3, 0.5f),
		SampleFractalNoise(Direction, Settings.GenerationSeed + 223, WarpFrequency, 3, 0.5f),
		SampleFractalNoise(Direction, Settings.GenerationSeed + 227, WarpFrequency, 3, 0.5f));

	return (Direction + (Warp * SafeStrength * 0.42f)).GetSafeNormal();
}

float FSRPlanetTerrainGenerator::SampleRiverMask(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float MountainMask)
{
	const float Strength = FMath::Clamp(Settings.RiverStrength, 0.0f, 1.0f);
	if (Strength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	const float ChannelA = FMath::Abs(SampleFractalNoise(Direction, Settings.GenerationSeed + 263, Settings.ContinentFrequency * 5.5f, 4, 0.55f));
	const float ChannelB = FMath::Abs(SampleFractalNoise(Direction, Settings.GenerationSeed + 269, Settings.ContinentFrequency * 9.0f, 3, 0.48f));
	const float LargeChannel = 1.0f - SmoothStep(0.018f, 0.082f, ChannelA);
	const float Tributary = 1.0f - SmoothStep(0.012f, 0.052f, ChannelB);
	const float SourceMask = SmoothStep(0.18f, 0.75f, MountainMask);
	const float RiverMask = FMath::Max(LargeChannel, Tributary * 0.55f) * FMath::Lerp(0.45f, 1.0f, SourceMask) * LandMask;
	return FMath::Clamp(RiverMask * Strength, 0.0f, 1.0f);
}

float FSRPlanetTerrainGenerator::SampleLakeMask(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float HeightAlpha)
{
	const float Strength = FMath::Clamp(Settings.LakeStrength, 0.0f, 1.0f);
	if (Strength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	const float BasinNoiseA = (SampleFractalNoise(Direction, Settings.GenerationSeed + 277, Settings.ContinentFrequency * 5.2f, 3, 0.42f) + 1.0f) * 0.5f;
	const float BasinNoiseB = FMath::Abs(SampleFractalNoise(Direction, Settings.GenerationSeed + 283, Settings.ContinentFrequency * 8.5f, 2, 0.36f));
	const float BasinMask = SmoothStep(0.84f, 0.98f, BasinNoiseA) * SmoothStep(0.08f, 0.28f, BasinNoiseB);
	const float LowlandMask = 1.0f - SmoothStep(0.04f, 0.30f, HeightAlpha);
	const float InlandMask = SmoothStep(0.72f, 0.94f, LandMask);
	return FMath::Clamp(BasinMask * LowlandMask * InlandMask * Strength * 0.45f, 0.0f, 1.0f);
}

FLinearColor FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome Biome, float HeightAlpha, float Moisture, float Temperature)
{
	FLinearColor BaseColor;
	switch (Biome)
	{
	case ESRPlanetBiome::Ocean:
		BaseColor = FLinearColor(0.018f, 0.105f, 0.255f, 1.0f);
		break;
	case ESRPlanetBiome::Coast:
		BaseColor = FLinearColor(0.62f, 0.56f, 0.38f, 1.0f);
		break;
	case ESRPlanetBiome::Snow:
		BaseColor = FLinearColor(0.78f, 0.80f, 0.77f, 1.0f);
		break;
	default:
		BaseColor = FLinearColor(0.28f, 0.46f, 0.23f, 1.0f);
		break;
	}

	const float HeightShade = FMath::Lerp(0.92f, 1.08f, FMath::Clamp((HeightAlpha + 1.0f) * 0.5f, 0.0f, 1.0f));
	const float MoistureShade = FMath::Lerp(0.96f, 1.06f, FMath::Clamp(Moisture, 0.0f, 1.0f));
	const float TemperatureShade = FMath::Lerp(0.97f, 1.03f, FMath::Clamp(Temperature, 0.0f, 1.0f));
	BaseColor *= HeightShade * MoistureShade * TemperatureShade;
	if (Biome == ESRPlanetBiome::Ocean)
	{
		const float ShallowWater = FMath::Clamp(HeightAlpha + 0.42f, 0.0f, 1.0f);
		BaseColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor(0.04f, 0.30f, 0.46f, 1.0f), ShallowWater * 0.45f);
	}
	else if (Biome == ESRPlanetBiome::Coast)
	{
		const float WetCoast = FMath::Clamp(Moisture, 0.0f, 1.0f);
		BaseColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor(0.20f, 0.46f, 0.24f, 1.0f), WetCoast * 0.35f);
	}
	BaseColor.A = 1.0f;
	return BaseColor;
}

FVector FSRPlanetTerrainGenerator::BuildSeedOffset(int32 Seed)
{
	const int64 Seed64 = static_cast<int64>(Seed);
	return FVector(
		static_cast<float>((Seed64 * 15731LL) % 10007LL),
		static_cast<float>((Seed64 * 789221LL) % 10009LL),
		static_cast<float>((Seed64 * 1376312589LL) % 10037LL));
}

float FSRPlanetTerrainGenerator::SmoothStep(float Edge0, float Edge1, float Value)
{
	if (FMath::IsNearlyEqual(Edge0, Edge1))
	{
		return Value >= Edge1 ? 1.0f : 0.0f;
	}

	const float Alpha = FMath::Clamp((Value - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
	return Alpha * Alpha * (3.0f - (2.0f * Alpha));
}
