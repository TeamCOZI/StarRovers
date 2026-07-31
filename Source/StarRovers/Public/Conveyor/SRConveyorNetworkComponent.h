#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Conveyor/SRConveyorNetworkRuntimeState.h"
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

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Conveyor")
	void ExportSaveData(FSRConveyorNetworkSaveData& OutSaveData) const;

	bool CanImportSaveData(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorNetworkSaveData& SaveData,
		FString& OutFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Save|Conveyor")
	bool ImportSaveData(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorNetworkSaveData& SaveData);

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

	bool GetConveyorBeltPathsInCells(
		const TSet<FSRPlanetSurfaceGridCellId>& CellIds,
		TArray<FSRConveyorBeltPath>& OutBeltPaths) const;

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

	bool TryRemoveConveyorBeltPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRConveyorBeltPath& BeltPath,
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
	bool GetConnectedConveyorBeltPathsAtCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 Layer,
		TArray<FSRConveyorBeltPath>& OutBeltPaths) const;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Rendering", meta = (DisplayName = "bBuildBeltRibbonMesh"))
	bool bBuildBeltRibbonMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Rendering", meta = (DisplayName = "bSpawnConveyorBeltActors"))
	bool bSpawnConveyorBeltActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Rendering", meta = (DisplayName = "MaxConveyorActorGroupsRefreshedPerFrame", ClampMin = "1"))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Label", meta = (DisplayName = "bShowTransportItemLabels"))
	bool bShowTransportItemLabels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Label", meta = (DisplayName = "ItemLabelHeightOffset", ClampMin = "0.0"))
	float ItemLabelHeightOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Label", meta = (DisplayName = "ItemPatternLabelWorldSize", ClampMin = "1.0"))
	float ItemPatternLabelWorldSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Label", meta = (DisplayName = "ItemPatternLabelMaxScale", ClampMin = "1.0"))
	float ItemPatternLabelMaxScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Label", meta = (DisplayName = "ItemPatternSparseColor"))
	FLinearColor ItemPatternSparseColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Label", meta = (DisplayName = "ItemPatternDenseColor"))
	FLinearColor ItemPatternDenseColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Conveyor|Transport Label", meta = (DisplayName = "ItemPatternSpecialColor"))
	FLinearColor ItemPatternSpecialColor;

	TMap<FSRConveyorLaneKey, FSRConveyorSegment> Segments;
	TArray<FSRConveyorBeltPath> BeltPaths;

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
	bool ApplySaveDataUnchecked(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorNetworkSaveData& SaveData);
	void DestroyPlacedConveyorActors();
	void ScheduleDirtyConveyorActorGroupRefresh(USRPlanetSurfaceGrid* SurfaceGrid);
	bool RefreshDirtyConveyorActorGroups(USRPlanetSurfaceGrid* SurfaceGrid, int32 MaxGroupCount = INDEX_NONE);
	bool HasDirtyConveyorActorGroups() const;
	void LogConveyorMutationMemoryDiagnostics(const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection) const;
	bool RebuildPlacedConveyorActors(USRPlanetSurfaceGrid* SurfaceGrid);
	void RefreshConveyorRibbonMesh(USRPlanetSurfaceGrid* SurfaceGrid);
	void RefreshPCGSplineInputs(USRPlanetSurfaceGrid* SurfaceGrid);
	void RefreshPathDebugLines(USRPlanetSurfaceGrid* SurfaceGrid);
	void ProcessConveyorTransport(USRPlanetSurfaceGrid* SurfaceGrid, float DeltaTime);
	void RefreshConveyorItemLabels(USRPlanetSurfaceGrid* SurfaceGrid, float DeltaTime);
	void DestroyConveyorItemLabels();
	bool ShouldKeepTransportTickEnabled() const;
	void RequestPCGGeneration();
	void BindPCGGenerationDelegates();
	void HandlePCGGraphGenerated(UPCGComponent* PCGComponent);

	UPROPERTY(Transient)
	FSRConveyorTransportRuntimeState TransportState;

	FSRConveyorActorGroupRuntimeState ActorGroupState;
};
