#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRStructurePlacementLibrary.generated.h"

class AActor;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

UCLASS()
class STARROVERS_API USRStructurePlacementLibrary : public UObject
{
	GENERATED_BODY()

public:
	static bool BuildStructurePlacementTransform(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		USRStructureDataAsset* StructureDataAsset,
		FTransform& OutTransform,
		float AdditionalYawDegrees = 0.0f);

	static bool TryPlaceStructureOnSurfaceGrid(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		USRStructureDataAsset* StructureDataAsset,
		AActor*& OutPlacedStructureActor,
		bool bUseStaticMeshMaterials = false,
		int32 PlacementRotationSteps = 0);
};
