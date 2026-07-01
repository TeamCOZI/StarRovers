#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBody.h"
#include "SRPlanet.generated.h"

class ULineBatchComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UDynamicMeshComponent;
class USRDynamicMeshBaseDataAsset;
class USRPlanetSurfaceGrid;
class USRConveyorNetworkComponent;
class USRStructureInstanceManagerComponent;
class USROrbit;
class USplineMeshComponent;
class UStaticMesh;
class USRFacilityNetworkComponent;
class USRCelestialRingMeshComponent;

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
	UFUNCTION(BlueprintPure, Category = "StarRovers|Conveyor")
	USRConveyorNetworkComponent* GetConveyorNetwork() const;
	UFUNCTION(BlueprintPure, Category = "StarRovers|Structure")
	USRStructureInstanceManagerComponent* GetStructureInstanceManager() const;
	UFUNCTION(BlueprintPure, Category = "StarRovers|Automation")
	USRFacilityNetworkComponent* GetFacilityNetwork() const;
	virtual void SetCelestialBodyMesh(bool bUseDynamicMesh) override;
	virtual void RefreshRotationAxisLineVisual() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OceanDynamicMesh"))
	TObjectPtr<UDynamicMeshComponent> OceanDynamicMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "AtmosphereDynamicMesh"))
	TObjectPtr<UDynamicMeshComponent> AtmosphereDynamicMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "Orbit"))
	TObjectPtr<USROrbit> Orbit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SurfaceGrid"))
	TObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "ConveyorNetwork"))
	TObjectPtr<USRConveyorNetworkComponent> ConveyorNetwork;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "StructureInstanceManager"))
	TObjectPtr<USRStructureInstanceManagerComponent> StructureInstanceManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "FacilityNetwork"))
	TObjectPtr<USRFacilityNetworkComponent> FacilityNetwork;

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
	void ApplyOceanMeshSettings();
	void ApplyOceanDynamicMeshSettings();
	float ResolveOceanDynamicMeshScale() const;
	float ComputeProceduralOceanLocalRadius() const;
	void ApplyAtmosphereMeshSettings();
	void ApplyAtmosphereDynamicMeshSettings();
	virtual void ApplyToonOutlineSettings() override;
	void ConfigureShellDynamicMeshComponent(UDynamicMeshComponent* ShellMesh) const;
	float ResolveAtmosphereDynamicMeshScale() const;
	float ComputeRotationAxisSurfaceRadius() const;
	float ComputeRotationAxisLineRadius() const;
	bool BuildShellDynamicMesh(
		UDynamicMeshComponent* TargetComponent,
		USRDynamicMeshBaseDataAsset* ShellBaseDataAsset,
		TObjectPtr<USRDynamicMeshBaseDataAsset>& InOutCachedBaseDataAsset,
		uint32& InOutCachedBuildHash,
		const TCHAR* ShellName);
	uint32 ComputeShellDynamicMeshBuildHash(const USRDynamicMeshBaseDataAsset* ShellBaseDataAsset) const;
	void EnsureSurfaceGrid();
	void HideSurfaceGrid();
	bool SupportsSurfaceGrid() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OrbitLineBatch", AllowPrivateAccess = "true"))
	TObjectPtr<ULineBatchComponent> OrbitLineBatch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "OrbitRingVisual", AllowPrivateAccess = "true"))
	TObjectPtr<USRCelestialRingMeshComponent> OrbitRingVisual;

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

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanDynamicMeshBaseDataAsset", AllowPrivateAccess = "true"))
	TObjectPtr<USRDynamicMeshBaseDataAsset> OceanDynamicMeshBaseDataAsset;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanMaterial", AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> OceanMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "OceanScaleMultiplier", AllowPrivateAccess = "true"))
	float OceanScaleMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "bHasAtmosphere", AllowPrivateAccess = "true"))
	bool bHasAtmosphere;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "AtmosphereDynamicMeshBaseDataAsset", AllowPrivateAccess = "true"))
	TObjectPtr<USRDynamicMeshBaseDataAsset> AtmosphereDynamicMeshBaseDataAsset;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "AtmosphereMaterial", AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> AtmosphereMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime", meta = (DisplayName = "AtmosphereScaleMultiplier", AllowPrivateAccess = "true"))
	float AtmosphereScaleMultiplier;

	UPROPERTY(Transient)
	TObjectPtr<USRDynamicMeshBaseDataAsset> CachedOceanDynamicMeshBaseDataAsset;

	UPROPERTY(Transient)
	TObjectPtr<USRDynamicMeshBaseDataAsset> CachedAtmosphereDynamicMeshBaseDataAsset;

	uint32 CachedOceanDynamicMeshBuildHash = 0;
	uint32 CachedAtmosphereDynamicMeshBuildHash = 0;

};
