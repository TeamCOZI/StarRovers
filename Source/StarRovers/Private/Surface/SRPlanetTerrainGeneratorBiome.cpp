#include "Surface/SRPlanetTerrainGeneratorSampling.h"

#include "HAL/CriticalSection.h"
#include "Misc/Crc.h"
#include "Misc/ScopeLock.h"
#include "Surface/SRPlanetBiomeDataAsset.h"
#include "Utility/SRLog.h"

namespace StarRovers::Terrain
{
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
			bool bAlreadyLogged = false;
			GNoMatchingBiomeLogKeys.Add(LogKey, &bAlreadyLogged);
			if (bAlreadyLogged)
			{
				return;
			}
		}

		SR_LOG(Surface,
			LogTemp,
			Warning,
			TEXT("Terrain generation could not find a Profile BiomeDataAsset whose placement filters pass for at least one sampled cell. Falling back to Plains for unmatched cells. Seed=%d Biomes=%d ExampleFace=%d ExampleCell=(%d,%d)"),
			Settings.GenerationSeed,
			CountValidBiomes(Settings),
			static_cast<int32>(Context.Face),
			Context.CellX,
			Context.CellY);
	}

	template <typename TBiomeData>
	ESRBiomeWaterRole GetWaterRoleForBiomeDataAsset(const TBiomeData& BiomeDataAsset)
	{
		return BiomeDataAsset.WaterRole;
	}

	bool IsOpenWaterRole(ESRBiomeWaterRole WaterRole)
	{
		return WaterRole == ESRBiomeWaterRole::Ocean || WaterRole == ESRBiomeWaterRole::River || WaterRole == ESRBiomeWaterRole::Lake;
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

	void LogNoMatchingBiomeOnce(const FSRDynamicMeshGeneration& Settings, const FSRBiomeSampleContext& Context)
	{
		LogNoMatchingBiomeOnce<FSRDynamicMeshGeneration>(Settings, Context);
	}

	void LogNoMatchingBiomeOnce(const FSRDynamicMeshGenerationSnapshot& Settings, const FSRBiomeSampleContext& Context)
	{
		LogNoMatchingBiomeOnce<FSRDynamicMeshGenerationSnapshot>(Settings, Context);
	}

	ESRBiomeWaterRole GetWaterRoleForBiomeDataAsset(const USRPlanetBiomeDataAsset& BiomeDataAsset)
	{
		return GetWaterRoleForBiomeDataAsset<USRPlanetBiomeDataAsset>(BiomeDataAsset);
	}

	ESRBiomeWaterRole GetWaterRoleForBiomeDataAsset(const FSRCompiledPlanetBiomeGenerationSnapshot& BiomeDataAsset)
	{
		return GetWaterRoleForBiomeDataAsset<FSRCompiledPlanetBiomeGenerationSnapshot>(BiomeDataAsset);
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

	ESRPlanetBiome GetRuntimeBiomeForBiomeDataAsset(const FSRCompiledPlanetBiomeGenerationSnapshot& BiomeDataAsset)
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

	FLinearColor GetBiomeDataAssetColor(const USRPlanetBiomeDataAsset& BiomeDataAsset, float HeightAlpha, float Moisture, float Temperature)
	{
		return GetBiomeDataAssetColor<USRPlanetBiomeDataAsset>(BiomeDataAsset, HeightAlpha, Moisture, Temperature);
	}

} // namespace StarRovers::Terrain
