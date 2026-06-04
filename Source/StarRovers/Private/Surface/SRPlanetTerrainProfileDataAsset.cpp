#include "Surface/SRPlanetTerrainProfileDataAsset.h"

USRPlanetTerrainProfileDataAsset::USRPlanetTerrainProfileDataAsset()
{
	NormalizeProfile();
}

void USRPlanetTerrainProfileDataAsset::PostLoad()
{
	Super::PostLoad();
	NormalizeProfile();
}

#if WITH_EDITOR
void USRPlanetTerrainProfileDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NormalizeProfile();
}
#endif

void USRPlanetTerrainProfileDataAsset::ApplyToDynamicMeshGeneration(FSRDynamicMeshGeneration& InOutDynamicMeshGeneration) const
{
	InOutDynamicMeshGeneration.NormalizeBiomeMaterials(GetAllowedBiomeDataAssets());
}

TArray<TObjectPtr<USRPlanetBiomeDataAsset>> USRPlanetTerrainProfileDataAsset::GetAllowedBiomeDataAssets() const
{
	TArray<TObjectPtr<USRPlanetBiomeDataAsset>> AllowedBiomeDataAssets;
	for (const FSRPlanetProfileBiomeEntry& BiomeEntry : Biomes)
	{
		if (IsValid(BiomeEntry.BiomeDataAsset.Get()) && !AllowedBiomeDataAssets.Contains(BiomeEntry.BiomeDataAsset))
		{
			AllowedBiomeDataAssets.Add(BiomeEntry.BiomeDataAsset);
		}
	}

	return AllowedBiomeDataAssets;
}

void USRPlanetTerrainProfileDataAsset::NormalizeProfile()
{
}
