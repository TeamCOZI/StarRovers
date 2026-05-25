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

	virtual void SetData(const FSRCelestialBodyData& NewData) override;
	virtual void ApplyData() override;
	virtual FSRCelestialBodyData GetData() const override;
	virtual USROrbit* GetOrbit() const override;
	virtual USRPlanetSurfaceGrid* GetSurfaceGrid() const override;
	virtual void SetCelestialBodyMesh(bool bUseDynamicMesh) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OceanStaticMesh"))
	TObjectPtr<UStaticMeshComponent> OceanStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Orbit"))
	TObjectPtr<USROrbit> Orbit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SurfaceGrid"))
	TObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;

	UPROPERTY()
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
	bool ShowOrbitLine;

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
	float ResolveOceanScale() const;
	float EstimateProceduralOceanScaleMultiplier() const;
	void EnsureSurfaceGrid();
	void HideSurfaceGrid();
	bool SupportsSurfaceGrid() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OrbitLineBatch", AllowPrivateAccess = "true"))
	TObjectPtr<ULineBatchComponent> OrbitLineBatch;

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

};
