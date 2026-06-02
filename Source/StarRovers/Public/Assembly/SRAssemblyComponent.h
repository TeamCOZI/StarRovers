#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyComponent.generated.h"

class ASRPlayerController;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRAssemblyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USRAssemblyComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
	bool IsAssemblyModeActive() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetAssemblyModeActive(bool bNewAssemblyModeActive);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void ToggleAssemblyMode();

	bool TryHandleAssemblyClick(AActor*& OutSelectedActor);
	void ClearSurfaceGridInteraction(AActor* SurfaceActor);
	void ClearSurfaceHover();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "HoveredSurfaceGrid"))
	TObjectPtr<USRPlanetSurfaceGrid> HoveredSurfaceGrid;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "bAssemblyModeActive"))
	bool bAssemblyModeActive;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ActiveAssemblySurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastHoveredSampleSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastPublishedHoveredSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<AActor> StructureGhostActor;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> StructureGhostDataAsset;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> LastLoggedInvalidGhostDataAsset;

	FVector2D LastHoveredSampleMousePosition;
	bool bHasLastHoveredSampleMousePosition;
	bool bHasLastPublishedHoveredCellInfo;
	FSRPlanetSurfaceGridCellId LastPublishedHoveredCellId;
	FSRPlanetSurfaceGridCellId StructureGhostCellId;
	bool bHasStructureGhostCellId;

private:
	ASRPlayerController* GetOwnerController() const;
	bool GetCursorRay(FVector& OutRayOrigin, FVector& OutRayDirection) const;
	bool TryGetFocusedSurfaceGrid(AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid) const;
	bool TryProjectCursorToSurfaceCell(USRPlanetSurfaceGrid* SurfaceGrid, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const;
	void UpdateSurfaceHover();
	void ApplyAssemblyModeToFocusedSurfaceGrid();
	void ResetHoverSampleCache();
	void PublishHoveredCellInfo(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell);
	void ClearPublishedHoveredCellInfo();
	void UpdateStructureGhostPreview();
	void DestroyStructureGhostPreview();
	bool BuildStructureGhostTransform(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId, USRStructureDataAsset* StructureDataAsset, FTransform& OutTransform) const;
	void PublishStructureGhostPlacementDebug(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell, const FTransform& GhostTransform, float StructureHeightOffset, bool bLogDebug) const;
	bool TryPlaceSelectedStructure(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell);
	void LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason);
};
