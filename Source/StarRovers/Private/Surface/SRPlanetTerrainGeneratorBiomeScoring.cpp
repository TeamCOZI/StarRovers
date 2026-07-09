#include "Surface/SRPlanetTerrainGeneratorBiomePlacement.h"

#include "Surface/SRPlanetBiomeDataAsset.h"

namespace StarRovers::Terrain
{
	FVector BuildBiomeAnchorDirection(FName BiomeId, int32 Salt)
	{
		const float Z = (HashBiomeUnit(BiomeId, Salt) * 2.0f) - 1.0f;
		const float Angle = HashBiomeUnit(BiomeId, Salt + 97) * 2.0f * PI;
		const float Radius = FMath::Sqrt(FMath::Max(0.0f, 1.0f - (Z * Z)));
		return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Z).GetSafeNormal();
	}

	template <typename TBiomeData, typename TSettings>
	float ScoreBiomeCandidate(
		const TBiomeData& BiomeDataAsset,
		const FSRBiomeSampleContext& Context,
		const TSettings& Settings,
		const FSRDefaultBiomeMetrics& Metrics,
		const FVector& UnitDirection)
	{
		(void)Context;

		if (BiomeDataAsset.BiomeId.IsNone())
		{
			return -FLT_MAX;
		}

		if (!PassesAllPlacementRules(BiomeDataAsset, Metrics))
		{
			return -FLT_MAX;
		}

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
			const float AnchorDot = FVector::DotProduct(UnitDirection, AnchorDirection);
			AnchorScore = FMath::Max(AnchorScore, FSRPlanetTerrainGenerator::SmoothStep(AnchorThreshold, 1.0f, AnchorDot));
		}

		const float PatchFrequency = FMath::Lerp(1.5f, 8.5f, HashBiomeUnit(BiomeDataAsset.BiomeId, 59));
		const float PatchNoise = FMath::Clamp(
			(SampleFractalNoiseUnitDirection(UnitDirection, Settings.GenerationSeed + static_cast<int32>(HashBiomeValue(BiomeDataAsset.BiomeId, 67) % 100000), PatchFrequency, 3, 0.52f) + 1.0f) * 0.5f,
			0.0f,
			1.0f);
		const float Weight = FMath::Max(0.01f, BiomeDataAsset.SpawnWeight);

		return Weight * ((ClimateScore * 0.42f) + (AnchorScore * 0.48f) + (PatchNoise * 0.10f));
	}

	float ScoreBiomeCandidate(
		const FSRCompiledPlanetBiomeGenerationSnapshot& BiomeData,
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGenerationSnapshot& Settings,
		const FSRDefaultBiomeMetrics& Metrics,
		const FVector& UnitDirection)
	{
		(void)Settings;
		(void)Context;

		if (BiomeData.BiomeId.IsNone())
		{
			return -FLT_MAX;
		}

		if (!PassesAllPlacementRules(BiomeData, Metrics))
		{
			return -FLT_MAX;
		}

		const float TemperatureScore = 1.0f - FMath::Abs(Metrics.Temperature - BiomeData.TargetTemperature);
		const float MoistureScore = 1.0f - FMath::Abs(Metrics.Moisture - BiomeData.TargetMoisture);
		const float HeightScore = 1.0f - FMath::Abs(((Metrics.HeightAlpha + 1.0f) * 0.5f) - BiomeData.TargetHeight);
		const float ContinentScore = 1.0f - FMath::Abs(((Metrics.Continentalness + 1.0f) * 0.5f) - BiomeData.TargetContinentalness);
		const float ClimateScore = FMath::Clamp((TemperatureScore + MoistureScore + HeightScore + ContinentScore) * 0.25f, 0.0f, 1.0f);

		float AnchorScore = 0.0f;
		for (const FVector& AnchorDirection : BiomeData.AnchorDirections)
		{
			const float AnchorDot = FVector::DotProduct(UnitDirection, AnchorDirection);
			AnchorScore = FMath::Max(AnchorScore, FSRPlanetTerrainGenerator::SmoothStep(BiomeData.AnchorThreshold, 1.0f, AnchorDot));
		}

		const float PatchNoise = FMath::Clamp(
			(SampleFractalNoiseUnitDirection(UnitDirection, BiomeData.PatchSeedOffset, BiomeData.PatchFrequency, 3, 0.52f) + 1.0f) * 0.5f,
			0.0f,
			1.0f);

		return BiomeData.Weight * ((ClimateScore * 0.42f) + (AnchorScore * 0.48f) + (PatchNoise * 0.10f));
	}

	const USRPlanetBiomeDataAsset* SelectBiomeDataAsset(
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGeneration& Settings,
		const FSRDefaultBiomeMetrics& Metrics,
		const FVector& UnitDirection)
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
			const float Score = ScoreBiomeCandidate(CandidateBiome, Context, Settings, Metrics, UnitDirection);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestBiome = &CandidateBiome;
			}

			if (CandidateBiome.bCanOverrideLowerPriorityBiomes && Score >= FMath::Max(0.0f, CandidateBiome.OverrideMinScore) && (!BestPriorityOverrideBiome || CandidateBiome.Priority > BestPriorityOverrideBiome->Priority || (CandidateBiome.Priority == BestPriorityOverrideBiome->Priority && Score > BestPriorityOverrideScore)))
			{
				BestPriorityOverrideBiome = &CandidateBiome;
				BestPriorityOverrideScore = Score;
			}
		}

		if (BestPriorityOverrideBiome && (!BestBiome || BestPriorityOverrideBiome->Priority > BestBiome->Priority))
		{
			return BestPriorityOverrideBiome;
		}

		return BestBiome;
	}

	const FSRCompiledPlanetBiomeGenerationSnapshot* SelectBiomeDataSnapshot(
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGenerationSnapshot& Settings,
		const FSRDefaultBiomeMetrics& Metrics,
		const FVector& UnitDirection)
	{
		(void)Context;
		(void)Settings;

		const FSRCompiledPlanetBiomeGenerationSnapshot* BestBiome = nullptr;
		float BestScore = -FLT_MAX;
		const FSRCompiledPlanetBiomeGenerationSnapshot* BestPriorityOverrideBiome = nullptr;
		float BestPriorityOverrideScore = -FLT_MAX;
		for (const FSRCompiledPlanetBiomeGenerationSnapshot& CandidateBiome : Settings.CompiledBiomes)
		{
			if (CandidateBiome.BiomeId.IsNone())
			{
				continue;
			}

			if (!PassesAllPlacementRules(CandidateBiome, Metrics))
			{
				continue;
			}

			const float TemperatureScore = 1.0f - FMath::Abs(Metrics.Temperature - CandidateBiome.TargetTemperature);
			const float MoistureScore = 1.0f - FMath::Abs(Metrics.Moisture - CandidateBiome.TargetMoisture);
			const float HeightScore = 1.0f - FMath::Abs(((Metrics.HeightAlpha + 1.0f) * 0.5f) - CandidateBiome.TargetHeight);
			const float ContinentScore = 1.0f - FMath::Abs(((Metrics.Continentalness + 1.0f) * 0.5f) - CandidateBiome.TargetContinentalness);
			const float ClimateScore = FMath::Clamp((TemperatureScore + MoistureScore + HeightScore + ContinentScore) * 0.25f, 0.0f, 1.0f);

			float AnchorScore = 0.0f;
			for (const FVector& AnchorDirection : CandidateBiome.AnchorDirections)
			{
				const float AnchorDot = FVector::DotProduct(UnitDirection, AnchorDirection);
				AnchorScore = FMath::Max(AnchorScore, FSRPlanetTerrainGenerator::SmoothStep(CandidateBiome.AnchorThreshold, 1.0f, AnchorDot));
			}

			const float ScoreWithoutPatchNoise = CandidateBiome.Weight * ((ClimateScore * 0.42f) + (AnchorScore * 0.48f));
			const float MaxPossibleScore = ScoreWithoutPatchNoise + (CandidateBiome.Weight * 0.10f);
			const bool bCanBeatCurrentBest = MaxPossibleScore > BestScore;
			const bool bCanBeatPriorityOverride =
				CandidateBiome.bCanOverrideLowerPriorityBiomes && MaxPossibleScore >= FMath::Max(0.0f, CandidateBiome.OverrideMinScore) && (!BestPriorityOverrideBiome || CandidateBiome.Priority > BestPriorityOverrideBiome->Priority || (CandidateBiome.Priority == BestPriorityOverrideBiome->Priority && MaxPossibleScore > BestPriorityOverrideScore));
			if (!bCanBeatCurrentBest && !bCanBeatPriorityOverride)
			{
				continue;
			}

			const float PatchNoise = FMath::Clamp(
				(SampleFractalNoiseUnitDirection(UnitDirection, CandidateBiome.PatchSeedOffset, CandidateBiome.PatchFrequency, 3, 0.52f) + 1.0f) * 0.5f,
				0.0f,
				1.0f);
			const float Score = ScoreWithoutPatchNoise + (CandidateBiome.Weight * PatchNoise * 0.10f);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestBiome = &CandidateBiome;
			}

			if (CandidateBiome.bCanOverrideLowerPriorityBiomes && Score >= FMath::Max(0.0f, CandidateBiome.OverrideMinScore) && (!BestPriorityOverrideBiome || CandidateBiome.Priority > BestPriorityOverrideBiome->Priority || (CandidateBiome.Priority == BestPriorityOverrideBiome->Priority && Score > BestPriorityOverrideScore)))
			{
				BestPriorityOverrideBiome = &CandidateBiome;
				BestPriorityOverrideScore = Score;
			}
		}

		if (BestPriorityOverrideBiome && (!BestBiome || BestPriorityOverrideBiome->Priority > BestBiome->Priority))
		{
			return BestPriorityOverrideBiome;
		}

		return BestBiome;
	}

} // namespace StarRovers::Terrain
