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
	virtual void SetDynamicMeshEnabled(bool bUseDynamicMesh) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OceanStaticMesh"))
	TObjectPtr<UStaticMeshComponent> OceanStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Orbit"))
	TObjectPtr<USROrbit> Orbit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SurfaceGrid"))
	TObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "ConstructionHeightOffset"))
	float ConstructionHeightOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "GridLineColor"))
	FLinearColor GridLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "GridLineOpacity", ClampMin = "0.0", ClampMax = "1.0"))
	float GridLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "GridLineThickness", ClampMin = "0.0"))
	float GridLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "HoveredCellColor"))
	FLinearColor HoveredCellColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "SelectedCellColor"))
	FLinearColor SelectedCellColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "OccupiedCellColor"))
	FLinearColor OccupiedCellColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "ShowOrbitLine"))
	bool bShowOrbitLine;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitLineColor"))
	FLinearColor OrbitLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitLineOpacity", ClampMin = "0.0", ClampMax = "1.0"))
	float OrbitLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitLineThickness", ClampMin = "0.0"))
	float OrbitLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitLineSegments", ClampMin = "3"))
	int32 OrbitLineSegments;

private:
	void ApplyOceanStaticMeshSettings();
	void SyncApproximateRadiusFromPlanetVisuals();
	void EnsureSurfaceGrid();
	void HideSurfaceGrid();
	bool SupportsSurfaceGrid() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OrbitLineBatch", AllowPrivateAccess = "true"))
	TObjectPtr<ULineBatchComponent> OrbitLineBatch;

	UPROPERTY()
	bool CanConstruct;

	UPROPERTY()
	float SurfaceGridSurfaceOffset;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "bHasOcean", AllowPrivateAccess = "true"))
	bool bHasOcean;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanMesh", AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> OceanMesh;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanMaterial", AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> OceanMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanScaleMultiplier", AllowPrivateAccess = "true"))
	float OceanScaleMultiplier;

};
