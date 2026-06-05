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

private:
	static FSRConveyorLaneKey MakeLaneKey(const FSRPlanetSurfaceGridCellId& CellId, int32 Layer);
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
	void RefreshConveyorVisuals(USRPlanetSurfaceGrid* SurfaceGrid);
	void RefreshPCGSplineInputs(USRPlanetSurfaceGrid* SurfaceGrid);
	void RefreshPathDebugLines(USRPlanetSurfaceGrid* SurfaceGrid);
	void RequestPCGGeneration();
	void BindPCGGenerationDelegates();
	void HandlePCGGraphGenerated(UPCGComponent* PCGComponent);
	void RebaseGeneratedPCGSplineMeshes(UPCGComponent* PCGComponent);
};
