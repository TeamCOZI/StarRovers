#pragma once

#include "CoreMinimal.h"
#include "SRPlanetTerrainTypes.generated.h"

UENUM(BlueprintType)
enum class ESRPlanetTerrainProfile : uint8
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
struct STARROVERS_API FSRPlanetTerrainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "TerrainProfile"))
	ESRPlanetTerrainProfile TerrainProfile = ESRPlanetTerrainProfile::EarthLike;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "bUseProceduralTerrain"))
	bool bUseProceduralTerrain = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "TerrainSeed"))
	int32 TerrainSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "TerrainHeight", ClampMin = "0.0"))
	float TerrainHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "SeaLevel", ClampMin = "-1.0", ClampMax = "1.0"))
	float SeaLevel = -0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "ContinentFrequency", ClampMin = "0.01"))
	float ContinentFrequency = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "MountainFrequency", ClampMin = "0.01"))
	float MountainFrequency = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "DetailFrequency", ClampMin = "0.01"))
	float DetailFrequency = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "ValleyStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float ValleyStrength = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "MountainSharpness", ClampMin = "0.5", ClampMax = "4.0"))
	float MountainSharpness = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "DomainWarpStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float DomainWarpStrength = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "RiverStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float RiverStrength = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "LakeStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float LakeStrength = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "MicroDetailStrength", ClampMin = "0.0", ClampMax = "1.0"))
	float MicroDetailStrength = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "MoistureFrequency", ClampMin = "0.01"))
	float MoistureFrequency = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "TemperatureNoiseFrequency", ClampMin = "0.01"))
	float TemperatureNoiseFrequency = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "TerrainOctaves", ClampMin = "1"))
	int32 TerrainOctaves = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Terrain", meta = (DisplayName = "TerrainPersistence", ClampMin = "0.0", ClampMax = "1.0"))
	float TerrainPersistence = 0.5f;
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

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "CraterMask"))
	float CraterMask = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "PlateBeltMask"))
	float PlateBeltMask = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "Biome"))
	ESRPlanetBiome Biome = ESRPlanetBiome::Plains;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "SurfaceColor"))
	FLinearColor SurfaceColor = FLinearColor(0.42f, 0.42f, 0.38f, 1.0f);
};
