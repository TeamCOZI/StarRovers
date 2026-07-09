#include "Assembly/SRAssemblyPlacementRestoration.h"

#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	void AppendRestorableStructures(
		const TArray<FSRPlacedStructureInstance>& RemovedStructures,
		TArray<FSRRestorableNaturalStructure>& OutRestorableStructures)
	{
		OutRestorableStructures.Reserve(OutRestorableStructures.Num() + RemovedStructures.Num());
		for (const FSRPlacedStructureInstance& RemovedStructure : RemovedStructures)
		{
			if (!IsValid(RemovedStructure.StructureDataAsset.Get()))
			{
				continue;
			}

			FSRRestorableNaturalStructure RestorableStructure;
			RestorableStructure.StructureDataAsset = RemovedStructure.StructureDataAsset.Get();
			RestorableStructure.OriginCellId = RemovedStructure.OriginCellId;
			RestorableStructure.PlacementRotationSteps = RemovedStructure.PlacementRotationSteps;
			RestorableStructure.bNaturalStructure = RemovedStructure.bNaturalStructure;
			RestorableStructure.bUseStaticMeshMaterials = RemovedStructure.bUseStaticMeshMaterials;
			OutRestorableStructures.Add(RestorableStructure);
		}
	}

	void RestoreRemovedStructures(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		const TArray<FSRPlacedStructureInstance>& RemovedStructures)
	{
		if (!IsValid(SurfaceGrid) || !IsValid(StructureInstanceManager))
		{
			return;
		}

		for (const FSRPlacedStructureInstance& RemovedStructure : RemovedStructures)
		{
			USRStructureDataAsset* StructureDataAsset = RemovedStructure.StructureDataAsset.Get();
			if (!IsValid(StructureDataAsset))
			{
				continue;
			}

			FName RestoredOccupantId = NAME_None;
			StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(
				SurfaceGrid,
				RemovedStructure.OriginCellId,
				StructureDataAsset,
				RestoredOccupantId,
				RemovedStructure.bNaturalStructure,
				RemovedStructure.bUseStaticMeshMaterials,
				RemovedStructure.PlacementRotationSteps);
		}
	}
}
