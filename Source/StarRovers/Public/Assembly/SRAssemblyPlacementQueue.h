#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Assembly
{
	struct FSRQueuedStructurePlacement
	{
		TWeakObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;
		FSRPlanetSurfaceGridCellId CellId;
		int32 PlacementRotationSteps = 0;
	};

	class STARROVERS_API FSRAssemblyPlacementQueue
	{
	public:
		void ConfigurePerformance(int32 NewMaxStructurePlacementsPerFrame, int32 NewMaxQueuedStructurePlacements);
		void Reset();
		bool IsEmpty() const;
		void Enqueue(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId, int32 PlacementRotationSteps);
		void PopNextFrame(TArray<FSRQueuedStructurePlacement>& OutPlacements);

	private:
		int32 MaxStructurePlacementsPerFrame = 4;
		int32 MaxQueuedStructurePlacements = 256;
		TArray<FSRQueuedStructurePlacement> Queue;
	};
}
