#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBody.h"
#include "SRPlanet.generated.h"

class ULineBatchComponent;
class UMaterialInterface;
class USRPlanetSurfaceGrid;
class USROrbit;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class STARROVERS_API ASRPlanet : public ASRCelestialBody
{
	GENERATED_BODY()

public:
	ASRPlanet();

	virtual void ApplySpec(const FSRCelestialBodySpec& NewSpec) override;
	virtual void ApplyConfiguredBodyState() override;
	virtual FSRCelestialBodySpec GetSpec() const override;
	virtual USROrbit* GetOrbitComponent() const override;
	virtual USRPlanetSurfaceGrid* GetSurfaceGrid() const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CastShadowStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> OceanStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Orbit"))
	TObjectPtr<USROrbit> Orbit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "Construction Height Offset"))
	float ConstructionHeightOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "Grid Line Color"))
	FLinearColor GridLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "Grid Line Opacity", ClampMin = "0.0", ClampMax = "1.0"))
	float GridLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "Grid Line Thickness", ClampMin = "0.0"))
	float GridLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "Hovered Cell Color"))
	FLinearColor HoveredCellColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "Selected Cell Color"))
	FLinearColor SelectedCellColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "Occupied Cell Color"))
	FLinearColor OccupiedCellColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "Show Orbit Line"))
	bool bShowOrbitLine;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "Orbit Line Color"))
	FLinearColor OrbitLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "Orbit Line Opacity", ClampMin = "0.0", ClampMax = "1.0"))
	float OrbitLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "Orbit Line Thickness", ClampMin = "0.0"))
	float OrbitLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "Orbit Line Segments", ClampMin = "3"))
	int32 OrbitLineSegments;

private:
	void ApplyOceanStaticMeshSettings();
	void ApplyCastShadowStaticMeshSettings();
	void SyncApproximateRadiusFromPlanetVisuals();
	void EnsureSurfaceGrid();
	void HideSurfaceGrid();
	bool SupportsSurfaceGrid() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULineBatchComponent> OrbitLineBatch;

	UPROPERTY()
	bool CanConstruct;

	UPROPERTY()
	float SurfaceGridSurfaceOffset;

	float CastShadowScaleMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (AllowPrivateAccess = "true"))
	bool bHasOcean;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> OceanMesh;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> OceanMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (AllowPrivateAccess = "true"))
	float OceanScaleMultiplier;

};
