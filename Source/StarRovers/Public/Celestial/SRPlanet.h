#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBody.h"
#include "SRPlanet.generated.h"

class ULineBatchComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USRPlanetSurfaceGrid;
class USROrbit;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class STARROVERS_API ASRPlanet : public ASRCelestialBody
{
	GENERATED_BODY()

public:
	ASRPlanet();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetData(const FSRCelestialBodyData& NewData) override;
	virtual void ApplyData() override;
	virtual FSRCelestialBodyData GetData() const override;
	virtual USROrbit* GetOrbit() const override;
	virtual USRPlanetSurfaceGrid* GetSurfaceGrid() const override;
	virtual void SetCelestialBodyMesh(bool bUseDynamicMesh) override;
	virtual void RefreshRotationAxisLineVisual() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OceanStaticMesh"))
	TObjectPtr<UStaticMeshComponent> OceanStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "AtmosphereStaticMesh"))
	TObjectPtr<UStaticMeshComponent> AtmosphereStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Orbit"))
	TObjectPtr<USROrbit> Orbit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SurfaceGrid"))
	TObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "GridOverlayMaterial"))
	TObjectPtr<UMaterialInterface> GridOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "ShowOrbitLine"))
	bool ShowOrbitLine;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitLineColor"))
	FLinearColor OrbitLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitLineOpacity", ClampMin = "0.0", ClampMax = "1.0"))
	float OrbitLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitLineThickness", ClampMin = "0.0"))
	float OrbitLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitLineSegments", ClampMin = "3"))
	int32 OrbitLineSegments;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Axis", meta = (DisplayName = "ShowRotationAxisLine"))
	bool ShowRotationAxisLine;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Axis", meta = (DisplayName = "RotationAxisLineColor"))
	FLinearColor RotationAxisLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Axis", meta = (DisplayName = "RotationAxisLineOpacity", ClampMin = "0.0", ClampMax = "1.0"))
	float RotationAxisLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Axis", meta = (DisplayName = "RotationAxisLineThickness", ClampMin = "0.0"))
	float RotationAxisLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Axis", meta = (DisplayName = "RotationAxisLineLengthMultiplier", ClampMin = "0.0"))
	float RotationAxisLineLengthMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Axis", meta = (DisplayName = "RotationAxisSplineMesh"))
	TObjectPtr<UStaticMesh> RotationAxisSplineMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Axis", meta = (DisplayName = "RotationAxisMaterial"))
	TObjectPtr<UMaterialInterface> RotationAxisMaterial;

private:
	void ApplyOceanStaticMeshSettings();
	float ResolveOceanScale() const;
	float EstimateProceduralOceanScaleMultiplier() const;
	void ApplyAtmosphereStaticMeshSettings();
	float ResolveAtmosphereScale() const;
	float ComputeRotationAxisSurfaceRadius() const;
	float ComputeRotationAxisLineRadius() const;
	void EnsureSurfaceGrid();
	void HideSurfaceGrid();
	bool SupportsSurfaceGrid() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OrbitLineBatch", AllowPrivateAccess = "true"))
	TObjectPtr<ULineBatchComponent> OrbitLineBatch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "RotationAxisNorthSpline", AllowPrivateAccess = "true"))
	TObjectPtr<USplineMeshComponent> RotationAxisNorthSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "RotationAxisSouthSpline", AllowPrivateAccess = "true"))
	TObjectPtr<USplineMeshComponent> RotationAxisSouthSpline;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RotationAxisNorthMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RotationAxisSouthMaterialInstance;

	UPROPERTY()
	bool CanConstruct;

	UPROPERTY()
	float SurfaceGridHeightOffset;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "bHasOcean", AllowPrivateAccess = "true"))
	bool bHasOcean;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanMesh", AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> OceanMesh;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanMaterial", AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> OceanMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanScaleMultiplier", AllowPrivateAccess = "true"))
	float OceanScaleMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "bHasAtmosphere", AllowPrivateAccess = "true"))
	bool bHasAtmosphere;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "AtmosphereMesh", AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> AtmosphereMesh;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "AtmosphereMaterial", AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> AtmosphereMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "AtmosphereScaleMultiplier", AllowPrivateAccess = "true"))
	float AtmosphereScaleMultiplier;

};
