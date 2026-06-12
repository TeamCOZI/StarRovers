#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GameFramework/Actor.h"
#include "Celestial/SRCelestialBodyCategory.h"
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
class USRGravityParent;
class USRCelestialBodyRegistrySubsystem;
class USRDynamicMeshBaseDataAsset;
class USRPlanetTerrainProfileDataAsset;

struct FSRCelestialBodyDynamicMeshColorElement
{
	int32 MeshComponentIndex = INDEX_NONE;
	int32 ElementId = INDEX_NONE;
	FLinearColor BaseColor = FLinearColor::White;
};

struct FSRCelestialBodyDynamicMeshQuadRenderData
{
	TArray<FSRCelestialBodyDynamicMeshColorElement, TInlineAllocator<8>> ColorElements;
};

struct FSRCelestialBodyDynamicMeshCellColorData
{
	TArray<FSRCelestialBodyDynamicMeshColorElement, TInlineAllocator<4>> SurfaceColorElements;
	TArray<FSRCelestialBodyDynamicMeshColorElement, TInlineAllocator<8>> SideColorElements;
};

struct FSRCelestialBodyPreparedDynamicMesh
{
	bool bValid = false;
	uint32 BuildHash = 0;
	TArray<UE::Geometry::FDynamicMesh3> FaceDynamicMeshes;
	TArray<FSRPlanetSurfaceGridCell> SurfaceGridCells;
	TMap<FSRPlanetSurfaceGridCellId, int32> CellIndexById;
	TArray<int32> CellIndexByFlatId;
	TArray<FSRCelestialBodyDynamicMeshCellColorData> ColorDataByFlatId;
	TArray<FString> DetailLines;
	double BuildMilliseconds = 0.0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyData
{
	GENERATED_BODY()

	FSRCelestialBodyData();

	UPROPERTY()
	FText VariableName;

	UPROPERTY()
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Unknown;

	UPROPERTY()
	TObjectPtr<AActor> ParentBody = nullptr;

	UPROPERTY()
	float OrbitRadius = 0.0f;

	UPROPERTY()
	float OrbitPeriod = 0.0f;

	UPROPERTY()
	float InitialAngle = 0.0f;

	UPROPERTY()
	float FocusZoomMultiplier = 3.0f;

	UPROPERTY()
	bool bCanConstruct = false;

	UPROPERTY()
	float GridLineThickness = 1.0f;

	UPROPERTY()
	FLinearColor GridLineColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);

	UPROPERTY()
	float GridLineOpacity = 1.0f;

	UPROPERTY()
	FLinearColor HoveredCellColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY()
	FLinearColor SelectedCellColor = FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);

	UPROPERTY()
	FLinearColor OccupiedCellColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);

	UPROPERTY()
	float SurfaceGridHeightOffset = 0.0f;

	UPROPERTY()
	float Scale = 1000.0f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY()
	TObjectPtr<USRDynamicMeshBaseDataAsset> DynamicMeshBaseDataAsset = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY()
	float Mass = 2000.0f;

	UPROPERTY()
	float StarPointLightIntensity = -1.0f;

	UPROPERTY()
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	UPROPERTY()
	int32 GenerationSeed = 1000;

	UPROPERTY()
	bool bRandomizeGenerationSeedEachRun = false;

	UPROPERTY()
	FSRDynamicMeshGeneration DynamicMeshGeneration;

	UPROPERTY()
	TObjectPtr<USRPlanetTerrainProfileDataAsset> TerrainProfileDataAsset = nullptr;

	UPROPERTY()
	TArray<FSRNaturalStructureSpawnRuleOverride> ProfileNaturalStructureSpawnRuleOverrides;

	UPROPERTY()
	bool bHasOcean = false;

	UPROPERTY()
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	UPROPERTY()
	float OceanScaleMultiplier = 1.0f;

	UPROPERTY()
	bool bHasAtmosphere = false;

	UPROPERTY()
	TObjectPtr<UStaticMesh> AtmosphereMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> AtmosphereMaterial = nullptr;

	UPROPERTY()
	float AtmosphereScaleMultiplier = 1.0f;

	UPROPERTY()
	bool ShowOrbitLine = true;

	UPROPERTY()
	FLinearColor OrbitLineColor = FLinearColor(0.2f, 0.75f, 1.0f, 1.0f);

	UPROPERTY()
	float OrbitLineOpacity = 0.85f;

	UPROPERTY()
	int32 OrbitLineSegments = 96;

	UPROPERTY()
	float OrbitLineThickness = 20.0f;

	UPROPERTY()
	float GravityRatio = 1.0f;

	UPROPERTY()
	float GravityRadiusRatio = 10.0f;

	UPROPERTY()
	bool ShowGravityLine = true;

	UPROPERTY()
	FLinearColor GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);

	UPROPERTY()
	float GravityLineOpacity = 0.85f;

	UPROPERTY()
	int32 GravityLineSegments = 96;

	UPROPERTY()
	float GravityLineThickness = 20.0f;

	UPROPERTY()
	bool ShowRotationAxisLine = true;

	UPROPERTY()
	FLinearColor RotationAxisLineColor = FLinearColor(1.0f, 0.9f, 0.2f, 1.0f);

	UPROPERTY()
	float RotationAxisLineOpacity = 0.95f;

	UPROPERTY()
	float RotationAxisLineThickness = 18.0f;

	UPROPERTY()
	float RotationAxisLineLengthMultiplier = 1.25f;
};

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
		const FSRPlanetSurfaceGridCellId& HoveredCellId,
		bool bHasHoveredCell,
		const FSRPlanetSurfaceGridCellId& SelectedCellId,
		bool bHasSelectedCell,
		const FLinearColor& HoveredCellColor,
		const FLinearColor& SelectedCellColor);
	void ClearSurfaceCellHighlights();
	bool HasSurfaceCellRenderData(const FSRPlanetSurfaceGridCellId& CellId) const;
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

	UMaterialInstanceDynamic* GetActiveBodyDynamicMaterial() const;

private:
	void ApplyGravityLineSettings();
	void EnsureCelestialBodyDynamicMeshVisuals(bool bBuildDynamicMesh);
	bool BuildDynamicMeshFromBaseMetadata(uint32 DynamicMeshBuildHash, double TotalStart);
	bool BuildCelestialBodyDynamicMesh();
	UDynamicMeshComponent* GetDynamicMeshFaceComponent(int32 FaceIndex) const;
	void SyncDynamicMeshFaceComponentSettings();
	uint32 ComputeDynamicMeshBuildHash() const;
	void ResetDynamicMeshCellColorData();
	const FSRCelestialBodyDynamicMeshCellColorData* FindDynamicMeshCellColorData(const FSRPlanetSurfaceGridCellId& CellId) const;
	USRCelestialBodyRegistrySubsystem* FindCelestialRegistry() const;
	bool IsStellarBody() const;
	void LogMissingDataErrorOnce(const TCHAR* Context) const;

	bool bHasAppliedData = false;

	mutable bool bHasLoggedMissingDataError = false;

	TArray<FSRCelestialBodyDynamicMeshCellColorData> DynamicMeshColorDataByFlatId;
	TSet<uint64> HighlightedDynamicMeshColorElements;
	TMap<uint64, FLinearColor> HighlightedDynamicMeshBaseColorByElement;
	TArray<FSRPlanetSurfaceGridCell> CachedSurfaceGridCells;
	uint32 CachedDynamicMeshBuildHash = 0;
	bool bHasCachedDynamicMeshBuildHash = false;

};
