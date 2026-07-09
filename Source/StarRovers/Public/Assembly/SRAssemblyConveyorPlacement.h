#pragma once

#include "CoreMinimal.h"
#include "Assembly/SRAssemblyPlacementHistory.h"

class AActor;
class USRConveyorNetworkComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
struct FSRStructureData;

namespace StarRovers::Assembly
{
	struct FSRAssemblyConveyorPlacementResult
	{
		FSRConveyorBeltPath HistoryBeltPath;
		TArray<FSRPlanetSurfaceGridCellId> HistoryPlacedCellIds;
		TArray<FSRRestorableNaturalStructure> HistoryRemovedNaturalStructures;
	};

	class STARROVERS_API FSRAssemblyConveyorPlacement
	{
	public:
		static FName MakeNetworkId(AActor* FocusedActor, int32 Layer);

		static bool TryPlacePath(
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRConveyorNetworkComponent* ConveyorNetwork,
			USRStructureDataAsset* ConveyorDataAsset,
			const FSRStructureData& ConveyorData,
			const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
			FName NetworkId,
			const FSRAssemblyPlacementHistory& PlacementHistory,
			FSRAssemblyConveyorPlacementResult& OutResult);
	};
}
