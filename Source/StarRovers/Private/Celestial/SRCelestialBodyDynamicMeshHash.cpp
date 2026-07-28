#include "Celestial/SRCelestialBody.h"

#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Misc/Crc.h"
#include "Surface/SRPlanetBiomeDataAsset.h"

namespace
{
	uint32 HashDynamicMeshBaseDataAssetSettings(uint32 Hash, const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset)
	{
		Hash = HashCombine(Hash, PointerHash(DynamicMeshBaseDataAsset));
		if (!IsValid(DynamicMeshBaseDataAsset))
		{
			return Hash;
		}

		Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(DynamicMeshBaseDataAsset->BaseShape)));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshBaseDataAsset->GetClampedFaceResolution()));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshBaseDataAsset->GetSafeBaseRadius()));
		return Hash;
	}

	uint32 HashLinearColor(uint32 Hash, const FLinearColor& Color)
	{
		Hash = HashCombine(Hash, ::GetTypeHash(Color.R));
		Hash = HashCombine(Hash, ::GetTypeHash(Color.G));
		Hash = HashCombine(Hash, ::GetTypeHash(Color.B));
		Hash = HashCombine(Hash, ::GetTypeHash(Color.A));
		return Hash;
	}

	uint32 HashDynamicMeshGenerationSettings(uint32 Hash, const FSRDynamicMeshGeneration& DynamicMeshGeneration)
	{
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.GenerationSeed));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.bDynamicMeshGeneration ? 1 : 0));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.bMinecraft ? 1 : 0));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.bClampTerrainHeightToOceanLevel ? 1 : 0));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.DynamicMeshHeight));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.ContinentFrequency));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.MountainFrequency));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.DetailFrequency));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.MoistureFrequency));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.MoistureBias));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.TemperatureFrequency));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.TemperatureBias));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.ValleyStrength));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.MountainStrength));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.NoiseStrength));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.RiverStrength));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.LakeStrength));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.DetailStrength));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.NoiseOctaves));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.NoisePersistence));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.OceanThreshold));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.AtmosphereThreshold));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.bApplyTemperatureStateSurfaceColor ? 1 : 0));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.TemperatureStateSurfaceColorBlendAlpha));
		Hash = HashLinearColor(Hash, DynamicMeshGeneration.FrozenTemperatureStateSurfaceColor);
		Hash = HashLinearColor(Hash, DynamicMeshGeneration.ColdTemperatureStateSurfaceColor);
		Hash = HashLinearColor(Hash, DynamicMeshGeneration.NormalTemperatureStateSurfaceColor);
		Hash = HashLinearColor(Hash, DynamicMeshGeneration.HotTemperatureStateSurfaceColor);
		Hash = HashLinearColor(Hash, DynamicMeshGeneration.OverheatedTemperatureStateSurfaceColor);
		return Hash;
	}

	uint32 HashToonOutlineSettings(uint32 Hash, const FSRToonOutlineSettings& ToonOutlineSettings)
	{
		Hash = HashCombine(Hash, ::GetTypeHash(ToonOutlineSettings.bEnableToonOutline ? 1 : 0));
		Hash = HashCombine(Hash, ::GetTypeHash(ToonOutlineSettings.bUseFeatureEdgeToonOutline ? 1 : 0));
		Hash = HashCombine(Hash, ::GetTypeHash(ToonOutlineSettings.ToonLineThickness));
		Hash = HashCombine(Hash, ::GetTypeHash(ToonOutlineSettings.ToonLineColor.R));
		Hash = HashCombine(Hash, ::GetTypeHash(ToonOutlineSettings.ToonLineColor.G));
		Hash = HashCombine(Hash, ::GetTypeHash(ToonOutlineSettings.ToonLineColor.B));
		Hash = HashCombine(Hash, ::GetTypeHash(ToonOutlineSettings.ToonLineColor.A));
		Hash = HashCombine(Hash, ::GetTypeHash(ToonOutlineSettings.FeatureEdgeAngleThresholdDegrees));
		return Hash;
	}

	uint32 HashBiomeDataAssetSettings(uint32 Hash, const USRPlanetBiomeDataAsset* BiomeDataAsset)
	{
		Hash = HashCombine(Hash, PointerHash(BiomeDataAsset));
		if (!IsValid(BiomeDataAsset))
		{
			return Hash;
		}

		Hash = HashCombine(Hash, FCrc::StrCrc32(*BiomeDataAsset->BiomeId.ToString()));
		Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(BiomeDataAsset->WaterRole)));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->SpawnWeight));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->RegionSize));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->Priority));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->bCanOverrideLowerPriorityBiomes ? 1 : 0));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->OverrideMinScore));

		for (const FSRBiomePlacementRule& Rule : BiomeDataAsset->PlacementRules)
		{
			Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(Rule.Metric)));
			Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(Rule.Comparison)));
			Hash = HashCombine(Hash, ::GetTypeHash(Rule.Threshold));
			Hash = HashCombine(Hash, ::GetTypeHash(Rule.MaxThreshold));
		}

		return Hash;
	}

	uint32 HashBiomeMaterialEntries(uint32 Hash, const TArray<FSRBiomeMaterialEntry>& BiomeMaterials)
	{
		for (const FSRBiomeMaterialEntry& BiomeMaterialEntry : BiomeMaterials)
		{
			UMaterialInterface* BiomeMaterialPtr = BiomeMaterialEntry.Material.Get();
			Hash = HashCombine(Hash, FCrc::StrCrc32(*BiomeMaterialEntry.BiomeId.ToString()));
			Hash = HashBiomeDataAssetSettings(Hash, BiomeMaterialEntry.BiomeDataAsset.Get());
			Hash = HashCombine(Hash, ::GetTypeHash(BiomeMaterialPtr));
		}

		return Hash;
	}
}

uint32 ASRCelestialBody::ComputeDynamicMeshBuildHash() const
{
	uint32 Hash = ::GetTypeHash(StaticMesh.Get());
	Hash = HashDynamicMeshBaseDataAssetSettings(Hash, DynamicMeshBaseDataAsset.Get());
	Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(BodyCategory)));
	Hash = HashCombine(Hash, ::GetTypeHash(Scale));
	Hash = HashCombine(Hash, ::GetTypeHash(GenerationSeed));
	Hash = HashDynamicMeshGenerationSettings(Hash, DynamicMeshGeneration);
	Hash = HashToonOutlineSettings(Hash, ToonOutlineSettings);
	return HashBiomeMaterialEntries(Hash, DynamicMeshGeneration.BiomeMaterials);
}
