#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Conveyor/SRConveyorTypes.h"
#include "UObject/UnrealType.h"
#include "SRConveyorNetworkComponent.generated.h"

class ASRConveyorBeltActor;
class UDynamicMeshComponent;
class ULineBatchComponent;
class UMaterialInterface;
class UPCGComponent;
class USplineComponent;
class USRFacilityNetworkComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
namespace UE::Geometry
{
	class FDynamicMesh3;
}

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRConveyorNetworkComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USRConveyorNetworkComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor")
	bool HasConveyorSegment(const FSRConveyorLaneKey& LaneKey) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor")
	bool GetConveyorSegment(const FSRConveyorLaneKey& LaneKey, FSRConveyorSegment& OutSegment) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	void ClearConveyors();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor|Debug")
	void SetPathDebugLineVisible(bool bNewPathDebugLineVisible);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor|Debug")
	bool IsPathDebugLineVisible() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor|Transport")
	int32 GetConveyorItemCount() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool FindConveyorPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		int32 Layer,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool TryPlaceConveyorPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
		int32 Layer,
		float LayerHeight,
		USRStructureDataAsset* StructureDataAsset,
		FName NetworkId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool TryRemoveConveyorAtCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 Layer);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "DefaultLayerHeight", ClampMin = "0.0"))
	float DefaultLayerHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "BeltWidth", ClampMin = "1.0"))
	float BeltWidth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "BeltThickness", ClampMin = "1.0"))
	float BeltThickness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor", meta = (DisplayName = "BeltSurfaceOffset", ClampMin = "0.0"))
	float BeltSurfaceOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Visual", meta = (DisplayName = "bBuildDynamicMeshVisuals"))
	bool bBuildDynamicMeshVisuals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Visual", meta = (DisplayName = "bSpawnConveyorBeltActors"))
	bool bSpawnConveyorBeltActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Visual", meta = (DisplayName = "MaxConveyorActorGroupsRefreshedPerFrame", ClampMin = "1"))
	int32 MaxConveyorActorGroupsRefreshedPerFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "bBuildPCGSplineInputs"))
	bool bBuildPCGSplineInputs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "PCGSplineComponentTag"))
	FName PCGSplineComponentTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "PCGSplineHeightOffset"))
	float PCGSplineHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|PCG", meta = (DisplayName = "bAutoGeneratePCG"))
	bool bAutoGeneratePCG;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "bShowPathDebugLine"))
	bool bShowPathDebugLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "PathDebugLineColor"))
	FLinearColor PathDebugLineColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "PathDebugLineThickness", ClampMin = "0.0"))
	float PathDebugLineThickness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport", meta = (DisplayName = "bAutoTransportItems"))
	bool bAutoTransportItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport", meta = (DisplayName = "ItemSpeedCellsPerSecond", ClampMin = "0.01"))
	float ItemSpeedCellsPerSecond;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport", meta = (DisplayName = "MaxItemTransfersPerTick", ClampMin = "1"))
	int32 MaxItemTransfersPerTick;

	TMap<FSRConveyorLaneKey, FSRConveyorSegment> Segments;
	TArray<FSRConveyorVisualPath> VisualPaths;

	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshComponent> BeltMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> PathDebugLineBatchComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineComponent>> PCGSplineComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASRConveyorBeltActor>> PlacedConveyorActors;

	UPROPERTY(Transient)
	TWeakObjectPtr<USRPlanetSurfaceGrid> PendingConveyorActorRefreshSurfaceGrid;

private:
	struct FSRConveyorActorGroupState
	{
		TArray<FSRConveyorVisualPath> VisualPaths;
		ASRConveyorBeltActor* Actor = nullptr;
		bool bDirty = false;
	};

	static FSRConveyorLaneKey MakeLaneKey(const FSRPlanetSurfaceGridCellId& CellId, int32 Layer);
	static FName MakeActorGroupKey(USRStructureDataAsset* StructureDataAsset, int32 Layer);
	static ESRConveyorGridDirection GetOppositeDirection(ESRConveyorGridDirection Direction);
	static ESRConveyorSegmentShape ResolveSegmentShape(ESRConveyorGridDirection InputDirection, ESRConveyorGridDirection OutputDirection);
	static bool GetNeighborCellIdByDirection(const FSRPlanetSurfaceGridCellNeighbors& Neighbors, ESRConveyorGridDirection Direction, FSRPlanetSurfaceGridCellId& OutCellId);
	static bool FindDirectionBetweenCells(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& FromCellId, const FSRPlanetSurfaceGridCellId& ToCellId, ESRConveyorGridDirection& OutDirection);

	bool CanPlaceConveyorSegment(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorLaneKey& LaneKey) const;
	void EnsureBeltMeshComponent();
	void EnsurePathDebugLineBatchComponent();
	USplineComponent* EnsurePCGSplineComponent(int32 SplineIndex);
	void ClearUnusedPCGSplineComponents(int32 FirstUnusedSplineIndex);
	bool BuildConveyorPathSplinePoints(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorVisualPath& VisualPath,
		TArray<FVector>& OutWorldPoints,
		TArray<FVector>& OutWorldNormals) const;
	float ResolveBeltHalfWidth(const TArray<FVector>& WorldPoints) const;
	float ResolveBeltHalfThickness(float HalfWidth, float LayerHeight) const;
	float ResolveConveyorLayerHeight(USRPlanetSurfaceGrid* SurfaceGrid, float RequestedLayerHeight) const;
	bool BuildConveyorSegmentRibbon(
		UE::Geometry::FDynamicMesh3& BeltMesh,
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorSegment& Segment,
		float LayerHeight) const;
	bool BuildConveyorPathRibbon(
		UE::Geometry::FDynamicMesh3& BeltMesh,
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorVisualPath& VisualPath) const;
	ASRConveyorBeltActor* SpawnConveyorActorForVisualPaths(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRConveyorVisualPath>& GroupedVisualPaths);
	void DestroyPlacedConveyorActors();
	void MarkConveyorActorGroupDirty(USRStructureDataAsset* StructureDataAsset, int32 Layer);
	void MarkConveyorActorGroupPlacementDiagnosticPending(USRStructureDataAsset* StructureDataAsset, int32 Layer);
	void MarkConveyorActorGroupDeletionDiagnosticPending(USRStructureDataAsset* StructureDataAsset, int32 Layer);
	void ScheduleDirtyConveyorActorGroupRefresh(USRPlanetSurfaceGrid* SurfaceGrid);
	bool RefreshConveyorActorGroup(USRPlanetSurfaceGrid* SurfaceGrid, FName ActorGroupKey);
	bool RefreshDirtyConveyorActorGroups(USRPlanetSurfaceGrid* SurfaceGrid, int32 MaxGroupCount = INDEX_NONE);
	bool HasDirtyConveyorActorGroups() const;
	void LogConveyorMutationMemoryDiagnostics(const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection) const;
	void RebuildSegmentsFromVisualPaths(USRPlanetSurfaceGrid* SurfaceGrid);
	bool RebuildPlacedConveyorActors(USRPlanetSurfaceGrid* SurfaceGrid);
	void RefreshConveyorVisuals(USRPlanetSurfaceGrid* SurfaceGrid);
	void RefreshPCGSplineInputs(USRPlanetSurfaceGrid* SurfaceGrid);
	void RefreshPathDebugLines(USRPlanetSurfaceGrid* SurfaceGrid);
	void ProcessConveyorTransport(USRPlanetSurfaceGrid* SurfaceGrid, float DeltaTime);
	bool TryResolveNextLane(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorSegment& Segment, FSRConveyorLaneKey& OutNextLane) const;
	bool TryPullFacilityOutputToConveyor(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRFacilityNetworkComponent* FacilityNetwork,
		const FSRConveyorLaneKey& LaneKey,
		TMap<FSRConveyorLaneKey, FSRConveyorItem>& OutNextItems) const;
	bool ShouldKeepTransportTickEnabled() const;
	void RequestPCGGeneration();
	void BindPCGGenerationDelegates();
	void HandlePCGGraphGenerated(UPCGComponent* PCGComponent);
	void RebaseGeneratedPCGSplineMeshes(UPCGComponent* PCGComponent);

	TMap<FName, FSRConveyorActorGroupState> ConveyorActorGroupsByKey;
	TSet<FName> PendingPlacementDiagnosticActorGroupKeys;
	TSet<FName> PendingDeletionDiagnosticActorGroupKeys;

	UPROPERTY(Transient)
	TMap<FSRConveyorLaneKey, FSRConveyorItem> ConveyorItemsByLane;
};
