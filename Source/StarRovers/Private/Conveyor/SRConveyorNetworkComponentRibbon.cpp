#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "Conveyor/SRConveyorComponentPool.h"
#include "Conveyor/SRConveyorRibbonBuilder.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/MaterialInterface.h"
#include "Structure/SRStructureDataAsset.h"

void USRConveyorNetworkComponent::RefreshConveyorRibbonMesh(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bBuildBeltRibbonMesh)
	{
		StarRovers::Conveyor::FSRConveyorComponentPool::ClearBeltMeshComponent(BeltMeshComponent, true);
		return;
	}

	StarRovers::Conveyor::FSRConveyorComponentPool::EnsureBeltMeshComponent(GetOwner(), this, BeltMeshComponent);
	if (!IsValid(BeltMeshComponent))
	{
		return;
	}

	UE::Geometry::FDynamicMesh3 BeltMesh;
	BeltMesh.EnableAttributes();
	BeltMesh.Attributes()->EnablePrimaryColors();
	BeltMesh.Attributes()->SetNumUVLayers(1);

	StarRovers::Conveyor::FSRConveyorRibbonBuildSettings RibbonSettings;
	RibbonSettings.BeltWidth = BeltWidth;
	RibbonSettings.BeltThickness = BeltThickness;
	RibbonSettings.BeltSurfaceOffset = BeltSurfaceOffset;
	RibbonSettings.ComponentTransform = GetComponentTransform();

	UMaterialInterface* BeltMaterial = nullptr;
	for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
	{
		if (!IsValid(BeltPath.StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = BeltPath.StructureDataAsset->BuildData();
		if (!IsValid(BeltMaterial) && IsValid(StructureData.Material.Get()))
		{
			BeltMaterial = StructureData.Material.Get();
		}
		StarRovers::Conveyor::FSRConveyorRibbonBuilder::BuildPathRibbon(BeltMesh, SurfaceGrid, BeltPath, RibbonSettings);
	}

	if (IsValid(BeltMaterial))
	{
		BeltMeshComponent->SetMaterial(0, BeltMaterial);
	}
	BeltMeshComponent->SetMesh(MoveTemp(BeltMesh));
	BeltMeshComponent->SetVisibility(true);
	BeltMeshComponent->SetHiddenInGame(false);
}
