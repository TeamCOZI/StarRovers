#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Celestial/SRCelestialBodyData.h"
#include "Celestial/SRCelestialBodyDynamicMeshTypes.h"
#include "Simulation/SRNaturalStructureSpawnTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Surface/SRPlanetTerrainTypes.h"
#include "SRCelestialBody.generated.h"

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

class UMaterialInstanceDynamic;
class UMaterialInterface;
class ULineBatchComponent;
class USRPlanetSurfaceGrid;
class USceneComponent;
class USphereComponent;
class USROrbit;
class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;
class USRGravityParent;
class USRCelestialRingMeshComponent;
class USRCelestialBodyRegistrySubsystem;
class USRDynamicMeshBaseDataAsset;
class USRPlanetTerrainProfileDataAsset;

UCLASS(Blueprintable)
class STARROVERS_API ASRCelestialBody : public AActor
{
	GENERATED_BODY()

public:
	ASRCelestialBody();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	virtual void SetData(const FSRCelestialBodyData& NewData);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	virtual void ApplyData();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	virtual FSRCelestialBodyData GetData() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	UDynamicMeshComponent* GetCelestialBodyDynamicMesh() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	void RefreshMaterialParameters();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial|Visual")
	virtual void RefreshRotationAxisLineVisual();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Lighting")
	virtual void SetCelestialBodyMesh(bool bUseDynamicMesh);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	ESRCelestialBodyCategory GetBodyCategory() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	USRGravityParent* GetGravityParent() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	virtual USROrbit* GetOrbit() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
	virtual USRPlanetSurfaceGrid* GetSurfaceGrid() const;

	bool ApplySurfaceCellHighlights(
		const TArray<FSRPlanetSurfaceGridCellId>& HoveredCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& SelectedCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& OccupiedPreviewCellIds,
		const FLinearColor& HoveredCellColor,
		const FLinearColor& SelectedCellColor,
		const FLinearColor& OccupiedCellColor);
	void ClearSurfaceCellHighlights();
	bool HasSurfaceCellRenderData(const FSRPlanetSurfaceGridCellId& CellId) const;
	bool ApplySurfaceTemperatureStateColor(const FSRPlanetSurfaceGridCellId& CellId, ESRFacilityTemperatureState TemperatureState);
	bool GetCachedSurfaceGridCells(TArray<FSRPlanetSurfaceGridCell>& OutCells) const;
	bool PrepareCelestialBodyDynamicMesh();
	bool BuildPreparedCelestialBodyDynamicMesh(FSRCelestialBodyPreparedDynamicMesh& OutPreparedMesh);
	bool ApplyPreparedCelestialBodyDynamicMesh(FSRCelestialBodyPreparedDynamicMesh&& PreparedMesh, double TotalStart);
	bool HasCelestialBodyDynamicMeshBuild() const;
	static void AppendRuntimeMemoryDiagnostics(TArray<FString>& OutLines);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SceneRoot"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "CelestialBodyDynamicMesh"))
	TObjectPtr<UDynamicMeshComponent> CelestialBodyDynamicMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "CelestialBodyDynamicMeshFaces"))
	TArray<TObjectPtr<UDynamicMeshComponent>> CelestialBodyDynamicMeshFaces;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "CelestialBodyStaticMesh"))
	TObjectPtr<UStaticMeshComponent> CelestialBodyStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "ClickSphereCollision"))
	TObjectPtr<USphereComponent> ClickSphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "GravityParent"))
	TObjectPtr<USRGravityParent> GravityParent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "GravityLineBatch"))
	TObjectPtr<ULineBatchComponent> GravityLineBatch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "GravityRingVisual"))
	TObjectPtr<USRCelestialRingMeshComponent> GravityRingVisual;

	FText VariableName;

	ESRCelestialBodyCategory BodyCategory;

	float Scale;

	float Mass;

	float GravityRatio;

	float GravityRadiusRatio;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "ShowGravityLine"))
	bool ShowGravityLine;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityLineColor"))
	FLinearColor GravityLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityLineOpacity", ClampMin = "0.0", ClampMax = "1.0"))
	float GravityLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityLineThickness", ClampMin = "0.0"))
	float GravityLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityLineSegments", ClampMin = "3"))
	int32 GravityLineSegments;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusZoomMultiplier", ClampMin = "0.0"))
	float FocusZoomMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "GenerationSeed"))
	int32 GenerationSeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "bRandomizeGenerationSeedEachRun"))
	bool bRandomizeGenerationSeedEachRun;

	UPROPERTY()
	TObjectPtr<USRPlanetTerrainProfileDataAsset> TerrainProfileDataAsset = nullptr;

	UPROPERTY()
	TArray<FSRNaturalStructureSpawnRuleOverride> ProfileNaturalStructureSpawnRuleOverrides;

	FSRDynamicMeshGeneration DynamicMeshGeneration;

	UPROPERTY()
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "DynamicMeshBaseDataAsset"))
	TObjectPtr<USRDynamicMeshBaseDataAsset> DynamicMeshBaseDataAsset;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "ToonOutlineSettings"))
	FSRToonOutlineSettings ToonOutlineSettings;

	UMaterialInstanceDynamic* GetActiveBodyDynamicMaterial() const;
	virtual void ApplyToonOutlineSettings();
	int32 ApplyToonOutlineToBodyMeshComponents();
	bool ApplyToonOutlineToPrimitive(UPrimitiveComponent* PrimitiveComponent, bool bEnableToonOutline) const;

private:
	void InitializeCelestialBodyComponents();
	void InitializeDynamicMeshComponents();
	void InitializeGravityComponents();
	void InitializeCelestialBodyDefaults();
	void CopyBodyDataFields(const FSRCelestialBodyData& NewData);
	void ApplyTerrainProfileData();
	bool ShouldAutoApplyDataAfterSet() const;
	void SanitizeBodyRuntimeValues();
	void ApplyBodyMeshTransforms();
	void UpdateDynamicMeshBuildStateForCurrentData();
	bool ShouldBuildDynamicMeshForCurrentWorld() const;
	void ApplyClickCollisionForCurrentBody();
	FSRCelestialBodyData BuildBodyDataSnapshot() const;
	void ApplyGravityLineSettings();
	void EnsureCelestialBodyMeshRendering(bool bBuildDynamicMesh);
	bool ApplyCelestialBodyMeshMaterials();
	bool BuildDynamicMeshFromStaticMeshFallback(uint32 DynamicMeshBuildHash, double TotalStart);
	bool BuildCelestialBodyDynamicMesh();
	UDynamicMeshComponent* GetDynamicMeshFaceComponent(int32 FaceIndex) const;
	void SyncDynamicMeshFaceComponentSettings();
	uint32 ComputeDynamicMeshBuildHash() const;
	void ResetDynamicMeshCellColorData();
	static void ClearDynamicMeshRuntimeCaches(const TCHAR* Reason);
	static UWorld* GetDynamicMeshRuntimeCacheWorld();
	static void SetDynamicMeshRuntimeCacheWorld(UWorld* World);
	const FSRCelestialBodyDynamicMeshCellColorData* FindDynamicMeshCellColorData(const FSRPlanetSurfaceGridCellId& CellId) const;
	USRCelestialBodyRegistrySubsystem* FindCelestialRegistry() const;
	bool IsStellarBody() const;
	void LogMissingDataErrorOnce(const TCHAR* Context) const;

	bool bHasAppliedData = false;

	mutable bool bHasLoggedMissingDataError = false;

	FSRCelestialBodyDynamicMeshRuntimeState DynamicMeshState;

};
