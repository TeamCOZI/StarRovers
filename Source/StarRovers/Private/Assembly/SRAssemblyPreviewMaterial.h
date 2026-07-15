#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Structure/SRStructureDataAsset.h"

namespace StarRovers::Assembly::PreviewMaterials
{
	inline UMaterialInterface* ResolveGhostMaterial(const FSRStructureData& StructureData)
	{
		return IsValid(StructureData.GhostMaterial.Get())
			? StructureData.GhostMaterial.Get()
			: StructureData.Material.Get();
	}

	inline UMaterialInterface* ResolveReplaceableMaterial(const FSRStructureData& StructureData)
	{
		return IsValid(StructureData.ReplaceableMaterial.Get())
			? StructureData.ReplaceableMaterial.Get()
			: ResolveGhostMaterial(StructureData);
	}

	inline void ApplyToActor(AActor* PreviewActor, UMaterialInterface* Material)
	{
		if (!IsValid(PreviewActor) || !IsValid(Material))
		{
			return;
		}

		TArray<UMeshComponent*> MeshComponents;
		PreviewActor->GetComponents<UMeshComponent>(MeshComponents);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (!IsValid(MeshComponent))
			{
				continue;
			}

			const int32 MaterialSlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
			{
				MeshComponent->SetMaterial(MaterialIndex, Material);
			}
		}
	}
}
