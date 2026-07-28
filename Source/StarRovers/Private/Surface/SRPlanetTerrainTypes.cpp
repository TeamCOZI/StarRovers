#include "Surface/SRPlanetTerrainTypes.h"

#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Surface/SRPlanetBiomeDataAsset.h"

namespace
{
	uint32 HashCompiledBiomeValue(FName BiomeId, int32 Salt)
	{
		const FString HashInput = FString::Printf(TEXT("%s:%d"), *BiomeId.ToString(), Salt);
		return FCrc::StrCrc32(*HashInput);
	}

	float HashCompiledBiomeUnit(FName BiomeId, int32 Salt)
	{
		return static_cast<float>(HashCompiledBiomeValue(BiomeId, Salt) & 0x00ffffff) / static_cast<float>(0x00ffffff);
	}

	FVector BuildCompiledBiomeAnchorDirection(FName BiomeId, int32 Salt)
	{
		const float Z = (HashCompiledBiomeUnit(BiomeId, Salt) * 2.0f) - 1.0f;
		const float Angle = HashCompiledBiomeUnit(BiomeId, Salt + 97) * 2.0f * PI;
		const float Radius = FMath::Sqrt(FMath::Max(0.0f, 1.0f - (Z * Z)));
		return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Z).GetSafeNormal();
	}

	FVector BuildCompiledNoiseSeedOffset(int32 Seed)
	{
		const int64 Seed64 = static_cast<int64>(Seed);
		return FVector(
			static_cast<float>((Seed64 * 15731LL) % 10007LL),
			static_cast<float>((Seed64 * 789221LL) % 10009LL),
			static_cast<float>((Seed64 * 1376312589LL) % 10037LL));
	}

	FSRCompiledTerrainNoiseDescriptor MakeNoiseDescriptor(int32 Seed, float Frequency, int32 Octaves, float Persistence)
	{
		FSRCompiledTerrainNoiseDescriptor Descriptor;
		Descriptor.SeedOffset = BuildCompiledNoiseSeedOffset(Seed);
		Descriptor.Frequency = FMath::Max(0.01f, Frequency);
		Descriptor.Octaves = FMath::Clamp(Octaves, 1, 8);
		Descriptor.Persistence = FMath::Clamp(Persistence, 0.0f, 1.0f);
		return Descriptor;
	}

	void GetCompiledPlacementMetricRange(ESRBiomePlacementMetric Metric, float& OutMinValue, float& OutMaxValue)
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

	float NormalizeCompiledPlacementMetricValue(ESRBiomePlacementMetric Metric, float Value)
	{
		float MinValue = 0.0f;
		float MaxValue = 1.0f;
		GetCompiledPlacementMetricRange(Metric, MinValue, MaxValue);
		return FMath::GetMappedRangeValueClamped(FVector2D(MinValue, MaxValue), FVector2D(0.0f, 1.0f), Value);
	}

	bool HasMetricRule(const TArray<FSRBiomePlacementRule>& PlacementRules, ESRBiomePlacementMetric Metric)
	{
		for (const FSRBiomePlacementRule& Rule : PlacementRules)
		{
			if (Rule.Metric == Metric)
			{
				return true;
			}
		}
		return false;
	}

	float GetCompiledRuleTargetForMetric(
		FName BiomeId,
		const TArray<FSRBiomePlacementRule>& PlacementRules,
		ESRBiomePlacementMetric Metric,
		float FallbackTarget)
	{
		float MinValue = 0.0f;
		float MaxValue = 1.0f;
		GetCompiledPlacementMetricRange(Metric, MinValue, MaxValue);

		float TargetSum = 0.0f;
		int32 TargetCount = 0;
		for (const FSRBiomePlacementRule& Rule : PlacementRules)
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
				TargetValue = HashCompiledBiomeUnit(BiomeId, static_cast<int32>(Metric) + 211) < 0.5f
					? (MinValue + LowerThreshold) * 0.5f
					: (UpperThreshold + MaxValue) * 0.5f;
				break;
			default:
				bHasTarget = false;
				break;
			}

			if (bHasTarget)
			{
				TargetSum += NormalizeCompiledPlacementMetricValue(Metric, TargetValue);
				++TargetCount;
			}
		}

		return TargetCount > 0 ? TargetSum / static_cast<float>(TargetCount) : FallbackTarget;
	}

	float GetRuleBasedTargetTemperature(FName BiomeId, const TArray<FSRBiomePlacementRule>& PlacementRules)
	{
		const float FallbackTarget = HashCompiledBiomeUnit(BiomeId, 17);
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::Temperature))
		{
			return GetCompiledRuleTargetForMetric(BiomeId, PlacementRules, ESRBiomePlacementMetric::Temperature, FallbackTarget);
		}

		const float LatitudeTarget = GetCompiledRuleTargetForMetric(BiomeId, PlacementRules, ESRBiomePlacementMetric::AbsLatitudeDegrees, -1.0f);
		return LatitudeTarget >= 0.0f ? 1.0f - LatitudeTarget : FallbackTarget;
	}

	float GetRuleBasedTargetMoisture(FName BiomeId, const TArray<FSRBiomePlacementRule>& PlacementRules)
	{
		const float FallbackTarget = HashCompiledBiomeUnit(BiomeId, 23);
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::Moisture))
		{
			return GetCompiledRuleTargetForMetric(BiomeId, PlacementRules, ESRBiomePlacementMetric::Moisture, FallbackTarget);
		}
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::RiverMask)
			|| HasMetricRule(PlacementRules, ESRBiomePlacementMetric::LakeMask))
		{
			return 0.82f;
		}

		return FallbackTarget;
	}

	float GetRuleBasedTargetHeight(FName BiomeId, const TArray<FSRBiomePlacementRule>& PlacementRules)
	{
		const float FallbackTarget = HashCompiledBiomeUnit(BiomeId, 31);
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::HeightAlpha))
		{
			return GetCompiledRuleTargetForMetric(BiomeId, PlacementRules, ESRBiomePlacementMetric::HeightAlpha, FallbackTarget);
		}
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::OceanDepthMask))
		{
			return 0.08f;
		}
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::CoastMask))
		{
			return 0.38f;
		}
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::MountainMask))
		{
			return 0.82f;
		}

		return FallbackTarget;
	}

	float GetRuleBasedTargetContinentalness(FName BiomeId, const TArray<FSRBiomePlacementRule>& PlacementRules)
	{
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::Continentalness))
		{
			return GetCompiledRuleTargetForMetric(BiomeId, PlacementRules, ESRBiomePlacementMetric::Continentalness, HashCompiledBiomeUnit(BiomeId, 43));
		}
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::OceanDepthMask))
		{
			return 0.08f;
		}
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::CoastMask))
		{
			return 0.46f;
		}
		if (HasMetricRule(PlacementRules, ESRBiomePlacementMetric::LandMask)
			|| HasMetricRule(PlacementRules, ESRBiomePlacementMetric::InlandMask))
		{
			return 0.76f;
		}

		return HashCompiledBiomeUnit(BiomeId, 43);
	}

	ESRPlanetBiome GetRuntimeBiomeForWaterRole(ESRBiomeWaterRole WaterRole)
	{
		if (WaterRole == ESRBiomeWaterRole::Ocean
			|| WaterRole == ESRBiomeWaterRole::River
			|| WaterRole == ESRBiomeWaterRole::Lake)
		{
			return ESRPlanetBiome::Ocean;
		}
		if (WaterRole == ESRBiomeWaterRole::Coast)
		{
			return ESRPlanetBiome::Coast;
		}
		return ESRPlanetBiome::Plains;
	}

	FSRCompiledPlanetBiomeGenerationSnapshot CompileBiomeGenerationSnapshot(
		const FSRPlanetBiomeGenerationSnapshot& BiomeSnapshot,
		int32 GenerationSeed)
	{
		FSRCompiledPlanetBiomeGenerationSnapshot CompiledBiome;
		CompiledBiome.BiomeId = BiomeSnapshot.BiomeId;
		CompiledBiome.WaterRole = BiomeSnapshot.WaterRole;
		CompiledBiome.PlacementRules = BiomeSnapshot.PlacementRules;
		CompiledBiome.Weight = FMath::Max(0.01f, BiomeSnapshot.SpawnWeight);
		CompiledBiome.Priority = BiomeSnapshot.Priority;
		CompiledBiome.bCanOverrideLowerPriorityBiomes = BiomeSnapshot.bCanOverrideLowerPriorityBiomes;
		CompiledBiome.OverrideMinScore = BiomeSnapshot.OverrideMinScore;
		CompiledBiome.TargetTemperature = GetRuleBasedTargetTemperature(BiomeSnapshot.BiomeId, BiomeSnapshot.PlacementRules);
		CompiledBiome.TargetMoisture = GetRuleBasedTargetMoisture(BiomeSnapshot.BiomeId, BiomeSnapshot.PlacementRules);
		CompiledBiome.TargetHeight = GetRuleBasedTargetHeight(BiomeSnapshot.BiomeId, BiomeSnapshot.PlacementRules);
		CompiledBiome.TargetContinentalness = GetRuleBasedTargetContinentalness(BiomeSnapshot.BiomeId, BiomeSnapshot.PlacementRules);

		const float SafeRegionSize = FMath::Clamp(BiomeSnapshot.RegionSize, 0.01f, 1.0f);
		CompiledBiome.AnchorThreshold = FMath::Lerp(0.98f, -0.12f, SafeRegionSize);
		CompiledBiome.AnchorDirections[0] = BuildCompiledBiomeAnchorDirection(BiomeSnapshot.BiomeId, GenerationSeed);
		CompiledBiome.AnchorDirections[1] = BuildCompiledBiomeAnchorDirection(BiomeSnapshot.BiomeId, GenerationSeed + 131);
		CompiledBiome.PatchFrequency = FMath::Lerp(1.5f, 8.5f, HashCompiledBiomeUnit(BiomeSnapshot.BiomeId, 59));
		CompiledBiome.PatchSeed = GenerationSeed + static_cast<int32>(HashCompiledBiomeValue(BiomeSnapshot.BiomeId, 67) % 100000);
		CompiledBiome.PatchSeedOffset = BuildCompiledNoiseSeedOffset(CompiledBiome.PatchSeed);
		const uint8 Hue = static_cast<uint8>(HashCompiledBiomeValue(BiomeSnapshot.BiomeId, 701) % 255);
		CompiledBiome.BaseLandColor = FLinearColor::MakeFromHSV8(Hue, 112, 158);
		CompiledBiome.BaseLandColor.A = 1.0f;
		CompiledBiome.RuntimeBiome = GetRuntimeBiomeForWaterRole(BiomeSnapshot.WaterRole);
		return CompiledBiome;
	}

	void CompileTerrainNoiseSnapshot(FSRDynamicMeshGenerationSnapshot& Snapshot)
	{
		Snapshot.SafeNoiseOctaves = FMath::Clamp(Snapshot.NoiseOctaves, 1, 8);
		Snapshot.SafeNoisePersistence = FMath::Clamp(Snapshot.NoisePersistence, 0.0f, 1.0f);
		Snapshot.SafeDynamicMeshHeight = FMath::Max(0.0f, Snapshot.DynamicMeshHeight);
		Snapshot.InvSafeDynamicMeshHeight = Snapshot.SafeDynamicMeshHeight > KINDA_SMALL_NUMBER ? 1.0f / Snapshot.SafeDynamicMeshHeight : 0.0f;
		Snapshot.ClimateWarpStrength = FMath::Clamp(Snapshot.NoiseStrength * 0.55f, 0.0f, 1.0f);
		Snapshot.TerrainWarpStrength = FMath::Clamp(Snapshot.NoiseStrength, 0.0f, 1.0f);
		Snapshot.MountainHeightStrengthScale = FMath::Pow(FMath::Clamp(Snapshot.MountainStrength / 2.0f, 0.25f, 2.0f), 0.45f);
		Snapshot.ClampedValleyStrength = FMath::Clamp(Snapshot.ValleyStrength, 0.0f, 1.0f);
		Snapshot.ClampedDetailStrength = FMath::Clamp(Snapshot.DetailStrength, 0.0f, 1.0f);
		Snapshot.ClampedRiverStrength = FMath::Clamp(Snapshot.RiverStrength, 0.0f, 1.0f);
		Snapshot.ClampedLakeStrength = FMath::Clamp(Snapshot.LakeStrength, 0.0f, 1.0f);

		const float WarpFrequency = Snapshot.ContinentFrequency * 2.0f;
		Snapshot.ClimateWarpNoise[0] = MakeNoiseDescriptor(Snapshot.GenerationSeed + 211, WarpFrequency, 3, 0.5f);
		Snapshot.ClimateWarpNoise[1] = MakeNoiseDescriptor(Snapshot.GenerationSeed + 223, WarpFrequency, 3, 0.5f);
		Snapshot.ClimateWarpNoise[2] = MakeNoiseDescriptor(Snapshot.GenerationSeed + 227, WarpFrequency, 3, 0.5f);
		Snapshot.TerrainWarpNoise[0] = Snapshot.ClimateWarpNoise[0];
		Snapshot.TerrainWarpNoise[1] = Snapshot.ClimateWarpNoise[1];
		Snapshot.TerrainWarpNoise[2] = Snapshot.ClimateWarpNoise[2];

		Snapshot.ContinentalnessNoise = MakeNoiseDescriptor(Snapshot.GenerationSeed + 10001, Snapshot.ContinentFrequency * 0.58f, FMath::Max(3, Snapshot.SafeNoiseOctaves), 0.56f);
		Snapshot.ErosionNoise = MakeNoiseDescriptor(Snapshot.GenerationSeed + 10037, Snapshot.ContinentFrequency * 1.18f, FMath::Max(3, Snapshot.SafeNoiseOctaves - 1), 0.52f);
		Snapshot.WeirdnessNoise = MakeNoiseDescriptor(Snapshot.GenerationSeed + 10061, Snapshot.MountainFrequency * 0.42f, FMath::Max(3, Snapshot.SafeNoiseOctaves), 0.50f);
		Snapshot.RidgesNoise = MakeNoiseDescriptor(Snapshot.GenerationSeed + 10091, Snapshot.MountainFrequency * 0.72f, FMath::Max(3, Snapshot.SafeNoiseOctaves - 1), 0.5f);
		Snapshot.DetailNoise = MakeNoiseDescriptor(Snapshot.GenerationSeed + 10111, Snapshot.DetailFrequency, FMath::Max(2, Snapshot.SafeNoiseOctaves - 2), Snapshot.SafeNoisePersistence);
		Snapshot.TemperatureNoise = MakeNoiseDescriptor(Snapshot.GenerationSeed + 10141, Snapshot.TemperatureFrequency, 3, 0.5f);
		Snapshot.HumidityNoise = MakeNoiseDescriptor(Snapshot.GenerationSeed + 10163, Snapshot.MoistureFrequency, 3, 0.5f);
		Snapshot.RiverNoise[0] = MakeNoiseDescriptor(Snapshot.GenerationSeed + 263, Snapshot.ContinentFrequency * 5.5f, 4, 0.55f);
		Snapshot.RiverNoise[1] = MakeNoiseDescriptor(Snapshot.GenerationSeed + 269, Snapshot.ContinentFrequency * 9.0f, 3, 0.48f);
		Snapshot.LakeNoise[0] = MakeNoiseDescriptor(Snapshot.GenerationSeed + 277, Snapshot.ContinentFrequency * 5.2f, 3, 0.42f);
		Snapshot.LakeNoise[1] = MakeNoiseDescriptor(Snapshot.GenerationSeed + 283, Snapshot.ContinentFrequency * 8.5f, 2, 0.36f);
		Snapshot.RareRegionNoise = MakeNoiseDescriptor(Snapshot.GenerationSeed + 503, Snapshot.ContinentFrequency * 3.75f, 3, 0.54f);
	}
}

FSRDynamicMeshGeneration::FSRDynamicMeshGeneration()
{
}

namespace
{
	template <typename SettingsType>
	FLinearColor GetTemperatureStateSurfaceColorForSettings(const SettingsType& Settings, ESRFacilityTemperatureState TemperatureState)
	{
		switch (TemperatureState)
		{
		case ESRFacilityTemperatureState::Frozen:
			return Settings.FrozenTemperatureStateSurfaceColor;
		case ESRFacilityTemperatureState::Cold:
			return Settings.ColdTemperatureStateSurfaceColor;
		case ESRFacilityTemperatureState::Hot:
			return Settings.HotTemperatureStateSurfaceColor;
		case ESRFacilityTemperatureState::Overheated:
			return Settings.OverheatedTemperatureStateSurfaceColor;
		case ESRFacilityTemperatureState::Normal:
		default:
			return Settings.NormalTemperatureStateSurfaceColor;
		}
	}

	template <typename SettingsType>
	FLinearColor ApplyTemperatureStateSurfaceColorForSettings(
		const SettingsType& Settings,
		const FLinearColor& BaseColor,
		ESRFacilityTemperatureState TemperatureState)
	{
		if (!Settings.bApplyTemperatureStateSurfaceColor)
		{
			return BaseColor;
		}

		const float BlendAlpha = FMath::Clamp(Settings.TemperatureStateSurfaceColorBlendAlpha, 0.0f, 1.0f);
		FLinearColor Result = FLinearColor::LerpUsingHSV(
			BaseColor,
			GetTemperatureStateSurfaceColorForSettings(Settings, TemperatureState),
			BlendAlpha);
		Result.A = BaseColor.A;
		return Result;
	}
}

FLinearColor FSRDynamicMeshGenerationSnapshot::GetTemperatureStateSurfaceColor(ESRFacilityTemperatureState TemperatureState) const
{
	return GetTemperatureStateSurfaceColorForSettings(*this, TemperatureState);
}

FLinearColor FSRDynamicMeshGenerationSnapshot::ApplyTemperatureStateSurfaceColor(
	const FLinearColor& BaseColor,
	ESRFacilityTemperatureState TemperatureState) const
{
	return ApplyTemperatureStateSurfaceColorForSettings(*this, BaseColor, TemperatureState);
}

FLinearColor FSRDynamicMeshGeneration::GetTemperatureStateSurfaceColor(ESRFacilityTemperatureState TemperatureState) const
{
	return GetTemperatureStateSurfaceColorForSettings(*this, TemperatureState);
}

FLinearColor FSRDynamicMeshGeneration::ApplyTemperatureStateSurfaceColor(
	const FLinearColor& BaseColor,
	ESRFacilityTemperatureState TemperatureState) const
{
	return ApplyTemperatureStateSurfaceColorForSettings(*this, BaseColor, TemperatureState);
}

void FSRDynamicMeshGeneration::NormalizeBiomeMaterials(const TArray<TObjectPtr<USRPlanetBiomeDataAsset>>& AllowedBiomeDataAssets)
{
	BiomeDataAssets.Reset();
	BiomeDataAssets.Reserve(AllowedBiomeDataAssets.Num());

	TMap<FName, UMaterialInterface*> ExistingMaterialsById;
	TMap<USRPlanetBiomeDataAsset*, UMaterialInterface*> ExistingMaterialsByAsset;
	ExistingMaterialsById.Reserve(BiomeMaterials.Num());
	ExistingMaterialsByAsset.Reserve(BiomeMaterials.Num());
	for (const FSRBiomeMaterialEntry& Entry : BiomeMaterials)
	{
		if (IsValid(Entry.BiomeDataAsset.Get()) && !ExistingMaterialsByAsset.Contains(Entry.BiomeDataAsset.Get()))
		{
			ExistingMaterialsByAsset.Add(Entry.BiomeDataAsset.Get(), Entry.Material.Get());
		}
		if (!Entry.BiomeId.IsNone() && !ExistingMaterialsById.Contains(Entry.BiomeId))
		{
			ExistingMaterialsById.Add(Entry.BiomeId, Entry.Material.Get());
		}
	}

	BiomeMaterials.Reset();
	BiomeMaterials.Reserve(AllowedBiomeDataAssets.Num());
	TSet<USRPlanetBiomeDataAsset*> AddedBiomeDataAssets;
	AddedBiomeDataAssets.Reserve(AllowedBiomeDataAssets.Num());
	for (USRPlanetBiomeDataAsset* BiomeDataAsset : AllowedBiomeDataAssets)
	{
		if (!IsValid(BiomeDataAsset))
		{
			continue;
		}

		bool bAlreadyAdded = false;
		AddedBiomeDataAssets.Add(BiomeDataAsset, &bAlreadyAdded);
		if (!bAlreadyAdded)
		{
			BiomeDataAssets.Add(BiomeDataAsset);
		}

		FSRBiomeMaterialEntry Entry;
		Entry.BiomeId = BiomeDataAsset->BiomeId;
		Entry.BiomeDataAsset = BiomeDataAsset;
		if (UMaterialInterface* const* ExistingMaterial = ExistingMaterialsByAsset.Find(BiomeDataAsset))
		{
			Entry.Material = *ExistingMaterial;
		}
		else if (UMaterialInterface* const* ExistingMaterialById = ExistingMaterialsById.Find(Entry.BiomeId))
		{
			Entry.Material = *ExistingMaterialById;
		}
		BiomeMaterials.Add(Entry);
	}
}

UMaterialInterface* FSRDynamicMeshGeneration::GetBiomeMaterial(FName BiomeId) const
{
	if (BiomeId.IsNone())
	{
		return nullptr;
	}

	for (const FSRBiomeMaterialEntry& Entry : BiomeMaterials)
	{
		if (Entry.BiomeId == BiomeId)
		{
			return Entry.Material.Get();
		}
	}

	return nullptr;
}

int32 FSRDynamicMeshGeneration::GetBiomeMaterialSlotIndex(FName BiomeId) const
{
	if (BiomeId.IsNone())
	{
		return 0;
	}

	for (int32 MaterialIndex = 0; MaterialIndex < BiomeMaterials.Num(); ++MaterialIndex)
	{
		if (BiomeMaterials[MaterialIndex].BiomeId == BiomeId)
		{
			return MaterialIndex + 1;
		}
	}

	return 0;
}

FSRDynamicMeshGenerationSnapshot FSRDynamicMeshGeneration::MakeThreadSafeSnapshot() const
{
	FSRDynamicMeshGenerationSnapshot Snapshot;
	Snapshot.bDynamicMeshGeneration = bDynamicMeshGeneration;
	Snapshot.bMinecraft = bMinecraft;
	Snapshot.bClampTerrainHeightToOceanLevel = bClampTerrainHeightToOceanLevel;
	Snapshot.GenerationSeed = GenerationSeed;
	Snapshot.bRandomizeGenerationSeedEachRun = bRandomizeGenerationSeedEachRun;
	Snapshot.DynamicMeshHeight = DynamicMeshHeight;
	Snapshot.OceanThreshold = OceanThreshold;
	Snapshot.ContinentFrequency = ContinentFrequency;
	Snapshot.MountainFrequency = MountainFrequency;
	Snapshot.MountainStrength = MountainStrength;
	Snapshot.ValleyStrength = ValleyStrength;
	Snapshot.RiverStrength = RiverStrength;
	Snapshot.LakeStrength = LakeStrength;
	Snapshot.TemperatureFrequency = TemperatureFrequency;
	Snapshot.TemperatureBias = FMath::Clamp(TemperatureBias, -1.0f, 1.0f);
	Snapshot.bApplyTemperatureStateSurfaceColor = bApplyTemperatureStateSurfaceColor;
	Snapshot.TemperatureStateSurfaceColorBlendAlpha = TemperatureStateSurfaceColorBlendAlpha;
	Snapshot.FrozenTemperatureStateSurfaceColor = FrozenTemperatureStateSurfaceColor;
	Snapshot.ColdTemperatureStateSurfaceColor = ColdTemperatureStateSurfaceColor;
	Snapshot.NormalTemperatureStateSurfaceColor = NormalTemperatureStateSurfaceColor;
	Snapshot.HotTemperatureStateSurfaceColor = HotTemperatureStateSurfaceColor;
	Snapshot.OverheatedTemperatureStateSurfaceColor = OverheatedTemperatureStateSurfaceColor;
	Snapshot.MoistureFrequency = MoistureFrequency;
	Snapshot.MoistureBias = FMath::Clamp(MoistureBias, -1.0f, 1.0f);
	Snapshot.DetailFrequency = DetailFrequency;
	Snapshot.DetailStrength = DetailStrength;
	Snapshot.NoiseStrength = NoiseStrength;
	Snapshot.NoiseOctaves = NoiseOctaves;
	Snapshot.NoisePersistence = NoisePersistence;
	CompileTerrainNoiseSnapshot(Snapshot);
	Snapshot.Biomes.Reserve(BiomeDataAssets.Num());
	Snapshot.CompiledBiomes.Reserve(BiomeDataAssets.Num());

	for (const TObjectPtr<USRPlanetBiomeDataAsset>& BiomeDataAsset : BiomeDataAssets)
	{
		if (!IsValid(BiomeDataAsset.Get()))
		{
			continue;
		}

		FSRPlanetBiomeGenerationSnapshot BiomeSnapshot;
		BiomeSnapshot.BiomeId = BiomeDataAsset->BiomeId;
		BiomeSnapshot.WaterRole = BiomeDataAsset->WaterRole;
		BiomeSnapshot.PlacementRules = BiomeDataAsset->PlacementRules;
		BiomeSnapshot.SpawnWeight = BiomeDataAsset->SpawnWeight;
		BiomeSnapshot.RegionSize = BiomeDataAsset->RegionSize;
		BiomeSnapshot.Priority = BiomeDataAsset->Priority;
		BiomeSnapshot.bCanOverrideLowerPriorityBiomes = BiomeDataAsset->bCanOverrideLowerPriorityBiomes;
		BiomeSnapshot.OverrideMinScore = BiomeDataAsset->OverrideMinScore;
		for (const FSRBiomePlacementRule& PlacementRule : BiomeSnapshot.PlacementRules)
		{
			if (PlacementRule.Metric == ESRBiomePlacementMetric::RareRegionNoise)
			{
				Snapshot.bUsesRareRegionPlacementMetric = true;
				break;
			}
		}
		Snapshot.CompiledBiomes.Add(CompileBiomeGenerationSnapshot(BiomeSnapshot, Snapshot.GenerationSeed));
		Snapshot.Biomes.Add(MoveTemp(BiomeSnapshot));
	}

	return Snapshot;
}
