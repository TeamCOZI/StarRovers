#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/MaterialInterface.h"
#include "Structure/SRStructureDataAsset.h"

void USRConveyorNetworkComponent::RefreshConveyorVisuals(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bBuildDynamicMeshVisuals)
	{
		if (IsValid(BeltMeshComponent))
		{
			UE::Geometry::FDynamicMesh3 EmptyMesh;
			EmptyMesh.EnableAttributes();
			EmptyMesh.Attributes()->EnablePrimaryColors();
			EmptyMesh.Attributes()->SetNumUVLayers(1);
			BeltMeshComponent->SetMesh(MoveTemp(EmptyMesh));
			BeltMeshComponent->SetVisibility(false);
			BeltMeshComponent->SetHiddenInGame(true);
		}
		return;
	}

	EnsureBeltMeshComponent();
	if (!IsValid(BeltMeshComponent))
	{
		return;
	}

	UE::Geometry::FDynamicMesh3 BeltMesh;
	BeltMesh.EnableAttributes();
	BeltMesh.Attributes()->EnablePrimaryColors();
	BeltMesh.Attributes()->SetNumUVLayers(1);

	UMaterialInterface* BeltMaterial = nullptr;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!IsValid(VisualPath.StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = VisualPath.StructureDataAsset->BuildData();
		if (!IsValid(BeltMaterial) && IsValid(StructureData.Material.Get()))
		{
			BeltMaterial = StructureData.Material.Get();
		}
		BuildConveyorPathRibbon(BeltMesh, SurfaceGrid, VisualPath);
	}

	if (IsValid(BeltMaterial))
	{
		BeltMeshComponent->SetMaterial(0, BeltMaterial);
	}
	BeltMeshComponent->SetMesh(MoveTemp(BeltMesh));
	BeltMeshComponent->SetVisibility(true);
	BeltMeshComponent->SetHiddenInGame(false);
}
