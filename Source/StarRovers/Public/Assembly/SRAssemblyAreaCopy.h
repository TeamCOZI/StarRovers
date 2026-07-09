#pragma once

#include "CoreMinimal.h"
#include "Assembly/SRAssemblyPlacementHistory.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class AActor;
class ASRConveyorBeltActor;
class USRConveyorNetworkComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
class USRStructureInstanceManagerComponent;
class UMaterialInterface;
class UWorld;

namespace StarRovers::Assembly
{
	enum class ESRAssemblyAreaCopyPlacementPreviewState : uint8
	{
		Placeable,
		Replaceable,
		Blocked,
	};

	struct FSRAssemblyAreaCopiedStructure
	{
		TWeakObjectPtr<USRStructureDataAsset> StructureDataAsset;
		FIntPoint AnchorOffset = FIntPoint::ZeroValue;
		int32 PlacementRotationSteps = 0;
		TWeakObjectPtr<AActor> PreviewActor;
	};

	struct FSRAssemblyAreaCopiedConveyorPath
	{
		TWeakObjectPtr<USRStructureDataAsset> StructureDataAsset;
		TArray<FIntPoint> AnchorOffsets;
		int32 Layer = 0;
		float LayerHeight = 0.0f;
		FName NetworkId = NAME_None;
		TWeakObjectPtr<ASRConveyorBeltActor> PreviewActor;
	};

	struct FSRAssemblyAreaCopyPlacementEvaluation
	{
		ESRAssemblyAreaCopyPlacementPreviewState PreviewState = ESRAssemblyAreaCopyPlacementPreviewState::Blocked;
		TArray<FSRPlanetSurfaceGridCellId> TargetOriginCellIds;
		TArray<FSRConveyorBeltPath> TargetConveyorBeltPaths;
		TSet<FName> ReplaceableOccupantIds;
		TSet<FSRPlanetSurfaceGridCellId> ReplaceableOccupiedCellIds;
		bool bCanPlace = false;
	};

	struct FSRAssemblyAreaCopyCommitResult
	{
		TWeakObjectPtr<AActor> SurfaceOwner;
		TArray<FSRAssemblyPlacementHistoryEntry> HistoryEntries;
		bool bPlacedAny = false;
	};

	class STARROVERS_API FSRAssemblyAreaCopy
	{
	public:
		bool IsPlacementActive() const;
		bool HasPayload() const;
		bool HasCachedPreviewForHover(const FSRPlanetSurfaceGridCellId& HoverCellId) const;

		void BeginPlacement(
			TArray<FSRAssemblyAreaCopiedStructure>&& NewCopiedStructures,
			TArray<FSRAssemblyAreaCopiedConveyorPath>&& NewCopiedConveyorPaths);
		static bool BuildPlacementPayloadFromSelection(
			const TArray<FSRPlanetSurfaceGridCellId>& SelectionCellIds,
			const FSRPlanetSurfaceGridCellId& SelectionCenterCellId,
			USRStructureInstanceManagerComponent* StructureInstanceManager,
			USRConveyorNetworkComponent* ConveyorNetwork,
			TArray<FSRAssemblyAreaCopiedStructure>& OutCopiedStructures,
			TArray<FSRAssemblyAreaCopiedConveyorPath>& OutCopiedConveyorPaths);
		void Cancel();
		void ResetPreviewCache();
		void ClearPreviewHoverCache();
		void StorePreviewEvaluation(
			const FSRPlanetSurfaceGridCellId& HoverCellId,
			const FSRAssemblyAreaCopyPlacementEvaluation& Evaluation);
		void SetPreviewState(ESRAssemblyAreaCopyPlacementPreviewState PreviewState);
		void DestroyPreviewActors(USRPlanetSurfaceGrid* HoveredSurfaceGrid);
		void RebuildPreviewActors(
			UWorld* World,
			AActor* SurfaceOwner,
			AActor* FallbackOwner,
			USRPlanetSurfaceGrid* HoveredSurfaceGrid);
		void ApplyPreviewState(ESRAssemblyAreaCopyPlacementPreviewState PreviewState);
		bool UpdatePlacementPreview(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& HoverCellId);
		void UpdateStructurePreviewActors(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRPlanetSurfaceGridCellId>& TargetOriginCellIds,
			ESRAssemblyAreaCopyPlacementPreviewState PreviewState);
		static UMaterialInterface* ResolvePreviewMaterial(
			USRStructureDataAsset* StructureDataAsset,
			ESRAssemblyAreaCopyPlacementPreviewState PreviewState);
		static void ApplyPreviewMaterial(AActor* PreviewActor, UMaterialInterface* Material);
		bool BuildPlacementEvaluation(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& HoverCellId,
			FSRAssemblyAreaCopyPlacementEvaluation& OutEvaluation) const;
		bool TryCommitPlacement(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& HoverCellId,
			const FSRAssemblyPlacementHistory& PlacementHistory,
			FSRAssemblyAreaCopyCommitResult& OutResult);
		bool MirrorPlacement(bool bMirrorLeftRight);
		bool RotatePlacement(int32 StepDelta);

		TArray<FSRAssemblyAreaCopiedStructure> CopiedStructures;
		TArray<FSRAssemblyAreaCopiedConveyorPath> CopiedConveyorPaths;
		TSet<FName> LastReplaceableOccupantIds;
		FSRPlanetSurfaceGridCellId LastPreviewHoverCellId;
		ESRAssemblyAreaCopyPlacementPreviewState LastPreviewState = ESRAssemblyAreaCopyPlacementPreviewState::Blocked;
		bool bIsPlacementActive = false;
		bool bHasLastPreviewHoverCell = false;
	};
}
