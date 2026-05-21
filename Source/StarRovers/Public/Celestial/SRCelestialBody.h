#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Surface/SRPlanetTerrainTypes.h"
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
class UWidgetComponent;
class USRGravityParent;
class USRCelestialBodyRegistrySubsystem;
class USRTimeControlSubsystem;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodySpec
{
	GENERATED_BODY()

	FSRCelestialBodySpec();

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Unknown;

	UPROPERTY()
	TObjectPtr<AActor> ParentBody = nullptr;

	UPROPERTY()
	float OrbitRadius = 0.0f;

	UPROPERTY()
	float OrbitPeriodInPeriods = 0.0f;

	UPROPERTY()
	float StartingPhase = 0.0f;

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
	float SurfaceGridSurfaceOffset = 0.0f;

	UPROPERTY()
	float ConstructionHeightOffset = 15.0f;

	UPROPERTY()
	float BodyScale = 1000.0f;

	float ApproximateRadius = 50000.0f;

	UPROPERTY()
	float Mass = 2000.0f;

	UPROPERTY()
	float StarPointLightIntensity = -1.0f;

	UPROPERTY()
	float StarMaterialEmissiveStrength = -1.0f;

	UPROPERTY()
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	UPROPERTY()
	bool bUseProceduralTerrain = false;

	UPROPERTY()
	int32 TerrainSeed = 1337;

	UPROPERTY()
	float TerrainHeight = 0.0f;

	UPROPERTY()
	float TerrainFrequency = 3.0f;

	UPROPERTY()
	int32 TerrainOctaves = 4;

	UPROPERTY()
	float TerrainPersistence = 0.5f;

	UPROPERTY()
	FSRPlanetTerrainSettings TerrainSettings;

	UPROPERTY()
	bool bHasOcean = false;

	UPROPERTY()
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	UPROPERTY()
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

	UPROPERTY()
	float GravityRatio = 1.0f;

	UPROPERTY()
	float GravityRadiusRatio = 10.0f;

	UPROPERTY()
	bool bShowGravityLine = true;

	UPROPERTY()
	FLinearColor GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);

	UPROPERTY()
	float GravityLineOpacity = 0.85f;

	UPROPERTY()
	int32 GravityLineSegments = 96;

	UPROPERTY()
	float GravityLineThickness = 20.0f;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyBiomeSpec
{
	GENERATED_BODY()

	FSRCelestialBodyBiomeSpec();

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	bool bUseProceduralTerrain = true;

	UPROPERTY()
	int32 TerrainSeed = 1337;

	UPROPERTY()
	float TerrainHeight = 0.0f;

	UPROPERTY()
	float TerrainFrequency = 3.0f;

	UPROPERTY()
	int32 TerrainOctaves = 4;

	UPROPERTY()
	float TerrainPersistence = 0.5f;

	UPROPERTY()
	FSRPlanetTerrainSettings TerrainSettings;

	UPROPERTY()
	bool bHasOcean = false;

	UPROPERTY()
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	UPROPERTY()
	float OceanScaleMultiplier = 0.97f;

	UPROPERTY()
	float SurfaceGridSurfaceOffset = 0.0f;

	UPROPERTY()
	float OrbitSpeed = 1.0f;

	UPROPERTY()
	float StarPointLightIntensity = -1.0f;

	UPROPERTY()
	float StarMaterialEmissiveStrength = -1.0f;

	UPROPERTY()
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
	UDynamicMeshComponent* GetCelestialBodyDynamicMesh() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Lighting")
	virtual void SetDynamicMeshEnabled(bool bUseDynamicMesh);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Lighting")
	float GetDynamicMeshThreshold() const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SceneRoot"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "CelestialBodyDynamicMesh"))
	TObjectPtr<UDynamicMeshComponent> CelestialBodyDynamicMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "CelestialBodyStaticMesh"))
	TObjectPtr<UStaticMeshComponent> CelestialBodyStaticMesh_;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "ClickSphereCollision"))
	TObjectPtr<USphereComponent> ClickSphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "GravityParent"))
	TObjectPtr<USRGravityParent> GravityParent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "GravityLineBatch"))
	TObjectPtr<ULineBatchComponent> GravityLineBatch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "CircleWidget"))
	TObjectPtr<UWidgetComponent> CircleWidget;

	UPROPERTY()
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "ShowGravityLine"))
	bool bShowGravityLine;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityLineColor"))
	FLinearColor GravityLineColor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityLineOpacity", ClampMin = "0.0", ClampMax = "1.0"))
	float GravityLineOpacity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityLineThickness", ClampMin = "0.0"))
	float GravityLineThickness;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityLineSegments", ClampMin = "3"))
	int32 GravityLineSegments;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Lighting", meta = (DisplayName = "DynamicMeshThreshold", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DynamicMeshThreshold;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusZoomMultiplier", ClampMin = "0.0"))
	float FocusZoomMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "GenerationSeed"))
	int32 GenerationSeed;

	UPROPERTY()
	FSRPlanetTerrainSettings TerrainSettings;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CelestialBodyStaticMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> CelestialBodyMaterial;

	float ComputeCelestialBodyDynamicMeshRadius() const;

	UMaterialInstanceDynamic* GetActiveBodyDynamicMaterial() const;

private:
	void ApplyGravityLineSettings();
	void EnsureCelestialBodyDynamicMeshVisuals();
	bool CopySourceStaticMeshToCelestialBodyDynamicMesh();
	void SyncApproximateRadiusFromCelestialBodyDynamicMesh();
	void UpdateCircleWidgetDrawSize();
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
