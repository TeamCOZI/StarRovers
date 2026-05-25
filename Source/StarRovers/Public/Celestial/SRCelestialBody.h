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
class ULineBatchComponent;
class USRPlanetSurfaceGrid;
class USceneComponent;
class USphereComponent;
class USROrbit;
class UStaticMesh;
class UStaticMeshComponent;
class USRGravityParent;
class USRCelestialBodyRegistrySubsystem;

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
	float ConstructionHeightOffset = 15.0f;

	UPROPERTY()
	float BodyScale = 1000.0f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY()
	float Mass = 2000.0f;

	UPROPERTY()
	float StarPointLightIntensity = -1.0f;

	UPROPERTY()
	float StarMaterialEmissiveStrength = -1.0f;

	UPROPERTY()
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	UPROPERTY()
	int32 GenerationSeed = 1337;

	UPROPERTY()
	FSRDynamicMeshGeneration DynamicMeshGeneration;

	UPROPERTY()
	bool bHasOcean = false;

	UPROPERTY()
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	UPROPERTY()
	float OceanScaleMultiplier = 1.0f;

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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "SceneRoot"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "CelestialBodyDynamicMesh"))
	TObjectPtr<UDynamicMeshComponent> CelestialBodyDynamicMesh;

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

	float BodyScale;

	float Mass;

	float GravityRatio;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusZoomMultiplier", ClampMin = "0.0"))
	float FocusZoomMultiplier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "GenerationSeed"))
	int32 GenerationSeed;

	FSRDynamicMeshGeneration DynamicMeshGeneration;

	UPROPERTY()
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	UMaterialInstanceDynamic* GetActiveBodyDynamicMaterial() const;

private:
	void ApplyGravityLineSettings();
	void EnsureCelestialBodyDynamicMeshVisuals();
	bool CopyStaticMeshToCelestialBodyDynamicMesh();
	USRCelestialBodyRegistrySubsystem* FindCelestialRegistry() const;
	bool IsStellarBody() const;
	void LogMissingDataErrorOnce(const TCHAR* Context) const;

	bool bHasAppliedData = false;

	mutable bool bHasLoggedMissingDataError = false;

};
