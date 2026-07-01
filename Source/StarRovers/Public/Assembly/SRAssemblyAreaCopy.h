#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class AActor;
class ASRConveyorBeltActor;
class USRStructureDataAsset;

namespace StarRovers::Assembly
{
	enum class ESRAreaCopyPlacementPreviewState : uint8
	{
		Placeable,
		Replaceable,
		Blocked,
	};

	struct FSRAreaCopiedStructure
	{
		TWeakObjectPtr<USRStructureDataAsset> StructureDataAsset;
		FIntPoint AnchorOffset = FIntPoint::ZeroValue;
		int32 PlacementRotationSteps = 0;
		TWeakObjectPtr<AActor> PreviewActor;
	};

	struct FSRAreaCopiedConveyorPath
	{
		TWeakObjectPtr<USRStructureDataAsset> StructureDataAsset;
		TArray<FIntPoint> AnchorOffsets;
		int32 Layer = 0;
		float LayerHeight = 0.0f;
		FName NetworkId = NAME_None;
		TWeakObjectPtr<ASRConveyorBeltActor> PreviewActor;
	};

	struct FSRAreaCopyPlacementEvaluation
	{
		ESRAreaCopyPlacementPreviewState PreviewState = ESRAreaCopyPlacementPreviewState::Blocked;
		TArray<FSRPlanetSurfaceGridCellId> TargetOriginCellIds;
		TArray<FSRConveyorVisualPath> TargetConveyorVisualPaths;
		TSet<FName> ReplaceableOccupantIds;
		TSet<FSRPlanetSurfaceGridCellId> ReplaceableOccupiedCellIds;
		bool bCanPlace = false;
	};

	class STARROVERS_API FSRAssemblyAreaCopy
	{
	public:
		bool IsPlacementActive() const;
		bool HasPayload() const;
		bool HasCachedPreviewForHover(const FSRPlanetSurfaceGridCellId& HoverCellId) const;

		void BeginPlacement(
			TArray<FSRAreaCopiedStructure>&& NewCopiedStructures,
			TArray<FSRAreaCopiedConveyorPath>&& NewCopiedConveyorPaths);
		void Cancel();
		void ResetPreviewCache();
		void ClearPreviewHoverCache();
		void StorePreviewEvaluation(
			const FSRPlanetSurfaceGridCellId& HoverCellId,
			const FSRAreaCopyPlacementEvaluation& Evaluation);
		void SetPreviewState(ESRAreaCopyPlacementPreviewState PreviewState);

		TArray<FSRAreaCopiedStructure> CopiedStructures;
		TArray<FSRAreaCopiedConveyorPath> CopiedConveyorPaths;
		TSet<FName> LastReplaceableOccupantIds;
		FSRPlanetSurfaceGridCellId LastPreviewHoverCellId;
		ESRAreaCopyPlacementPreviewState LastPreviewState = ESRAreaCopyPlacementPreviewState::Blocked;
		bool bIsPlacementActive = false;
		bool bHasLastPreviewHoverCell = false;
	};
}
