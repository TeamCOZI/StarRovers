#include "Surface/SRPlanetTerrainTypes.h"

#include "Materials/MaterialInterface.h"
#include "Surface/SRPlanetBiomeDataAsset.h"

FSRDynamicMeshGeneration::FSRDynamicMeshGeneration()
{
}

void FSRDynamicMeshGeneration::NormalizeBiomeMaterials(const TArray<TObjectPtr<USRPlanetBiomeDataAsset>>& AllowedBiomeDataAssets)
{
	BiomeDataAssets.Reset();
	TMap<FName, UMaterialInterface*> ExistingMaterialsById;
	TMap<USRPlanetBiomeDataAsset*, UMaterialInterface*> ExistingMaterialsByAsset;
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
	for (USRPlanetBiomeDataAsset* BiomeDataAsset : AllowedBiomeDataAssets)
	{
		if (!IsValid(BiomeDataAsset))
		{
			continue;
		}

		if (!BiomeDataAssets.Contains(BiomeDataAsset))
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
