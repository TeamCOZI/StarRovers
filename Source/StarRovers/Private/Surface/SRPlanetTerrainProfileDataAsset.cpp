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
	AllowedBiomeDataAssets.Reserve(Biomes.Num());
	TSet<USRPlanetBiomeDataAsset*> SeenBiomeDataAssets;
	SeenBiomeDataAssets.Reserve(Biomes.Num());
	for (const FSRPlanetProfileBiomeEntry& BiomeEntry : Biomes)
	{
		USRPlanetBiomeDataAsset* BiomeDataAsset = BiomeEntry.BiomeDataAsset.Get();
		if (!IsValid(BiomeDataAsset))
		{
			continue;
		}

		bool bAlreadySeen = false;
		SeenBiomeDataAssets.Add(BiomeDataAsset, &bAlreadySeen);
		if (!bAlreadySeen)
		{
			AllowedBiomeDataAssets.Add(BiomeEntry.BiomeDataAsset);
		}
	}

	return AllowedBiomeDataAssets;
}

void USRPlanetTerrainProfileDataAsset::NormalizeProfile()
{
}
