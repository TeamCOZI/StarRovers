#include "Surface/SRPlanetTerrainGenerator.h"

#include "HAL/CriticalSection.h"
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

	FCriticalSection GNoMatchingBiomeLogCriticalSection;
	TSet<uint32> GNoMatchingBiomeLogKeys;

	uint32 HashBiomeValue(FName BiomeId, int32 Salt)
	{
		const FString HashInput = FString::Printf(TEXT("%s:%d"), *BiomeId.ToString(), Salt);
		return FCrc::StrCrc32(*HashInput);
	}

	float HashBiomeUnit(FName BiomeId, int32 Salt)
	{
		return static_cast<float>(HashBiomeValue(BiomeId, Salt) & 0x00ffffff) / static_cast<float>(0x00ffffff);
	}

	uint32 BuildNoMatchingBiomeLogKey(const FSRDynamicMeshGeneration& Settings)
	{
		uint32 Hash = ::GetTypeHash(Settings.GenerationSeed);
		Hash = HashCombine(Hash, ::GetTypeHash(Settings.BiomeDataAssets.Num()));
		for (const TObjectPtr<USRPlanetBiomeDataAsset>& BiomeDataAsset : Settings.BiomeDataAssets)
		{
			if (IsValid(BiomeDataAsset.Get()))
			{
				Hash = HashCombine(Hash, FCrc::StrCrc32(*BiomeDataAsset->BiomeId.ToString()));
			}
		}
		return Hash;
	}

	uint32 BuildNoMatchingBiomeLogKey(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		uint32 Hash = ::GetTypeHash(Settings.GenerationSeed);
		Hash = HashCombine(Hash, ::GetTypeHash(Settings.Biomes.Num()));
		for (const FSRPlanetBiomeGenerationSnapshot& Biome : Settings.Biomes)
		{
			Hash = HashCombine(Hash, FCrc::StrCrc32(*Biome.BiomeId.ToString()));
		}
		return Hash;
	}

	int32 CountValidBiomes(const FSRDynamicMeshGeneration& Settings)
	{
		int32 Count = 0;
		for (const TObjectPtr<USRPlanetBiomeDataAsset>& BiomeDataAsset : Settings.BiomeDataAssets)
		{
			if (IsValid(BiomeDataAsset.Get()))
			{
				++Count;
			}
		}
		return Count;
	}

	int32 CountValidBiomes(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		int32 Count = 0;
		for (const FSRPlanetBiomeGenerationSnapshot& Biome : Settings.Biomes)
		{
			if (!Biome.BiomeId.IsNone())
			{
				++Count;
			}
		}
		return Count;
	}

	template <typename TSettings>
	void LogNoMatchingBiomeOnce(const TSettings& Settings, const FSRBiomeSampleContext& Context)
	{
		const uint32 LogKey = BuildNoMatchingBiomeLogKey(Settings);
		{
			FScopeLock Lock(&GNoMatchingBiomeLogCriticalSection);
			if (GNoMatchingBiomeLogKeys.Contains(LogKey))
			{
				return;
			}
			GNoMatchingBiomeLogKeys.Add(LogKey);
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Terrain generation could not find a Profile BiomeDataAsset whose placement filters pass for at least one sampled cell. Falling back to Plains for unmatched cells. Seed=%d Biomes=%d ExampleFace=%d ExampleCell=(%d,%d)"),
			Settings.GenerationSeed,
			CountValidBiomes(Settings),
			static_cast<int32>(Context.Face),
			Context.CellX,
			Context.CellY);
	}

	FVector BuildBiomeAnchorDirection(FName BiomeId, int32 Salt)
	{
		const float Z = (HashBiomeUnit(BiomeId, Salt) * 2.0f) - 1.0f;
		const float Angle = HashBiomeUnit(BiomeId, Salt + 97) * 2.0f * PI;
		const float Radius = FMath::Sqrt(FMath::Max(0.0f, 1.0f - (Z * Z)));
		return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Z).GetSafeNormal();
	}

	FVector BuildNoiseSeedOffset(int32 Seed)
	{
		const int64 Seed64 = static_cast<int64>(Seed);
		return FVector(
			static_cast<float>((Seed64 * 15731LL) % 10007LL),
			static_cast<float>((Seed64 * 789221LL) % 10009LL),
			static_cast<float>((Seed64 * 1376312589LL) % 10037LL));
	}

	float SampleFractalNoiseUnitDirection(const FVector& UnitDirection, int32 Seed, float Frequency, int32 Octaves, float Persistence)
	{
		float CurrentFrequency = FMath::Max(0.01f, Frequency);
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;
		const FVector SeedOffset = BuildNoiseSeedOffset(Seed);
		const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 8);
		const float SafePersistence = FMath::Clamp(Persistence, 0.0f, 1.0f);

		for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + SeedOffset);
			NoiseSum += NoiseValue * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= SafePersistence;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float SampleFractalNoiseUnitDirection(const FVector& UnitDirection, const FSRCompiledTerrainNoiseDescriptor& Noise)
	{
		float CurrentFrequency = Noise.Frequency;
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;

		for (int32 OctaveIndex = 0; OctaveIndex < Noise.Octaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + Noise.SeedOffset);
			NoiseSum += NoiseValue * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= Noise.Persistence;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float SampleFractalNoiseUnitDirection(
		const FVector& UnitDirection,
		const FVector& SeedOffset,
		float Frequency,
		int32 Octaves,
		float Persistence)
	{
		float CurrentFrequency = Frequency;
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;

		for (int32 OctaveIndex = 0; OctaveIndex < Octaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + SeedOffset);
			NoiseSum += NoiseValue * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= Persistence;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float SampleRidgedNoiseUnitDirection(const FVector& UnitDirection, int32 Seed, float Frequency, int32 Octaves)
	{
		float CurrentFrequency = FMath::Max(0.01f, Frequency);
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;
		const FVector SeedOffset = BuildNoiseSeedOffset(Seed);
		const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 8);

		for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + SeedOffset);
			const float RidgedValue = 1.0f - FMath::Abs(NoiseValue);
			NoiseSum += FMath::Clamp(RidgedValue, 0.0f, 1.0f) * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= 0.5f;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float SampleRidgedNoiseUnitDirection(const FVector& UnitDirection, const FSRCompiledTerrainNoiseDescriptor& Noise)
	{
		float CurrentFrequency = Noise.Frequency;
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;

		for (int32 OctaveIndex = 0; OctaveIndex < Noise.Octaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + Noise.SeedOffset);
			const float RidgedValue = 1.0f - FMath::Abs(NoiseValue);
			NoiseSum += FMath::Clamp(RidgedValue, 0.0f, 1.0f) * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= 0.5f;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
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
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::RiverMask)
			|| HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::LakeMask))
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
		if (HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::LandMask)
			|| HasMetricRule(BiomeDataAsset, ESRBiomePlacementMetric::InlandMask))
		{
			return 0.76f;
		}

		return HashBiomeUnit(BiomeDataAsset.BiomeId, 43);
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
				CandidateBiome.bCanOverrideLowerPriorityBiomes
				&& MaxPossibleScore >= FMath::Max(0.0f, CandidateBiome.OverrideMinScore)
				&& (!BestPriorityOverrideBiome
					|| CandidateBiome.Priority > BestPriorityOverrideBiome->Priority
					|| (CandidateBiome.Priority == BestPriorityOverrideBiome->Priority && MaxPossibleScore > BestPriorityOverrideScore));
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

	template <typename TBiomeData>
	ESRBiomeWaterRole GetWaterRoleForBiomeDataAsset(const TBiomeData& BiomeDataAsset)
	{
		return BiomeDataAsset.WaterRole;
	}

	bool IsOpenWaterRole(ESRBiomeWaterRole WaterRole)
	{
		return WaterRole == ESRBiomeWaterRole::Ocean
			|| WaterRole == ESRBiomeWaterRole::River
			|| WaterRole == ESRBiomeWaterRole::Lake;
	}

	template <typename TBiomeData>
	FLinearColor GetBiomeDataAssetColor(const TBiomeData& BiomeDataAsset, float HeightAlpha, float Moisture, float Temperature)
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

	FLinearColor GetBiomeDataAssetColor(const FSRCompiledPlanetBiomeGenerationSnapshot& BiomeData, float HeightAlpha, float Moisture, float Temperature)
	{
		const ESRBiomeWaterRole WaterRole = GetWaterRoleForBiomeDataAsset(BiomeData);
		if (IsOpenWaterRole(WaterRole))
		{
			return FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome::Ocean, HeightAlpha, Moisture, Temperature);
		}
		if (WaterRole == ESRBiomeWaterRole::Coast)
		{
			return FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome::Coast, HeightAlpha, Moisture, Temperature);
		}

		FLinearColor BaseColor = BiomeData.BaseLandColor;
		const float HeightShade = FMath::Lerp(0.92f, 1.08f, FMath::Clamp((HeightAlpha + 1.0f) * 0.5f, 0.0f, 1.0f));
		const float MoistureShade = FMath::Lerp(0.96f, 1.06f, FMath::Clamp(Moisture, 0.0f, 1.0f));
		const float TemperatureShade = FMath::Lerp(0.97f, 1.03f, FMath::Clamp(Temperature, 0.0f, 1.0f));
		BaseColor *= HeightShade * MoistureShade * TemperatureShade;
		BaseColor.A = 1.0f;
		return BaseColor;
	}

	int32 GetSafeNoiseOctavesForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Clamp(Settings.NoiseOctaves, 1, 8);
	}

	int32 GetSafeNoiseOctavesForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.SafeNoiseOctaves;
	}

	float GetSafeNoisePersistenceForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Clamp(Settings.NoisePersistence, 0.0f, 1.0f);
	}

	float GetSafeNoisePersistenceForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.SafeNoisePersistence;
	}

	float GetSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Max(0.0f, Settings.DynamicMeshHeight);
	}

	float GetSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.SafeDynamicMeshHeight;
	}

	float GetInvSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		const float SafeDynamicMeshHeight = GetSafeDynamicMeshHeightForSettings(Settings);
		return SafeDynamicMeshHeight > KINDA_SMALL_NUMBER ? 1.0f / SafeDynamicMeshHeight : 0.0f;
	}

	float GetInvSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.InvSafeDynamicMeshHeight;
	}

	float GetMountainHeightStrengthScaleForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Pow(FMath::Clamp(Settings.MountainStrength / 2.0f, 0.25f, 2.0f), 0.45f);
	}

	float GetMountainHeightStrengthScaleForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.MountainHeightStrengthScale;
	}

	float GetClampedValleyStrengthForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Clamp(Settings.ValleyStrength, 0.0f, 1.0f);
	}

	float GetClampedValleyStrengthForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.ClampedValleyStrength;
	}

	float GetClampedDetailStrengthForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Clamp(Settings.DetailStrength, 0.0f, 1.0f);
	}

	float GetClampedDetailStrengthForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.ClampedDetailStrength;
	}

	template <typename TBiomeData>
	ESRPlanetBiome GetRuntimeBiomeForBiomeDataAsset(const TBiomeData& BiomeDataAsset)
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

	template <typename TSettings>
	FVector ApplyDomainWarpForSettings(const FVector& LocalUnitDirection, const TSettings& Settings, float Strength)
	{
		const float SafeStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
		if (SafeStrength <= KINDA_SMALL_NUMBER)
		{
			return LocalUnitDirection;
		}

		const float WarpFrequency = FMath::Max(0.01f, Settings.ContinentFrequency * 2.0f);
		const FVector Warp(
			SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 211, WarpFrequency, 3, 0.5f),
			SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 223, WarpFrequency, 3, 0.5f),
			SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 227, WarpFrequency, 3, 0.5f));

		return (LocalUnitDirection + (Warp * SafeStrength * 0.42f)).GetSafeNormal();
	}

	FVector ApplyCompiledDomainWarpForSettings(
		const FVector& LocalUnitDirection,
		const FSRCompiledTerrainNoiseDescriptor (&WarpNoise)[3],
		float SafeStrength)
	{
		if (SafeStrength <= KINDA_SMALL_NUMBER)
		{
			return LocalUnitDirection;
		}

		const FVector Warp(
			SampleFractalNoiseUnitDirection(LocalUnitDirection, WarpNoise[0]),
			SampleFractalNoiseUnitDirection(LocalUnitDirection, WarpNoise[1]),
			SampleFractalNoiseUnitDirection(LocalUnitDirection, WarpNoise[2]));

		return (LocalUnitDirection + (Warp * SafeStrength * 0.42f)).GetSafeNormal();
	}

	template <typename TSettings>
	FVector ApplyClimateDomainWarpForSettings(const FVector& LocalUnitDirection, const TSettings& Settings)
	{
		return ApplyDomainWarpForSettings(LocalUnitDirection, Settings, Settings.NoiseStrength * 0.55f);
	}

	FVector ApplyClimateDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return ApplyCompiledDomainWarpForSettings(LocalUnitDirection, Settings.ClimateWarpNoise, Settings.ClimateWarpStrength);
	}

	template <typename TSettings>
	FVector ApplyTerrainDomainWarpForSettings(const FVector& LocalUnitDirection, const TSettings& Settings)
	{
		return ApplyDomainWarpForSettings(LocalUnitDirection, Settings, Settings.NoiseStrength);
	}

	FVector ApplyTerrainDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return ApplyCompiledDomainWarpForSettings(LocalUnitDirection, Settings.TerrainWarpNoise, Settings.TerrainWarpStrength);
	}

	template <typename TSettings>
	float SampleRiverMaskForSettings(const FVector& LocalUnitDirection, const TSettings& Settings, float LandMask, float MountainMask)
	{
		const float Strength = FMath::Clamp(Settings.RiverStrength, 0.0f, 1.0f);
		if (Strength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float ChannelA = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 263, Settings.ContinentFrequency * 5.5f, 4, 0.55f));
		const float ChannelB = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 269, Settings.ContinentFrequency * 9.0f, 3, 0.48f));
		const float LargeChannel = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.018f, 0.082f, ChannelA);
		const float Tributary = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.012f, 0.052f, ChannelB);
		const float SourceMask = FSRPlanetTerrainGenerator::SmoothStep(0.18f, 0.75f, MountainMask);
		const float RiverMask = FMath::Max(LargeChannel, Tributary * 0.55f) * FMath::Lerp(0.45f, 1.0f, SourceMask) * LandMask;
		return FMath::Clamp(RiverMask * Strength, 0.0f, 1.0f);
	}

	float SampleRiverMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings, float LandMask, float MountainMask)
	{
		if (Settings.ClampedRiverStrength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float ChannelA = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.RiverNoise[0]));
		const float ChannelB = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.RiverNoise[1]));
		const float LargeChannel = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.018f, 0.082f, ChannelA);
		const float Tributary = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.012f, 0.052f, ChannelB);
		const float SourceMask = FSRPlanetTerrainGenerator::SmoothStep(0.18f, 0.75f, MountainMask);
		const float RiverMask = FMath::Max(LargeChannel, Tributary * 0.55f) * FMath::Lerp(0.45f, 1.0f, SourceMask) * LandMask;
		return FMath::Clamp(RiverMask * Settings.ClampedRiverStrength, 0.0f, 1.0f);
	}

	template <typename TSettings>
	float SampleLakeMaskForSettings(const FVector& LocalUnitDirection, const TSettings& Settings, float LandMask, float HeightAlpha)
	{
		const float Strength = FMath::Clamp(Settings.LakeStrength, 0.0f, 1.0f);
		if (Strength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float BasinNoiseA = (SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 277, Settings.ContinentFrequency * 5.2f, 3, 0.42f) + 1.0f) * 0.5f;
		const float BasinNoiseB = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 283, Settings.ContinentFrequency * 8.5f, 2, 0.36f));
		const float BasinMask = FSRPlanetTerrainGenerator::SmoothStep(0.84f, 0.98f, BasinNoiseA) * FSRPlanetTerrainGenerator::SmoothStep(0.08f, 0.28f, BasinNoiseB);
		const float LowlandMask = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.04f, 0.30f, HeightAlpha);
		const float InlandMask = FSRPlanetTerrainGenerator::SmoothStep(0.72f, 0.94f, LandMask);
		return FMath::Clamp(BasinMask * LowlandMask * InlandMask * Strength * 0.45f, 0.0f, 1.0f);
	}

	float SampleLakeMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings, float LandMask, float HeightAlpha)
	{
		if (Settings.ClampedLakeStrength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float BasinNoiseA = (SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.LakeNoise[0]) + 1.0f) * 0.5f;
		const float BasinNoiseB = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.LakeNoise[1]));
		const float BasinMask = FSRPlanetTerrainGenerator::SmoothStep(0.84f, 0.98f, BasinNoiseA) * FSRPlanetTerrainGenerator::SmoothStep(0.08f, 0.28f, BasinNoiseB);
		const float LowlandMask = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.04f, 0.30f, HeightAlpha);
		const float InlandMask = FSRPlanetTerrainGenerator::SmoothStep(0.72f, 0.94f, LandMask);
		return FMath::Clamp(BasinMask * LowlandMask * InlandMask * Settings.ClampedLakeStrength * 0.45f, 0.0f, 1.0f);
	}

	float ComputeMinecraftPeaksAndValleysForSettings(float Weirdness)
	{
		const float AbsWeirdness = FMath::Abs(FMath::Clamp(Weirdness, -1.0f, 1.0f));
		return -(FMath::Abs(AbsWeirdness - (2.0f / 3.0f)) - (1.0f / 3.0f)) * 3.0f;
	}

	bool ShouldSampleRareRegionNoise(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.bUsesRareRegionPlacementMetric;
	}

	bool ShouldSampleRareRegionNoise(const FSRDynamicMeshGeneration& Settings)
	{
		(void)Settings;
		return true;
	}

	float SampleContinentalnessForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10001, Settings.ContinentFrequency * 0.58f, FMath::Max(3, SafeOctaves), 0.56f);
	}

	float SampleContinentalnessForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves)
	{
		(void)SafeOctaves;
		return SampleFractalNoiseUnitDirection(Direction, Settings.ContinentalnessNoise);
	}

	float SampleErosionForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10037, Settings.ContinentFrequency * 1.18f, FMath::Max(3, SafeOctaves - 1), 0.52f);
	}

	float SampleErosionForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves)
	{
		(void)SafeOctaves;
		return SampleFractalNoiseUnitDirection(Direction, Settings.ErosionNoise);
	}

	float SampleWeirdnessForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10061, Settings.MountainFrequency * 0.42f, FMath::Max(3, SafeOctaves), 0.50f);
	}

	float SampleWeirdnessForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves)
	{
		(void)SafeOctaves;
		return SampleFractalNoiseUnitDirection(Direction, Settings.WeirdnessNoise);
	}

	float SampleRidgesForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves)
	{
		return SampleRidgedNoiseUnitDirection(Direction, Settings.GenerationSeed + 10091, Settings.MountainFrequency * 0.72f, FMath::Max(3, SafeOctaves - 1));
	}

	float SampleRidgesForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves)
	{
		(void)SafeOctaves;
		return SampleRidgedNoiseUnitDirection(Direction, Settings.RidgesNoise);
	}

	float SampleDetailForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves, float SafePersistence)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10111, Settings.DetailFrequency, FMath::Max(2, SafeOctaves - 2), SafePersistence);
	}

	float SampleDetailForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves, float SafePersistence)
	{
		(void)SafeOctaves;
		(void)SafePersistence;
		return SampleFractalNoiseUnitDirection(Direction, Settings.DetailNoise);
	}

	float SampleTemperatureForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10141, Settings.TemperatureFrequency, 3, 0.5f);
	}

	float SampleTemperatureForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.TemperatureNoise);
	}

	float SampleHumidityForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10163, Settings.MoistureFrequency, 3, 0.5f);
	}

	float SampleHumidityForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.HumidityNoise);
	}

	float SampleRareRegionForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 503, Settings.ContinentFrequency * 3.75f, 3, 0.54f);
	}

	float SampleRareRegionForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.RareRegionNoise);
	}

	template <typename TSettings, typename TSelectBiome>
	FSRPlanetTerrainSample SampleDefaultTerrainForSettings(
		const FSRBiomeSampleContext& Context,
		const TSettings& Settings,
		TSelectBiome SelectBiome)
	{
		FSRPlanetTerrainSample Sample;

		const FVector Direction = Context.LocalUnitDirection.GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			return Sample;
		}

		const int32 SafeOctaves = GetSafeNoiseOctavesForSettings(Settings);
		const float SafePersistence = GetSafeNoisePersistenceForSettings(Settings);
		const float SafeDynamicMeshHeight = GetSafeDynamicMeshHeightForSettings(Settings);
		const float InvSafeDynamicMeshHeight = GetInvSafeDynamicMeshHeightForSettings(Settings);
		const FVector ClimateDirection = ApplyClimateDomainWarpForSettings(Direction, Settings);
		const FVector TerrainDirection = ApplyTerrainDomainWarpForSettings(Direction, Settings);

		const float Continentalness = SampleContinentalnessForSettings(ClimateDirection, Settings, SafeOctaves);
		const float ErosionNoise = SampleErosionForSettings(ClimateDirection, Settings, SafeOctaves);
		const float Weirdness = SampleWeirdnessForSettings(TerrainDirection, Settings, SafeOctaves);
		const float Ridges = SampleRidgesForSettings(TerrainDirection, Settings, SafeOctaves);
		const float Detail = SampleDetailForSettings(TerrainDirection, Settings, SafeOctaves, SafePersistence);
		const float TemperatureNoise = SampleTemperatureForSettings(ClimateDirection, Settings);
		const float HumidityNoise = SampleHumidityForSettings(ClimateDirection, Settings);

		const float ContinentalnessBias = 0.18f;
		const float EffectiveContinentalness = FMath::Clamp(Continentalness + ContinentalnessBias - (Settings.OceanThreshold * 0.62f), -1.0f, 1.0f);
		const float Erosion = FMath::Clamp((ErosionNoise + 1.0f) * 0.5f, 0.0f, 1.0f);
		const float PeaksAndValleys = ComputeMinecraftPeaksAndValleysForSettings(Weirdness);
		const float LandMask = FSRPlanetTerrainGenerator::SmoothStep(-0.12f, 0.02f, EffectiveContinentalness);
		const float CoastMask = 1.0f - FMath::Abs(FMath::Clamp(EffectiveContinentalness / 0.14f, -1.0f, 1.0f));
		const float OceanDepthMask = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(-0.58f, -0.12f, EffectiveContinentalness);
		const float InlandMask = FSRPlanetTerrainGenerator::SmoothStep(0.00f, 0.36f, EffectiveContinentalness);
		const float MountainSuppressionByErosion = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.36f, 0.86f, Erosion);
		const float MountainPotential = FMath::Clamp(
			FMath::Max(FSRPlanetTerrainGenerator::SmoothStep(0.18f, 0.90f, PeaksAndValleys), Ridges * 0.72f)
			* MountainSuppressionByErosion
			* LandMask,
			0.0f,
			1.0f);
		const float ValleyMask = FSRPlanetTerrainGenerator::SmoothStep(-0.95f, -0.18f, -PeaksAndValleys) * LandMask;
		const float PlateauMask = FSRPlanetTerrainGenerator::SmoothStep(0.18f, 0.72f, EffectiveContinentalness) * FSRPlanetTerrainGenerator::SmoothStep(0.28f, 0.68f, Erosion);

		const float OceanFloorHeight = -OceanDepthMask * OceanDepthMask * SafeDynamicMeshHeight * 0.42f;
		const float LandLift = LandMask * SafeDynamicMeshHeight * 0.10f;
		const float CoastalShelfHeight = CoastMask * SafeDynamicMeshHeight * 0.08f;
		const float PlainsHeight = LandMask * SafeDynamicMeshHeight * FMath::Lerp(0.14f, 0.28f, InlandMask) * FSRPlanetTerrainGenerator::SmoothStep(0.12f, 0.78f, Erosion);
		const float PlateauHeight = PlateauMask * SafeDynamicMeshHeight * 0.24f;
		const float MountainHeight = MountainPotential
			* SafeDynamicMeshHeight
			* FMath::Lerp(0.48f, 0.92f, FMath::Clamp(PeaksAndValleys, 0.0f, 1.0f))
			* GetMountainHeightStrengthScaleForSettings(Settings);
		const float ValleyCarve = ValleyMask * SafeDynamicMeshHeight * FMath::Lerp(0.10f, 0.31f, 1.0f - Erosion) * GetClampedValleyStrengthForSettings(Settings);
		const float DetailHeight = Detail * SafeDynamicMeshHeight * 0.035f * GetClampedDetailStrengthForSettings(Settings) * FMath::Lerp(0.35f, 1.0f, LandMask);
		const float RidgeBonus = FMath::Square(Ridges) * MountainPotential * SafeDynamicMeshHeight * 0.16f;

		const float HeightBeforeSurfaceRules = OceanFloorHeight + LandLift + CoastalShelfHeight + PlainsHeight + PlateauHeight + MountainHeight + RidgeBonus + DetailHeight - ValleyCarve;
		const float HeightAlphaBeforeRules = FMath::Clamp(HeightBeforeSurfaceRules * InvSafeDynamicMeshHeight, -1.0f, 1.0f);
		const float RiverMask = SampleRiverMaskForSettings(TerrainDirection, Settings, LandMask, MountainPotential);
		const float LakeMask = SampleLakeMaskForSettings(TerrainDirection, Settings, LandMask, HeightAlphaBeforeRules);
		const float SurfaceRuleCarve = ((RiverMask * 0.13f) + (LakeMask * 0.08f)) * SafeDynamicMeshHeight;

		Sample.HeightOffset = HeightBeforeSurfaceRules - SurfaceRuleCarve;
		Sample.Continent = EffectiveContinentalness;
		Sample.MountainMask = MountainPotential;
		Sample.RiverMask = RiverMask;
		Sample.LakeMask = LakeMask;
		Sample.PlateBeltMask = Ridges * MountainPotential;

		const float LatitudeTemperature = 1.0f - FMath::Abs(Direction.Z);
		const float HeightTemperaturePenalty = FMath::Max(0.0f, Sample.HeightOffset * InvSafeDynamicMeshHeight) * 0.28f;
		Sample.Temperature = FMath::Clamp((LatitudeTemperature * 0.78f) + (TemperatureNoise * 0.18f) + 0.11f - HeightTemperaturePenalty, 0.0f, 1.0f);
		Sample.Moisture = FMath::Clamp((HumidityNoise + 1.0f) * 0.5f, 0.0f, 1.0f);

		const float HeightAlpha = FMath::Clamp(Sample.HeightOffset * InvSafeDynamicMeshHeight, -1.0f, 1.0f);
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
		Metrics.RareRegionNoise = ShouldSampleRareRegionNoise(Settings)
			? FMath::Clamp((SampleRareRegionForSettings(Direction, Settings) + 1.0f) * 0.5f, 0.0f, 1.0f)
			: 0.0f;

		if (const auto* BiomeDataAsset = SelectBiome(Context, Settings, Metrics, Direction))
		{
			Sample.Biome = GetRuntimeBiomeForBiomeDataAsset(*BiomeDataAsset);
			Sample.BiomeId = BiomeDataAsset->BiomeId;
			Sample.WaterRole = GetWaterRoleForBiomeDataAsset(*BiomeDataAsset);
			Sample.SurfaceColor = GetBiomeDataAssetColor(*BiomeDataAsset, HeightAlpha, Sample.Moisture, Sample.Temperature);
		}
		else
		{
			Sample.Biome = ESRPlanetBiome::Plains;
			Sample.BiomeId = FName(TEXT("Plains"));
			Sample.WaterRole = ESRBiomeWaterRole::None;
			Sample.SurfaceColor = FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome::Plains, HeightAlpha, Sample.Moisture, Sample.Temperature);
			LogNoMatchingBiomeOnce(Settings, Context);
		}

		if (Sample.WaterRole == ESRBiomeWaterRole::Coast && (RiverMask > 0.58f || LakeMask > 0.38f))
		{
			const float WaterMask = FMath::Clamp(FMath::Max(RiverMask, LakeMask), 0.0f, 1.0f);
			Sample.SurfaceColor = FLinearColor::LerpUsingHSV(Sample.SurfaceColor, FLinearColor(0.03f, 0.18f, 0.34f, 1.0f), WaterMask * 0.74f);
		}

		Sample.SurfaceColor.A = 1.0f;
		return Sample;
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

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGenerationSnapshot& Settings)
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
	return SampleDefaultTerrainForSettings(Context, Settings, SelectBiomeDataAsset);
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleDefaultTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGenerationSnapshot& Settings)
{
	return SampleDefaultTerrainForSettings(Context, Settings, SelectBiomeDataSnapshot);
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
