#pragma once

#include "CoreMinimal.h"
#include "SRPlanetTerrainTypes.generated.h"

class UMaterialInterface;

UENUM(BlueprintType)
enum class ESRPlanetBiomeProfile : uint8
{
	None = 0,
	EarthLike = 1,
	GasGiant = 2,
	RockyMoon = 3,
	MarsLike = 4,
	IceWorld = 5,
	Volcanic = 6,
	OceanWorld = 7
};

UENUM(BlueprintType)
enum class ESRPlanetBiome : uint8
{
	Ocean,
	Coast,
	Plains,
	Forest,
	Desert,
	Mountain,
	Snow,
	Ice
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRDynamicMeshGeneration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "bDynamicMeshGeneration"))
	bool bDynamicMeshGeneration = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "bMinecraft"))
	bool bMinecraft = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "BiomeProfile"))
	ESRPlanetBiomeProfile BiomeProfile = ESRPlanetBiomeProfile::EarthLike;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "BiomeMaterials"))
	TMap<ESRPlanetBiome, TObjectPtr<UMaterialInterface>> BiomeMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "GenerationSeed"))
	int32 GenerationSeed = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "DynamicMeshHeight", ClampMin = "0.0"))
	float DynamicMeshHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "OceanThreshold", ClampMin = "-1.0", ClampMax = "1.0"))
	float OceanThreshold = -0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "AtmosphereThreshold", ClampMin = "0.01"))
	float AtmosphereThreshold = 1.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "ContinentFrequency", ClampMin = "0.01"))
	float ContinentFrequency = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "MountainFrequency", ClampMin = "0.01"))
	float MountainFrequency = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "MountainStrength", ClampMin = "0.5", ClampMax = "4.0"))
	float MountainStrength = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "ValleyStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float ValleyStrength = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "RiverStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float RiverStrength = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "LakeStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float LakeStrength = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "TemperatureFrequency", ClampMin = "0.01"))
	float TemperatureFrequency = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "MoistureFrequency", ClampMin = "0.01"))
	float MoistureFrequency = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "DetailFrequency", ClampMin = "0.01"))
	float DetailFrequency = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "DetailStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float DetailStrength = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "NoiseStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float NoiseStrength = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "NoiseOctaves", ClampMin = "1"))
	int32 NoiseOctaves = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "NoisePersistence", ClampMin = "0.0", ClampMax = "1.0"))
	float NoisePersistence = 0.5f;
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

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "SurfaceColor"))
	FLinearColor SurfaceColor = FLinearColor(0.42f, 0.42f, 0.38f, 1.0f);
};
