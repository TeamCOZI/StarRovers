#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class AActor;
class UWorld;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
struct FSRAssemblyPlacementDragState;
struct FSRAssemblyStructurePreviewState;
struct FSRStructureData;

namespace StarRovers::Assembly
{
	class STARROVERS_API FSRAssemblyStructureDragPreviewUpdater
	{
	public:
		static bool Update(
			UWorld* World,
			AActor* PreviewOwner,
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRStructureDataAsset* StructureDataAsset,
			const FSRStructureData& StructureData,
			const TArray<FSRPlanetSurfaceGridCellId>& CandidateCellIds,
			int32 PlacementRotationSteps,
			float AdditionalYawDegrees,
			FSRAssemblyPlacementDragState& PlacementDrag,
			FSRAssemblyStructurePreviewState& StructurePreview);
	};
}
