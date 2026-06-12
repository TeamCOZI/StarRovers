#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetBiomeTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRPlanetTerrainTypes.generated.h"

class UMaterialInterface;
class USRPlanetBiomeDataAsset;

struct STARROVERS_API FSRPlanetBiomeGenerationSnapshot
{
	FName BiomeId = FName(TEXT("Plains"));
	ESRBiomeWaterRole WaterRole = ESRBiomeWaterRole::None;
	TArray<FSRBiomePlacementRule> PlacementRules;
	float SpawnWeight = 1.0f;
	float RegionSize = 0.35f;
	int32 Priority = 0;
	bool bCanOverrideLowerPriorityBiomes = false;
	float OverrideMinScore = 0.65f;
};

struct STARROVERS_API FSRCompiledPlanetBiomeGenerationSnapshot
{
	FName BiomeId = FName(TEXT("Plains"));
	ESRBiomeWaterRole WaterRole = ESRBiomeWaterRole::None;
	TArray<FSRBiomePlacementRule> PlacementRules;
	float Weight = 1.0f;
	int32 Priority = 0;
	bool bCanOverrideLowerPriorityBiomes = false;
	float OverrideMinScore = 0.65f;
	float TargetTemperature = 0.5f;
	float TargetMoisture = 0.5f;
	float TargetHeight = 0.5f;
	float TargetContinentalness = 0.5f;
	float AnchorThreshold = 0.0f;
	FVector AnchorDirections[2] = { FVector::UpVector, FVector::ForwardVector };
	float PatchFrequency = 1.5f;
	int32 PatchSeed = 0;
	FVector PatchSeedOffset = FVector::ZeroVector;
	FLinearColor BaseLandColor = FLinearColor(0.28f, 0.46f, 0.23f, 1.0f);
	ESRPlanetBiome RuntimeBiome = ESRPlanetBiome::Plains;
};

struct STARROVERS_API FSRCompiledTerrainNoiseDescriptor
{
	FVector SeedOffset = FVector::ZeroVector;
	float Frequency = 1.0f;
	int32 Octaves = 1;
	float Persistence = 0.5f;
};

struct STARROVERS_API FSRDynamicMeshGenerationSnapshot
{
	bool bDynamicMeshGeneration = true;
	bool bMinecraft = false;
	int32 GenerationSeed = 1000;
	bool bRandomizeGenerationSeedEachRun = false;
	float DynamicMeshHeight = 120.0f;
	float OceanThreshold = -0.05f;
	float ContinentFrequency = 1.15f;
	float MountainFrequency = 7.0f;
	float MountainStrength = 1.8f;
	float ValleyStrength = 0.35f;
	float RiverStrength = 0.28f;
	float LakeStrength = 0.18f;
	float TemperatureFrequency = 1.75f;
	float MoistureFrequency = 2.5f;
	float DetailFrequency = 18.0f;
	float DetailStrength = 0.45f;
	float NoiseStrength = 0.18f;
	int32 NoiseOctaves = 5;
	float NoisePersistence = 0.5f;
	TArray<FSRPlanetBiomeGenerationSnapshot> Biomes;
	TArray<FSRCompiledPlanetBiomeGenerationSnapshot> CompiledBiomes;
	bool bUsesRareRegionPlacementMetric = false;
	int32 SafeNoiseOctaves = 5;
	float SafeNoisePersistence = 0.5f;
	float SafeDynamicMeshHeight = 120.0f;
	float InvSafeDynamicMeshHeight = 1.0f / 120.0f;
	float ClimateWarpStrength = 0.099f;
	float TerrainWarpStrength = 0.18f;
	float MountainHeightStrengthScale = 1.0f;
	float ClampedValleyStrength = 0.35f;
	float ClampedDetailStrength = 0.45f;
	float ClampedRiverStrength = 0.28f;
	float ClampedLakeStrength = 0.18f;
	FSRCompiledTerrainNoiseDescriptor ClimateWarpNoise[3];
	FSRCompiledTerrainNoiseDescriptor TerrainWarpNoise[3];
	FSRCompiledTerrainNoiseDescriptor ContinentalnessNoise;
	FSRCompiledTerrainNoiseDescriptor ErosionNoise;
	FSRCompiledTerrainNoiseDescriptor WeirdnessNoise;
	FSRCompiledTerrainNoiseDescriptor RidgesNoise;
	FSRCompiledTerrainNoiseDescriptor DetailNoise;
	FSRCompiledTerrainNoiseDescriptor TemperatureNoise;
	FSRCompiledTerrainNoiseDescriptor HumidityNoise;
	FSRCompiledTerrainNoiseDescriptor RiverNoise[2];
	FSRCompiledTerrainNoiseDescriptor LakeNoise[2];
	FSRCompiledTerrainNoiseDescriptor RareRegionNoise;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRBiomeMaterialEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "BiomeId", ToolTip = "이 material entry가 연결되는 BiomeId입니다. Profile의 Biome DA에서 자동으로 가져옵니다."))
	FName BiomeId = FName(TEXT("Plains"));

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "BiomeDataAsset", ToolTip = "이 material entry가 참조하는 Biome DA입니다. Profile의 Biome 목록에 맞춰 자동 정규화됩니다."))
	TObjectPtr<USRPlanetBiomeDataAsset> BiomeDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "Material", ToolTip = "해당 BiomeId가 선택된 surface cell에 사용할 머티리얼입니다. 비워두면 기본 material slot fallback을 사용합니다."))
	TObjectPtr<UMaterialInterface> Material = nullptr;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRBiomeSampleContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "LocalUnitDirection"))
	FVector LocalUnitDirection = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "Face"))
	ESRCubeSphereFace Face = ESRCubeSphereFace::PositiveZ;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "CellX"))
	int32 CellX = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "CellY"))
	int32 CellY = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "FaceResolution"))
	int32 FaceResolution = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "FaceUV"))
	FVector2D FaceUV = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRDynamicMeshGeneration
{
	GENERATED_BODY()

	FSRDynamicMeshGeneration();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "bDynamicMeshGeneration", ToolTip = "절차 지형 생성을 사용할지 정합니다. 끄면 Static Mesh 형태를 그대로 사용하고 Dynamic Mesh 높이 변형을 적용하지 않습니다."))
	bool bDynamicMeshGeneration = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "bMinecraft", ToolTip = "Quantizes terrain height to block steps. When enabled, the step matches one regular cube-face cell edge length; when disabled, continuous terrain height is used."))
	bool bMinecraft = false;

	UPROPERTY()
	TArray<TObjectPtr<USRPlanetBiomeDataAsset>> BiomeDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "BiomeMaterials", EditFixedSize, ToolTip = "Profile에 포함된 Biome DA별 머티리얼 슬롯입니다. Profile의 Biome 목록 순서에 맞춰 자동 정규화되며, 최종 표면 표현은 여기 지정한 머티리얼을 우선 사용합니다."))
	TArray<FSRBiomeMaterialEntry> BiomeMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "bRandomizeGenerationSeedEachRun", ToolTip = "When enabled, runtime generation replaces GenerationSeed with a fresh random seed each level run."))
	bool bRandomizeGenerationSeedEachRun = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "GenerationSeed", ToolTip = "지형, 기후, biome 패치 배치를 재현하기 위한 시드입니다. 같은 설정과 같은 Seed는 같은 결과를 만듭니다. 값을 바꾸면 전체 패턴이 달라집니다."))
	int32 GenerationSeed = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "DynamicMeshHeight", ClampMin = "0.0", ToolTip = "지형 높낮이의 전체 스케일입니다. 값을 키우면 산, 골짜기, 해저의 높이 차가 커지고, 줄이면 표면이 더 평탄해집니다."))
	float DynamicMeshHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "OceanThreshold", ClampMin = "-1.0", ClampMax = "1.0", ToolTip = "바다/육지 경계를 정하는 대륙성 보정값입니다. 값을 키우면 바다 판정이 늘고 육지가 줄어들며, 값을 낮추면 육지가 늘어납니다."))
	float OceanThreshold = -0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "AtmosphereThreshold", ClampMin = "0.01", ToolTip = "대기 구체 크기를 body radius 대비 배율로 정합니다. 값을 키우면 대기 mesh가 행성보다 더 크게 감싸고, 줄이면 표면에 가까워집니다."))
	float AtmosphereThreshold = 1.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "ContinentFrequency", ClampMin = "0.01", ToolTip = "대륙 노이즈의 샘플 빈도입니다. 값을 키우면 대륙과 해안선이 더 잘게 쪼개지고, 줄이면 큰 대륙 덩어리가 생깁니다."))
	float ContinentFrequency = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "MountainFrequency", ClampMin = "0.01", ToolTip = "산맥/봉우리 노이즈의 샘플 빈도입니다. 값을 키우면 산 패턴이 더 촘촘하고 잦아지며, 줄이면 큰 산맥 위주가 됩니다."))
	float MountainFrequency = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "MountainStrength", ClampMin = "0.5", ClampMax = "4.0", ToolTip = "산 높이 반영 강도입니다. 값을 키우면 산이 더 높고 뚜렷해지며, 줄이면 산악 지형이 낮고 완만해집니다."))
	float MountainStrength = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "ValleyStrength", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "골짜기를 파내는 강도입니다. 값을 키우면 골짜기가 더 깊고 뚜렷해지며, 줄이면 골짜기 영향이 약해집니다."))
	float ValleyStrength = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "RiverStrength", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "강 마스크와 강 carve 영향입니다. 값을 키우면 강 후보가 강해지고 강 주변 지형 영향이 커지며, 줄이면 강이 드물거나 약해집니다."))
	float RiverStrength = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "LakeStrength", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "호수/분지 마스크 영향입니다. 값을 키우면 호수 후보가 강하고 넓어지며, 줄이면 호수 후보가 줄어듭니다."))
	float LakeStrength = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "TemperatureFrequency", ClampMin = "0.01", ToolTip = "기온 노이즈의 샘플 빈도입니다. 값을 키우면 뜨겁고 추운 구역이 더 자잘하게 반복되고, 줄이면 넓은 기후대가 생깁니다."))
	float TemperatureFrequency = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "MoistureFrequency", ClampMin = "0.01", ToolTip = "습도 노이즈의 샘플 빈도입니다. 값을 키우면 습윤/건조 구역이 더 자잘해지고, 줄이면 넓은 습도대가 생깁니다."))
	float MoistureFrequency = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "DetailFrequency", ClampMin = "0.01", ToolTip = "최종 표면에 더하는 미세 기복 노이즈의 빈도입니다. 값을 키우면 작은 요철이 더 자주 나타나고, 줄이면 큰 완만한 변화만 남습니다."))
	float DetailFrequency = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "DetailStrength", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "미세 기복이 최종 높이에 반영되는 강도입니다. 값을 키우면 표면이 더 거칠어지고, 줄이면 부드러워집니다."))
	float DetailStrength = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "NoiseStrength", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "노이즈 샘플 위치를 비트는 domain warp 강도입니다. 값을 키우면 대륙선, 산맥, 기후 패턴이 더 불규칙하게 휘고, 줄이면 패턴이 단순해집니다."))
	float NoiseStrength = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "NoiseOctaves", ClampMin = "1", ToolTip = "Fractal noise를 몇 겹 합성할지 정합니다. 값을 키우면 더 복잡하고 세밀한 패턴이 생기지만 계산 비용도 늘어납니다."))
	int32 NoiseOctaves = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "NoisePersistence", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "다음 octave로 넘어갈 때 진폭을 얼마나 유지할지 정합니다. 값을 키우면 작은 패턴의 영향이 강해져 거칠고 복잡해지며, 줄이면 큰 형태 위주로 부드러워집니다."))
	float NoisePersistence = 0.5f;

	void NormalizeBiomeMaterials(const TArray<TObjectPtr<USRPlanetBiomeDataAsset>>& AllowedBiomeDataAssets);
	UMaterialInterface* GetBiomeMaterial(FName BiomeId) const;
	int32 GetBiomeMaterialSlotIndex(FName BiomeId) const;
	FSRDynamicMeshGenerationSnapshot MakeThreadSafeSnapshot() const;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetTerrainSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "HeightOffset"))
	float HeightOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "Continent"))
	float Continent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "MountainMask"))
	float MountainMask = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "Temperature"))
	float Temperature = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "Moisture"))
	float Moisture = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "RiverMask"))
	float RiverMask = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "LakeMask"))
	float LakeMask = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "PlateBeltMask"))
	float PlateBeltMask = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "Biome"))
	ESRPlanetBiome Biome = ESRPlanetBiome::Plains;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "BiomeId"))
	FName BiomeId = FName(TEXT("Plains"));

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "WaterRole"))
	ESRBiomeWaterRole WaterRole = ESRBiomeWaterRole::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "SurfaceColor"))
	FLinearColor SurfaceColor = FLinearColor(0.42f, 0.42f, 0.38f, 1.0f);
};
