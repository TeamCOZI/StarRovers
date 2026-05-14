#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyComponent.generated.h"

class ASRPlayerController;
class USRPlanetSurfaceGrid;

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRAssemblyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USRAssemblyComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

private:
	ASRPlayerController* GetOwnerController() const;
	bool GetCursorRay(FVector& OutRayOrigin, FVector& OutRayDirection) const;
	bool TryGetFocusedSurfaceGrid(AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid) const;
	bool TryProjectCursorToSurfaceCell(USRPlanetSurfaceGrid* SurfaceGrid, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const;
	void UpdateSurfaceHover();
	void ApplyAssemblyModeToFocusedSurfaceGrid();
	void HideAllSurfaceGridsExcept(USRPlanetSurfaceGrid* SurfaceGridToKeep);
};
