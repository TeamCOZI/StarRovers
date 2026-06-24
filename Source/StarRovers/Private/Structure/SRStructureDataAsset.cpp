#include "Structure/SRStructureDataAsset.h"

USRStructureDataAsset::USRStructureDataAsset()
{
	StructureId = FName(TEXT("Structure"));
	DisplayName = NSLOCTEXT("StarRoversStructure", "DefaultStructureDisplayName", "Structure");
	Description = NSLOCTEXT("StarRoversStructure", "DefaultStructureDescription", "Surface structure.");
	MeshRelativeScale = FVector::OneVector;
	FootprintCellsX = 1;
	FootprintCellsY = 1;
	ConstructionHeightOffset = 0.0f;
	PlacementYawDegrees = 0.0f;
	bAlignToSurfaceNormal = true;
	bAvailableForConstruction = true;
	bDestroyableByConstruction = true;
	BuildKind = ESRStructureBuildKind::Structure;
	bIsResourceDeposit = false;
	DepositTotalAmount = 0;
	ConveyorLayer = 0;
	ConveyorLayerHeight = 160.0f;
}

FSRStructureData USRStructureDataAsset::BuildData() const
{
	FSRStructureData Result;
	Result.StructureId = StructureId;
	Result.DisplayName = DisplayName;
	Result.Description = Description;
	Result.StructureActorClass = StructureActorClass;
	Result.StaticMesh = StaticMesh;
	Result.Material = Material;
	Result.GhostMaterial = GhostMaterial;
	Result.MeshRelativeLocation = MeshRelativeLocation;
	Result.MeshRelativeRotation = MeshRelativeRotation;
	Result.MeshRelativeScale = MeshRelativeScale;
	Result.FootprintCellsX = FMath::Max(1, FootprintCellsX);
	Result.FootprintCellsY = FMath::Max(1, FootprintCellsY);
	Result.ConstructionHeightOffset = FMath::Max(0.0f, ConstructionHeightOffset);
	Result.PlacementYawDegrees = PlacementYawDegrees;
	Result.bAlignToSurfaceNormal = bAlignToSurfaceNormal;
	Result.bAvailableForConstruction = bAvailableForConstruction;
	Result.bDestroyableByConstruction = bDestroyableByConstruction;
	Result.BuildKind = BuildKind;
	Result.FacilityDataAsset = FacilityDataAsset;
	Result.bIsResourceDeposit = bIsResourceDeposit;
	Result.DepositResourceDataAsset = DepositResourceDataAsset;
	Result.DepositTotalAmount = FMath::Max(0, DepositTotalAmount);
	Result.InputPorts = InputPorts;
	Result.OutputPorts = OutputPorts;
	Result.ConveyorLayer = FMath::Max(0, ConveyorLayer);
	Result.ConveyorLayerHeight = FMath::Max(0.0f, ConveyorLayerHeight);
	return Result;
}
