#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class AActor;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

namespace StarRovers::Assembly
{
	class STARROVERS_API FSRAssemblySingleCellDeletion
	{
	public:
		static void BuildCandidateConveyorLayers(const USRStructureDataAsset* SelectedStructureDataAsset, TArray<int32>& OutLayers);
		static bool TryDeleteStructureAtCell(
			AActor* FocusedActor,
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& TargetCellId,
			const TArray<int32>& CandidateConveyorLayers);
		static bool TryDeleteConnectedConveyorsAtCell(
			AActor* FocusedActor,
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& TargetCellId,
			const TArray<int32>& CandidateConveyorLayers);

	private:
		static bool TryDestroyAttachedOccupantActor(AActor* SurfaceOwner, FName OccupantId);
	};
}
