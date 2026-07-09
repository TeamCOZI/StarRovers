#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
struct FSRAssemblyStructurePreviewState;

namespace StarRovers::Assembly
{
	enum class ESRAssemblyStructureGhostPreviewUpdateResult : uint8
	{
		DestroyPreview,
		NoChange,
		Updated,
	};

	class STARROVERS_API FSRAssemblyStructureGhostPreviewUpdater
	{
	public:
		static ESRAssemblyStructureGhostPreviewUpdateResult Update(
			UWorld* World,
			AActor* PreviewOwner,
			bool bAssemblyModeActive,
			USRPlanetSurfaceGrid* HoveredSurfaceGrid,
			USRStructureDataAsset* StructureDataAsset,
			int32 RotationSteps,
			float AdditionalYawDegrees,
			FSRAssemblyStructurePreviewState& StructurePreview);
	};
}
