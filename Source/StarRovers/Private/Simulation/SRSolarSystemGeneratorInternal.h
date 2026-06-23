#pragma once

#include "Simulation/SRSolarSystemGenerator.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Celestial/SRMoonDataAsset.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Celestial/SRStarDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Misc/Guid.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"

namespace StarRoversSolarSystemGeneratorInternal
{
	inline double SRSolarNowSeconds()
	{
		return FPlatformTime::Seconds();
	}

	inline double SRSolarElapsedMilliseconds(double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}

	struct FSROrbitInfo
	{
		float OrbitingBodyExtent = 0.0f;
		float DesiredOrbitRadius = 0.0f;
	};

	struct FSRGenerationStageTiming
	{
		FString Name;
		double Milliseconds = 0.0;
	};

	inline int32 CreateRuntimeRandomGenerationSeed()
	{
		const FGuid Guid = FGuid::NewGuid();
		uint32 Hash = HashCombine(Guid.A, Guid.B);
		Hash = HashCombine(Hash, Guid.C);
		Hash = HashCombine(Hash, Guid.D);
		Hash = HashCombine(Hash, ::GetTypeHash(FPlatformTime::Cycles64()));
		Hash = HashCombine(Hash, ::GetTypeHash(FMath::Rand()));
		return static_cast<int32>((Hash % static_cast<uint32>(TNumericLimits<int32>::Max() - 1)) + 1);
	}

	inline void ApplyResolvedGenerationSeed(FSRCelestialBodyData& InOutData, int32 ResolvedSeed)
	{
		InOutData.GenerationSeed = ResolvedSeed;
		InOutData.DynamicMeshGeneration.GenerationSeed = ResolvedSeed;
	}

	inline bool ShouldRandomizeBodyGenerationSeed(const FSRCelestialBodyData& BodyData)
	{
		return BodyData.bRandomizeGenerationSeedEachRun
			|| BodyData.DynamicMeshGeneration.bRandomizeGenerationSeedEachRun;
	}

	inline void LogGeneratorMissingData(const UObject* SourceObject, const TCHAR* FieldName)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Solar system generation requires %s on '%s'."),
			FieldName ? FieldName : TEXT("<UnknownField>"),
			IsValid(SourceObject) ? *SourceObject->GetName() : TEXT("<InvalidObject>"));
	}

	inline bool TryComputeScaledBodyRadiusFromCelestialBodyRequest(const FSRCelestialBodyGenerateRequest& CelestialBodyRequest, float& OutRadius)
	{
		OutRadius = 0.0f;
		const UStaticMesh* MeshAsset = CelestialBodyRequest.BodyData.StaticMesh.Get();
		const float BodyScale = FMath::Max(0.0f, CelestialBodyRequest.BodyData.Scale);
		if (IsValid(MeshAsset))
		{
			OutRadius = FMath::Max(0.0f, MeshAsset->GetBounds().SphereRadius * BodyScale);
			return true;
		}

		const USRDynamicMeshBaseDataAsset* DynamicMeshBase = CelestialBodyRequest.BodyData.DynamicMeshBaseDataAsset.Get();
		if (IsValid(DynamicMeshBase))
		{
			OutRadius = FMath::Max(0.0f, DynamicMeshBase->GetSafeBaseRadius() * BodyScale);
			return true;
		}

		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires StaticMesh or DynamicMeshBaseDataAsset for '%s'."), *CelestialBodyRequest.BodyData.VariableName.ToString());
		return false;
	}

	inline float ComputeScaledBodyRadius(const ASRCelestialBody* CelestialBody)
	{
		if (!IsValid(CelestialBody))
		{
			return 0.0f;
		}

		const FSRCelestialBodyData BodyData = CelestialBody->GetData();
		if (IsValid(BodyData.StaticMesh.Get()))
		{
			return BodyData.StaticMesh->GetBounds().SphereRadius * FMath::Max(0.0f, BodyData.Scale);
		}
		return IsValid(BodyData.DynamicMeshBaseDataAsset.Get())
			? BodyData.DynamicMeshBaseDataAsset->GetSafeBaseRadius() * FMath::Max(0.0f, BodyData.Scale)
			: 0.0f;
	}

	inline float ComputeGravityRadiusFromCelestialBodyRequest(const FSRCelestialBodyGenerateRequest& CelestialBodyRequest)
	{
		return FMath::Max(0.0f, CelestialBodyRequest.BodyData.Mass)
			* FMath::Max(0.0f, CelestialBodyRequest.BodyData.GravityRadiusRatio);
	}

	inline bool TryGetRequiredVariableName(const UObject* DataAsset, const FText& VariableName, FText& OutVariableName)
	{
		OutVariableName = FText::GetEmpty();
		if (!IsValid(DataAsset))
		{
			LogGeneratorMissingData(DataAsset, TEXT("DataAsset"));
			return false;
		}

		if (VariableName.IsEmpty())
		{
			LogGeneratorMissingData(DataAsset, TEXT("VariableName"));
			return false;
		}

		OutVariableName = VariableName;
		return true;
	}

	inline bool TryBuildDataFromDataAsset(
		const TSubclassOf<ASRCelestialBody>& BodyClass,
		const FSRCelestialBodyData& DataAssetData,
		FSRCelestialBodyData& OutData)
	{
		if (!BodyClass || BodyClass == ASRCelestialBody::StaticClass())
		{
			UE_LOG(LogTemp, Error, TEXT("Solar system generation requires a concrete celestial body class."));
			return false;
		}

		OutData = DataAssetData;
		OutData.Scale = FMath::Max(0.0f, DataAssetData.Scale);
		OutData.Mass = FMath::Max(0.0f, DataAssetData.Mass);
		OutData.GravityRatio = FMath::Max(0.0f, DataAssetData.GravityRatio);
		OutData.GravityRadiusRatio = FMath::Max(0.0f, DataAssetData.GravityRadiusRatio);
		OutData.OceanScaleMultiplier = DataAssetData.OceanScaleMultiplier;
		OutData.AtmosphereScaleMultiplier = DataAssetData.AtmosphereScaleMultiplier;
		OutData.SurfaceGridHeightOffset = DataAssetData.SurfaceGridHeightOffset;
		OutData.OrbitPeriod = FMath::Max(0.0f, DataAssetData.OrbitPeriod);
		OutData.StarPointLightIntensity = DataAssetData.StarPointLightIntensity;
		OutData.StarPointLightColor = DataAssetData.StarPointLightColor;
		OutData.InitialStoredStellarFuel = FMath::Max(0.0, DataAssetData.InitialStoredStellarFuel);
		OutData.RequiredStellarFuelPerCycle = FMath::Max(0.0, DataAssetData.RequiredStellarFuelPerCycle);
		OutData.StellarFuelRequirementGrowthPerCycle = FMath::Max(0.0, DataAssetData.StellarFuelRequirementGrowthPerCycle);
		OutData.InitialRedGiantPressure = FMath::Max(0.0, DataAssetData.InitialRedGiantPressure);
		OutData.RedGiantPressurePerMissingFuel = FMath::Max(0.0, DataAssetData.RedGiantPressurePerMissingFuel);
		return true;
	}

	inline void ApplyClassDefaultRuntimeVisualSettings(
		const TSubclassOf<ASRCelestialBody>& BodyClass,
		FSRCelestialBodyData& InOutData)
	{
		const ASRCelestialBody* ClassDefaultBody = BodyClass
			? Cast<ASRCelestialBody>(BodyClass->GetDefaultObject())
			: nullptr;
		if (!IsValid(ClassDefaultBody))
		{
			return;
		}

		const FSRCelestialBodyData ClassDefaultData = ClassDefaultBody->GetData();
		InOutData.bRandomizeGenerationSeedEachRun =
			InOutData.bRandomizeGenerationSeedEachRun
			|| ClassDefaultData.bRandomizeGenerationSeedEachRun
			|| ClassDefaultData.DynamicMeshGeneration.bRandomizeGenerationSeedEachRun;
		InOutData.DynamicMeshGeneration.bRandomizeGenerationSeedEachRun =
			InOutData.DynamicMeshGeneration.bRandomizeGenerationSeedEachRun
			|| ClassDefaultData.DynamicMeshGeneration.bRandomizeGenerationSeedEachRun;
		InOutData.FocusZoomMultiplier = ClassDefaultData.FocusZoomMultiplier;
		InOutData.GridLineThickness = ClassDefaultData.GridLineThickness;
		InOutData.GridLineColor = ClassDefaultData.GridLineColor;
		InOutData.GridLineOpacity = ClassDefaultData.GridLineOpacity;
		InOutData.HoveredCellColor = ClassDefaultData.HoveredCellColor;
		InOutData.SelectedCellColor = ClassDefaultData.SelectedCellColor;
		InOutData.OccupiedCellColor = ClassDefaultData.OccupiedCellColor;
		InOutData.ShowOrbitLine = ClassDefaultData.ShowOrbitLine;
		InOutData.OrbitLineColor = ClassDefaultData.OrbitLineColor;
		InOutData.OrbitLineOpacity = ClassDefaultData.OrbitLineOpacity;
		InOutData.OrbitLineSegments = ClassDefaultData.OrbitLineSegments;
		InOutData.OrbitLineThickness = ClassDefaultData.OrbitLineThickness;
		InOutData.ShowGravityLine = ClassDefaultData.ShowGravityLine;
		InOutData.GravityLineColor = ClassDefaultData.GravityLineColor;
		InOutData.GravityLineOpacity = ClassDefaultData.GravityLineOpacity;
		InOutData.GravityLineSegments = ClassDefaultData.GravityLineSegments;
		InOutData.GravityLineThickness = ClassDefaultData.GravityLineThickness;
		InOutData.ShowRotationAxisLine = ClassDefaultData.ShowRotationAxisLine;
		InOutData.RotationAxisLineColor = ClassDefaultData.RotationAxisLineColor;
		InOutData.RotationAxisLineOpacity = ClassDefaultData.RotationAxisLineOpacity;
		InOutData.RotationAxisLineThickness = ClassDefaultData.RotationAxisLineThickness;
		InOutData.RotationAxisLineLengthMultiplier = ClassDefaultData.RotationAxisLineLengthMultiplier;
	}

	inline TSubclassOf<ASRCelestialBody> ValidateRuntimeCelestialClass(
		const TSubclassOf<ASRCelestialBody>& ConfiguredClass,
		const TCHAR* ClassPurpose)
	{
		if (ConfiguredClass && ConfiguredClass != ASRCelestialBody::StaticClass())
		{
			return ConfiguredClass;
		}

		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires a configured %s class."), ClassPurpose ? ClassPurpose : TEXT("celestial body"));
		return nullptr;
	}

	template <typename TDataAsset>
	bool TryBuildRequestFromDataAsset(
		const TSubclassOf<ASRCelestialBody>& BodyClass,
		const TDataAsset* DataAsset,
		FSRCelestialBodyGenerateRequest& OutRequest)
	{
		OutRequest = FSRCelestialBodyGenerateRequest();
		if (!IsValid(DataAsset))
		{
			LogGeneratorMissingData(DataAsset, TEXT("DataAsset"));
			return false;
		}

		OutRequest.BodyClass = BodyClass;
		if (!TryBuildDataFromDataAsset(BodyClass, DataAsset->BuildData(), OutRequest.BodyData))
		{
			return false;
		}
		ApplyClassDefaultRuntimeVisualSettings(BodyClass, OutRequest.BodyData);

		const bool bRequiresDynamicMeshBase =
			OutRequest.BodyData.BodyCategory == ESRCelestialBodyCategory::Planet
			|| OutRequest.BodyData.BodyCategory == ESRCelestialBodyCategory::Moon;
		if (bRequiresDynamicMeshBase && !IsValid(OutRequest.BodyData.DynamicMeshBaseDataAsset.Get()))
		{
			LogGeneratorMissingData(DataAsset, TEXT("DynamicMeshBaseDataAsset"));
			return false;
		}
		if (!bRequiresDynamicMeshBase && !IsValid(OutRequest.BodyData.StaticMesh))
		{
			LogGeneratorMissingData(DataAsset, TEXT("StaticMesh"));
			return false;
		}
		if (!IsValid(OutRequest.BodyData.Material))
		{
			LogGeneratorMissingData(DataAsset, TEXT("Material"));
			return false;
		}
		if ((OutRequest.BodyData.BodyCategory == ESRCelestialBodyCategory::Planet
				|| OutRequest.BodyData.BodyCategory == ESRCelestialBodyCategory::Moon)
			&& !IsValid(OutRequest.BodyData.TerrainProfileDataAsset.Get()))
		{
			LogGeneratorMissingData(DataAsset, TEXT("TerrainProfileDataAsset"));
			return false;
		}
		if (IsValid(OutRequest.BodyData.TerrainProfileDataAsset.Get())
			&& OutRequest.BodyData.TerrainProfileDataAsset->GetAllowedBiomeDataAssets().IsEmpty())
		{
			LogGeneratorMissingData(OutRequest.BodyData.TerrainProfileDataAsset.Get(), TEXT("Biomes"));
			return false;
		}
		if (OutRequest.BodyData.bHasOcean)
		{
			if (!IsValid(OutRequest.BodyData.OceanMesh.Get()))
			{
				LogGeneratorMissingData(DataAsset, TEXT("OceanMesh"));
				return false;
			}
			if (!IsValid(OutRequest.BodyData.OceanMaterial.Get()))
			{
				LogGeneratorMissingData(DataAsset, TEXT("OceanMaterial"));
				return false;
			}
		}
		if (OutRequest.BodyData.bHasAtmosphere)
		{
			if (!IsValid(OutRequest.BodyData.AtmosphereMesh.Get()))
			{
				LogGeneratorMissingData(DataAsset, TEXT("AtmosphereMesh"));
				return false;
			}
			if (!IsValid(OutRequest.BodyData.AtmosphereMaterial.Get()))
			{
				LogGeneratorMissingData(DataAsset, TEXT("AtmosphereMaterial"));
				return false;
			}
		}

		FText VariableName;
		if (!TryGetRequiredVariableName(DataAsset, DataAsset->VariableName, VariableName))
		{
			return false;
		}
		OutRequest.BodyData.VariableName = VariableName;
		return true;
	}

	template <typename TDataAsset>
	const TDataAsset* ResolveRandomDataAssetStrict(const TArray<TObjectPtr<TDataAsset>>& DataAssets, FRandomStream& RandomStream, const TCHAR* AssetTypeName)
	{
		TArray<const TDataAsset*> ValidAssets;
		for (const TDataAsset* Asset : DataAssets)
		{
			if (IsValid(Asset))
			{
				ValidAssets.Add(Asset);
			}
		}

		if (ValidAssets.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("ASRSolarSystemGenerator requires at least one valid %s data asset."), AssetTypeName ? AssetTypeName : TEXT("celestial body"));
			return nullptr;
		}

		return ValidAssets[RandomStream.RandRange(0, ValidAssets.Num() - 1)];
	}
}
