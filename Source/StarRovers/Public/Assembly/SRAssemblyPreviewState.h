#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyPreviewState.generated.h"

class AActor;
class ASRConveyorBeltActor;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
class UWorld;
struct FSRConveyorBeltPath;
struct FSRStructureData;

enum class ESRAssemblyConveyorGhostUpdateResult : uint8
{
	Failed,
	Updated,
	PreviewFailed,
};

USTRUCT()
struct STARROVERS_API FSRStructurePlacementDragPreviewActor
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId CellId;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PreviewActor;
};

USTRUCT()
struct STARROVERS_API FSRAssemblyStructurePreviewState
{
	GENERATED_BODY()

	void ClearGhostPortPreview();
	void UpdateGhostPortPreview(USRPlanetSurfaceGrid* SurfaceGrid, const FSRStructureData& StructureData, const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds, int32 PlacementRotationSteps);
	bool UpdateGhostActor(UWorld* World, AActor* Owner, USRPlanetSurfaceGrid* HoveredSurfaceGrid, USRStructureDataAsset* StructureDataAsset, const FSRStructureData& StructureData, const FTransform& GhostTransform, const FSRPlanetSurfaceGridCellInfo& PreviewCellInfo);
	void DestroyGhostActor(USRPlanetSurfaceGrid* HoveredSurfaceGrid);
	void DestroyPlacementDragPreviewActors(USRPlanetSurfaceGrid* HoveredSurfaceGrid);
	AActor* SpawnPlacementDragPreviewActor(UWorld* World, AActor* FallbackOwner, USRPlanetSurfaceGrid* SurfaceGrid, USRStructureDataAsset* StructureDataAsset) const;
	void LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason);

	UPROPERTY(Transient)
	TObjectPtr<AActor> StructureGhostActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> StructureGhostDataAsset = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> StructureGhostPortPreviewSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> LastLoggedInvalidGhostDataAsset = nullptr;

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId StructureGhostCellId;

	UPROPERTY(Transient)
	bool bHasStructureGhostCellId = false;

	UPROPERTY(Transient)
	bool bHasStructureGhostPortPreview = false;

	UPROPERTY(Transient)
	TArray<FSRStructurePlacementDragPreviewActor> StructurePlacementDragPreviewActors;
};

USTRUCT()
struct STARROVERS_API FSRAssemblyConveyorPreviewState
{
	GENERATED_BODY()

	void ClearPortPreview();
	void SetInvalidPlacementPreview(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
	void ClearInvalidPlacementPreview();
	void SetBulkDeletionPreview(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
	void ClearBulkDeletionPreview();
	bool IsGhostActorCurrent(USRPlanetSurfaceGrid* SurfaceGrid, USRStructureDataAsset* ConveyorDataAsset, const FSRPlanetSurfaceGridCellId& TargetCellId) const;
	ESRAssemblyConveyorGhostUpdateResult UpdateGhostActor(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRPlanetSurfaceGrid* CleanupSurfaceGrid,
		USRStructureDataAsset* ConveyorDataAsset,
		const FSRStructureData& ConveyorData,
		const TArray<FSRConveyorBeltPath>& BeltPaths,
		FName ConveyorActorSplineComponentTag,
		float ConveyorActorSurfaceOffset,
		const FSRPlanetSurfaceGridCellId& TargetCellId);
	void DestroyGhostActor(USRPlanetSurfaceGrid* HoveredSurfaceGrid);
	bool IsDeletionGhostActorCurrent(USRPlanetSurfaceGrid* SurfaceGrid, USRStructureDataAsset* ConveyorDataAsset, const FSRPlanetSurfaceGridCellId& TargetCellId, int32 Layer) const;
	bool UpdateDeletionGhostActor(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureDataAsset* ConveyorDataAsset,
		const FSRStructureData& ConveyorData,
		const TArray<FSRConveyorBeltPath>& BeltPaths,
		FName ConveyorActorSplineComponentTag,
		float ConveyorActorSurfaceOffset,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		int32 Layer);
	void DestroyDeletionGhostActor();

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorPortPreviewSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ASRConveyorBeltActor> ConveyorGhostActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ASRConveyorBeltActor> ConveyorDeletionGhostActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> ConveyorGhostDataAsset = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> ConveyorDeletionGhostDataAsset = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorGhostSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorDeletionGhostSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorBulkDeletionPreviewSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorInvalidPlacementPreviewSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId ConveyorGhostTargetCellId;

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId ConveyorDeletionGhostTargetCellId;

	UPROPERTY(Transient)
	bool bHasConveyorPortPreview = false;

	UPROPERTY(Transient)
	bool bHasConveyorGhostTargetCell = false;

	UPROPERTY(Transient)
	bool bHasConveyorDeletionGhostTargetCell = false;

	UPROPERTY(Transient)
	bool bHasConveyorBulkDeletionPreview = false;

	UPROPERTY(Transient)
	bool bHasConveyorInvalidPlacementPreview = false;

	UPROPERTY(Transient)
	int32 ConveyorDeletionGhostLayer = 0;
};

struct STARROVERS_API FSRAssemblyPreviewResetOptions
{
	bool bClearConveyorPortPreview = true;
	bool bClearConveyorBulkDeletionPreview = true;
	bool bClearConveyorInvalidPlacementPreview = true;
	bool bDestroyStructureGhostActor = true;
	bool bDestroyStructurePlacementDragPreviewActors = false;
	bool bDestroyConveyorGhostActor = true;
	bool bDestroyConveyorDeletionGhostActor = false;
};

struct STARROVERS_API FSRAssemblyPreviewReset
{
	static void Apply(
		FSRAssemblyStructurePreviewState& StructurePreview,
		FSRAssemblyConveyorPreviewState& ConveyorPreview,
		USRPlanetSurfaceGrid* HoveredSurfaceGrid,
		const FSRAssemblyPreviewResetOptions& Options = FSRAssemblyPreviewResetOptions());
};
