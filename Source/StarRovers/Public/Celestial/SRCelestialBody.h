#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "SRCelestialBody.generated.h"

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USRMoonDataAsset;
class ULineBatchComponent;
class USRPlanetDataAsset;
class USRPlanetSurfaceGrid;
class USceneComponent;
class USphereComponent;
class USRStarDataAsset;
class USROrbit;
class UStaticMesh;
class UStaticMeshComponent;
class USRGravityParent;
class USRCelestialBodyRegistrySubsystem;
class USRTimeControlSubsystem;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodySpec
{
	GENERATED_BODY()

	FSRCelestialBodySpec();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|01 Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|01 Identity")
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|02 Orbit")
	TObjectPtr<AActor> ParentBody = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|02 Orbit", meta = (ClampMin = "0.0"))
	float OrbitRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|02 Orbit", meta = (ClampMin = "0.0"))
	float OrbitPeriodInPeriods = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|02 Orbit")
	float StartingPhase = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Focus", meta = (ClampMin = "0.0", ToolTip = "Multiplier applied to the auto-computed focus distance derived from this body's visual scale."))
	float FocusZoomMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|04 Surface|Construction")
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
	float SurfaceGridSurfaceOffset = 8.0f;

	UPROPERTY()
	float ConstructionHeightOffset = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|05 Body", meta = (ClampMin = "0.0"))
	float BodyScale = 1000.0f;

	float ApproximateRadius = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|10 Gravity|Source", meta = (ClampMin = "0.0"))
	float Mass = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|07 Lighting|Star", meta = (ClampMin = "-1.0", UIMin = "0.0", ToolTip = "Point light intensity for stars. Negative values are treated as missing data."))
	float StarPointLightIntensity = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|07 Lighting|Star", meta = (ClampMin = "-1.0", UIMin = "0.0", ToolTip = "Emissive scalar applied to star materials. Negative values are treated as missing data."))
	float StarMaterialEmissiveStrength = -1.0f;

	float ShadowCasterScaleMultiplier = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|07 Lighting|Star")
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|08 Terrain|Shape")
	bool bUseProceduralTerrain = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|08 Terrain|Shape")
	int32 TerrainSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|08 Terrain|Shape", meta = (ClampMin = "0.0"))
	float TerrainHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|08 Terrain|Shape", meta = (ClampMin = "0.01"))
	float TerrainFrequency = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "StarRovers|08 Terrain|Shape", meta = (ClampMin = "1"))
	int32 TerrainOctaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "StarRovers|08 Terrain|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TerrainPersistence = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|09 Ocean", meta = (ToolTip = "Render a water/base sphere at the configured sea level. Land terrain rises above this sphere; lower terrain is visually covered by it."))
	bool bHasOcean = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|09 Ocean", meta = (EditCondition = "bHasOcean", ToolTip = "Optional mesh used by the visible ocean/base sphere."))
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|09 Ocean", meta = (EditCondition = "bHasOcean", ToolTip = "Optional material used by the visible ocean/base sphere."))
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|09 Ocean", meta = (EditCondition = "bHasOcean", ClampMin = "0.01", ToolTip = "Scale multiplier applied to CelestialBodyStaticMesh scale for the visible ocean mesh."))
	float OceanScaleMultiplier = 0.97f;

	UPROPERTY()
	bool bShowOrbitLine = true;

	UPROPERTY()
	FLinearColor OrbitLineColor = FLinearColor(0.2f, 0.75f, 1.0f, 1.0f);

	UPROPERTY()
	float OrbitLineOpacity = 0.85f;

	UPROPERTY()
	int32 OrbitLineSegments = 96;

	UPROPERTY()
	float OrbitLineThickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|10 Gravity|Source", meta = (ClampMin = "0.0"))
	float GravityRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|10 Gravity|Source", meta = (ClampMin = "0.0"))
	float GravityRadiusRatio = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|10 Gravity|Visual")
	bool bShowGravityLine = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|10 Gravity|Visual")
	FLinearColor GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|10 Gravity|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float GravityLineOpacity = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "StarRovers|10 Gravity|Visual", meta = (ClampMin = "3", UIMin = "3"))
	int32 GravityLineSegments = 96;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "StarRovers|10 Gravity|Visual", meta = (ClampMin = "0.0", ToolTip = "Per-body gravity field line thickness used by the owned gravity source visual."))
	float GravityLineThickness = 20.0f;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyBiomeSpec
{
	GENERATED_BODY()

	FSRCelestialBodyBiomeSpec();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|01 Biome Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|02 Terrain")
	bool bUseProceduralTerrain = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|02 Terrain")
	int32 TerrainSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|02 Terrain", meta = (ClampMin = "0.0"))
	float TerrainHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|02 Terrain", meta = (ClampMin = "0.01"))
	float TerrainFrequency = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "StarRovers|02 Terrain", meta = (ClampMin = "1"))
	int32 TerrainOctaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "StarRovers|02 Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TerrainPersistence = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|03 Ocean")
	bool bHasOcean = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|03 Ocean", meta = (EditCondition = "bHasOcean"))
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|03 Ocean", meta = (EditCondition = "bHasOcean"))
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|03 Ocean", meta = (EditCondition = "bHasOcean", ClampMin = "0.01"))
	float OceanScaleMultiplier = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "StarRovers|04 Surface Grid", meta = (ClampMin = "0.0"))
	float SurfaceGridSurfaceOffset = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|05 Shadow", meta = (ClampMin = "0.01"))
	float ShadowCasterScaleMultiplier = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|06 Orbit", meta = (DisplayName = "Orbit Speed", ClampMin = "0.0", ToolTip = "Orbit revolutions per simulation period. Zero disables orbit movement."))
	float OrbitSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|04 Star Appearance", meta = (ClampMin = "-1.0", UIMin = "0.0", ToolTip = "Point light intensity for star biomes. Negative values are treated as missing data."))
	float StarPointLightIntensity = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|04 Star Appearance", meta = (ClampMin = "-1.0", UIMin = "0.0", ToolTip = "Emissive scalar for star biomes. Negative values are treated as missing data."))
	float StarMaterialEmissiveStrength = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|04 Star Appearance")
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

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
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	virtual void ApplySpec(const FSRCelestialBodySpec& NewSpec);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	void ApplyBiomeSpec(const FSRCelestialBodyBiomeSpec& NewBiomeSpec);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	virtual void ApplyConfiguredBodyState();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	virtual FSRCelestialBodySpec GetSpec() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	float ConvertPeriodsToSeconds(float PeriodValue) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	UStaticMeshComponent* GetCelestialBodyStaticMesh() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	ESRCelestialBodyCategory GetBodyCategory() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Celestial")
	USRGravityParent* GetGravityParent() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Orbit")
	virtual USROrbit* GetOrbitComponent() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
	virtual USRPlanetSurfaceGrid* GetSurfaceGrid() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Visual")
	void SetCelestialBodyAssets(UStaticMesh* NewCelestialBodyStaticMesh, UMaterialInterface* NewCelestialBodyMaterial);

protected:
	static constexpr const TCHAR* HideForStarEditCondition = TEXT("BodyCategory != ESRCelestialBodyCategory::Star");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "CelestialBodyStaticMesh"))
	TObjectPtr<UStaticMeshComponent> CelestialBodyStaticMesh_;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> ClickSphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USRGravityParent> GravityParent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULineBatchComponent> GravityLineBatch;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Runtime")
	FText DisplayName;

	UPROPERTY()
	ESRCelestialBodyCategory BodyCategory;

	UPROPERTY()
	float BodyScale;

	float ApproximateRadius;

	UPROPERTY()
	float Mass;

	UPROPERTY()
	float GravityRatio;

	UPROPERTY()
	float GravityRadiusRatio;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Show Gravity Line"))
	bool bShowGravityLine;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Gravity Line Color"))
	FLinearColor GravityLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Gravity Line Opacity", ClampMin = "0.0", ClampMax = "1.0"))
	float GravityLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Gravity Line Thickness", ClampMin = "0.0"))
	float GravityLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Gravity Line Segments", ClampMin = "3"))
	int32 GravityLineSegments;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "Focus Zoom Multiplier", ClampMin = "0.0"))
	float FocusZoomMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "Generation Seed"))
	int32 GenerationSeed;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CelestialBodyStaticMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> CelestialBodyMaterial;

	float ComputeCelestialBodyStaticMeshRadius() const;

	UMaterialInstanceDynamic* GetActiveBodyDynamicMaterial() const;

private:
	void ApplyGravityLineSettings();
	void EnsureCelestialBodyStaticMeshVisuals();
	void SyncApproximateRadiusFromCelestialBodyStaticMesh();
	USRCelestialBodyRegistrySubsystem* FindCelestialRegistry() const;
	USRTimeControlSubsystem* FindTimeControlSubsystem() const;
	bool IsStellarBody() const;
	float ResolveSecondsPerPeriod() const;
	void LogMissingSpecErrorOnce(const TCHAR* Context) const;

	UPROPERTY(Transient)
	bool bHasAppliedSpec = false;

	UPROPERTY(Transient)
	mutable bool bHasLoggedMissingSpecError = false;

};
