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
class UTextRenderComponent;
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

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor")
	bool HasConveyorSegmentAtCell(const FSRPlanetSurfaceGridCellId& CellId) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	void ClearConveyors();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor|Debug")
	void SetPathDebugLineVisible(bool bNewPathDebugLineVisible);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor|Debug")
	bool IsPathDebugLineVisible() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor|Debug")
	void SetConnectionDebugLineVisible(bool bNewConnectionDebugLineVisible);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor|Debug")
	bool IsConnectionDebugLineVisible() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor|Transport")
	int32 GetConveyorItemCount() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor|PCG")
	FName GetConveyorActorSplineComponentTag() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor|PCG")
	float GetConveyorActorSurfaceOffset() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool FindConveyorPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		int32 Layer,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath) const;

	bool FindConveyorPathAvoidingCells(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		int32 Layer,
		const TSet<FSRPlanetSurfaceGridCellId>& AdditionalBlockedCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath) const;

	bool GetConveyorVisualPathsInCells(
		const TSet<FSRPlanetSurfaceGridCellId>& CellIds,
		TArray<FSRConveyorVisualPath>& OutVisualPaths) const;

	bool CanPlaceConveyorPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
		int32 Layer,
		const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds) const;

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

	bool TryRemoveConveyorVisualPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorVisualPath& VisualPath,
		const TArray<FSRPlanetSurfaceGridCellId>& PlacedCellIds);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool TryRemoveConveyorsAtCells(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor")
	bool GetConnectedConveyorCellIdsAtCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 Layer,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor")
	bool GetConnectedConveyorVisualPathsAtCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 Layer,
		TArray<FSRConveyorVisualPath>& OutVisualPaths) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Conveyor")
	bool TryRemoveConnectedConveyorsAtCell(
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "bShowConnectionDebugLine"))
	bool bShowConnectionDebugLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "ConnectionDebugLineColor"))
	FLinearColor ConnectionDebugLineColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "BrokenConnectionDebugLineColor"))
	FLinearColor BrokenConnectionDebugLineColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "EndpointDebugLineColor"))
	FLinearColor EndpointDebugLineColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "ConnectionDebugLineThickness", ClampMin = "0.0"))
	float ConnectionDebugLineThickness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Debug", meta = (DisplayName = "ConnectionDebugLineHeightOffset", ClampMin = "0.0"))
	float ConnectionDebugLineHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport", meta = (DisplayName = "bAutoTransportItems"))
	bool bAutoTransportItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport", meta = (DisplayName = "ItemSpeedCellsPerSecond", ClampMin = "0.01"))
	float ItemSpeedCellsPerSecond;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport", meta = (DisplayName = "MaxItemTransfersPerTick", ClampMin = "1"))
	int32 MaxItemTransfersPerTick;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Visual", meta = (DisplayName = "bShowTransportItemVisuals"))
	bool bShowTransportItemVisuals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Visual", meta = (DisplayName = "ItemVisualHeightOffset", ClampMin = "0.0"))
	float ItemVisualHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Visual", meta = (DisplayName = "ItemEnergyLabelWorldSize", ClampMin = "1.0"))
	float ItemEnergyLabelWorldSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Visual", meta = (DisplayName = "ItemEnergyLabelMaxScale", ClampMin = "1.0"))
	float ItemEnergyLabelMaxScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Visual", meta = (DisplayName = "ItemEnergyLowColor"))
	FLinearColor ItemEnergyLowColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Visual", meta = (DisplayName = "ItemEnergyHighColor"))
	FLinearColor ItemEnergyHighColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Visual", meta = (DisplayName = "ItemEnergyNegativeColor"))
	FLinearColor ItemEnergyNegativeColor;

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
	static int32 GetConveyorDirectionClockwiseOrder(ESRConveyorGridDirection Direction);
	static void SortConveyorDirectionsClockwise(TArray<ESRConveyorGridDirection>& Directions);
	static void CollectConveyorInputDirections(const FSRConveyorSegment& Segment, TArray<ESRConveyorGridDirection>& OutDirections);
	static void CollectConveyorOutputDirections(const FSRConveyorSegment& Segment, TArray<ESRConveyorGridDirection>& OutDirections);

	bool CanPlaceConveyorSegment(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorLaneKey& LaneKey) const;
	bool CanMergeConveyorSegment(const FSRConveyorSegment& Segment) const;
	void MergeConveyorSegment(const FSRConveyorSegment& Segment);
	void MergeConveyorInputDirection(FSRConveyorSegment& ExistingSegment, ESRConveyorGridDirection IncomingDirection);
	void MergeConveyorOutputDirection(FSRConveyorSegment& ExistingSegment, ESRConveyorGridDirection IncomingDirection);
	bool CanDestroyNaturalStructureForConveyorPlacement(USRPlanetSurfaceGrid* SurfaceGrid, FName OccupantId) const;
	bool DoesConveyorSegmentReferenceLane(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorSegment& Segment, const FSRConveyorLaneKey& TargetLaneKey) const;
	bool GatherConnectedConveyorLaneKeysAtCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 Layer,
		TArray<FSRConveyorLaneKey>& OutLaneKeys) const;
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
	void RefreshConveyorItemVisuals(USRPlanetSurfaceGrid* SurfaceGrid, float DeltaTime);
	void DestroyConveyorItemVisuals();
	UTextRenderComponent* EnsureConveyorItemLabelComponent(const FSRConveyorLaneKey& LaneKey);
	bool ResolveConveyorItemWorldLocation(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorItem& Item,
		FVector& OutWorldLocation,
		FVector& OutWorldNormal) const;
	FText BuildConveyorItemLabelText(const FSRResourceInstance& ResourceInstance) const;
	FColor ResolveConveyorItemLabelColor(const FSRResourceInstance& ResourceInstance) const;
	bool TryResolveNextLane(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorSegment& Segment, FSRConveyorLaneKey& OutNextLane) const;
	bool TryResolveNextLaneByDirection(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorSegment& Segment, ESRConveyorGridDirection Direction, FSRConveyorLaneKey& OutNextLane) const;
	bool TryResolveNextTransferLane(
		USRPlanetSurfaceGrid* SurfaceGrid,
		FSRConveyorSegment& Segment,
		const TMap<FSRConveyorLaneKey, FSRConveyorItem>& NextItemsByLane,
		FSRConveyorLaneKey& OutNextLane);
	bool CanTransferIntoMergeConveyorSegment(
		USRPlanetSurfaceGrid* SurfaceGrid,
		FSRConveyorSegment& MergeSegment,
		ESRConveyorGridDirection IncomingInputDirection,
		const TMap<FSRConveyorLaneKey, FSRConveyorItem>& NextItemsByLane) const;
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

	UPROPERTY(Transient)
	TMap<FSRConveyorLaneKey, TObjectPtr<UTextRenderComponent>> ConveyorItemLabelsByLane;
};
