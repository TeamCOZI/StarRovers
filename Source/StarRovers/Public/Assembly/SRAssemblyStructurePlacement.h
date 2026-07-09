#pragma once

#include "CoreMinimal.h"
#include "Assembly/SRAssemblyPlacementHistory.h"

class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
class USRStructureInstanceManagerComponent;
struct FSRStructureData;

namespace StarRovers::Assembly
{
	struct FSRAssemblyStructurePlacementResult
	{
		TWeakObjectPtr<USRStructureInstanceManagerComponent> StructureInstanceManager;
		FName OccupantId = NAME_None;
		TArray<FSRRestorableNaturalStructure> RemovedNaturalStructures;
		bool bPlacedWithStructureInstanceManager = false;
		bool bShouldDestroyPreviewOnFailure = false;
	};

	class STARROVERS_API FSRAssemblyStructurePlacement
	{
	public:
		static bool TryPlace(
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRStructureDataAsset* StructureDataAsset,
			const FSRStructureData& StructureData,
			const FSRPlanetSurfaceGridCellId& TargetCellId,
			int32 PlacementRotationSteps,
			FSRAssemblyStructurePlacementResult& OutResult);

		static FSRAssemblyPlacementHistoryEntry BuildHistoryEntry(
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRStructureInstanceManagerComponent* StructureInstanceManager,
			USRStructureDataAsset* StructureDataAsset,
			const FSRPlanetSurfaceGridCellId& OriginCellId,
			int32 PlacementRotationSteps,
			const FSRAssemblyStructurePlacementResult& PlacementResult);
	};
}
