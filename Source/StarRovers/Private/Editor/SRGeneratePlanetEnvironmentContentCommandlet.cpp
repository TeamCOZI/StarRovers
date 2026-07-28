#include "Editor/SRGeneratePlanetEnvironmentContentCommandlet.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Celestial/SRPlanetShapeDataAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Simulation/SRPlanetEnvironmentSelection.h"
#include "Simulation/SRSolarSystemGenerator.h"
#include "Surface/SRPlanetBiomeDataAsset.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr TCHAR GeneratorBlueprintPath[] =
		TEXT("/Game/StarRovers/Generation/Blueprints/BP_SolarSystemGenerator.BP_SolarSystemGenerator");
	constexpr TCHAR SolarSystemMapPackagePath[] = TEXT("/Game/Levels/SolarSystem");
	constexpr TCHAR TemperatePlanetPackage[] =
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_Temperate");
	constexpr TCHAR LavaOceanPlanetPackage[] =
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_LavaOcean");
	constexpr TCHAR BadlandsPlanetPackage[] =
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_BadLands");
	constexpr TCHAR FrozenOceanPlanetPackage[] =
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_FrozenOcean");
	constexpr TCHAR AridDesertPlanetPackage[] =
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_AridDesert");
	constexpr TCHAR ToxicWetlandPlanetPackage[] =
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_ToxicWetland");
	constexpr TCHAR EarthProfilePath[] =
		TEXT("/Game/StarRovers/Surface/DataAssets/TerrainProfiles/DA_Profile_Earth.DA_Profile_Earth");
	constexpr TCHAR BiomeRoot[] = TEXT("/Game/StarRovers/Surface/DataAssets/Biomes/PlanetEnvironments");
	constexpr TCHAR ProfileRoot[] = TEXT("/Game/StarRovers/Surface/DataAssets/TerrainProfiles");

	const TArray<FString> PlanetObjectPaths = {
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_Temperate.DA_Planet_Temperate"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_LavaOcean.DA_Planet_LavaOcean"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_BadLands.DA_Planet_BadLands"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_FrozenOcean.DA_Planet_FrozenOcean"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_AridDesert.DA_Planet_AridDesert"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_ToxicWetland.DA_Planet_ToxicWetland"),
	};

	TArray<FName> MakeRequiredSystemResourceRuleIds()
	{
		return {
			TEXT("ResourceV2.HeliosIron"),
			TEXT("ResourceV2.EchoQuartz"),
			TEXT("ResourceV2.VerdantSpore"),
			TEXT("ResourceV2.AuroraPlasma"),
			TEXT("ResourceV2.NullPearl"),
			TEXT("ResourceV2.CommonOre"),
			TEXT("ResourceV2.BiomassFeedstock"),
		};
	}

	FString MakeObjectPath(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
	}

	template <typename TObjectType>
	TObjectType* LoadOrCreateAsset(const FString& PackageName, bool& bOutCreated)
	{
		bOutCreated = false;
		if (PackageName.IsEmpty())
		{
			return nullptr;
		}
		if (FPackageName::DoesPackageExist(PackageName))
		{
			if (TObjectType* Existing = LoadObject<TObjectType>(nullptr, *MakeObjectPath(PackageName)))
			{
				return Existing;
			}
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!IsValid(Package))
		{
			return nullptr;
		}
		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		TObjectType* Asset = NewObject<TObjectType>(
			Package,
			AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		if (IsValid(Asset))
		{
			FAssetRegistryModule::AssetCreated(Asset);
			bOutCreated = true;
		}
		return Asset;
	}

	bool SaveAsset(UObject* Asset)
	{
		if (!IsValid(Asset))
		{
			return false;
		}
		Asset->MarkPackageDirty();
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
	}

	FString GetObjectPath(const UObject* Object)
	{
		return IsValid(Object) ? Object->GetPathName() : TEXT("None");
	}

	void LogGeneratorProperty(const UObject& GeneratorObject, const FName PropertyName)
	{
		const FProperty* Property = FindFProperty<FProperty>(GeneratorObject.GetClass(), PropertyName);
		if (!Property)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Planet environment inspection could not find %s on %s."),
				*PropertyName.ToString(),
				*GeneratorObject.GetPathName());
			return;
		}

		FString ExportedValue;
		Property->ExportText_InContainer(
			0,
			ExportedValue,
			&GeneratorObject,
			&GeneratorObject,
			const_cast<UObject*>(&GeneratorObject),
			PPF_None);
		UE_LOG(LogTemp, Display,
			TEXT("Generator %s | %s=%s"),
			*GeneratorObject.GetPathName(),
			*PropertyName.ToString(),
			*ExportedValue);
	}

	void LogGenerator(const UObject* GeneratorObject)
	{
		if (!IsValid(GeneratorObject))
		{
			return;
		}

		LogGeneratorProperty(*GeneratorObject, TEXT("GenerationSeed"));
		LogGeneratorProperty(*GeneratorObject, TEXT("bRandomizeGenerationSeedEachRun"));
		LogGeneratorProperty(*GeneratorObject, TEXT("MinPlanet"));
		LogGeneratorProperty(*GeneratorObject, TEXT("MaxPlanet"));
		LogGeneratorProperty(*GeneratorObject, TEXT("MinMoon"));
		LogGeneratorProperty(*GeneratorObject, TEXT("MaxMoon"));
		LogGeneratorProperty(*GeneratorObject, TEXT("MinimumUniquePlanetTypes"));
		LogGeneratorProperty(*GeneratorObject, TEXT("RequiredSystemResourceRuleIds"));
		LogGeneratorProperty(*GeneratorObject, TEXT("PlanetDataAssets"));
	}

	void LogProfile(const USRPlanetTerrainProfileDataAsset* Profile)
	{
		if (!IsValid(Profile))
		{
			return;
		}

		TArray<FString> BiomeSummaries;
		for (const FSRPlanetProfileBiomeEntry& Entry : Profile->Biomes)
		{
			const USRPlanetBiomeDataAsset* Biome = Entry.BiomeDataAsset.Get();
			BiomeSummaries.Add(FString::Printf(
				TEXT("%s[%s,water=%d,rules=%d,weight=%.2f,region=%.2f]"),
				*GetObjectPath(Biome),
				IsValid(Biome) ? *Biome->BiomeId.ToString() : TEXT("None"),
				IsValid(Biome) ? static_cast<int32>(Biome->WaterRole) : INDEX_NONE,
				IsValid(Biome) ? Biome->PlacementRules.Num() : 0,
				IsValid(Biome) ? Biome->SpawnWeight : 0.0f,
				IsValid(Biome) ? Biome->RegionSize : 0.0f));
		}
		UE_LOG(LogTemp, Display,
			TEXT("TerrainProfile %s | Id=%s | Biomes=%s | NaturalRules=%d"),
			*Profile->GetPathName(),
			*Profile->ProfileId.ToString(),
			*FString::Join(BiomeSummaries, TEXT("; ")),
			Profile->ProfileNaturalStructureSpawnRules.Num());
	}

	void LogPlanet(const USRPlanetDataAsset* Planet)
	{
		if (!IsValid(Planet))
		{
			return;
		}

		const FSRDynamicMeshGeneration& Terrain = Planet->DynamicMeshGeneration;
		TArray<FString> ResourceDistributionSummaries;
		TSet<FName> EnabledResourceRuleIds;
		FSRPlanetEnvironmentSelector::GetEnabledResourceRuleIds(
			Planet,
			EnabledResourceRuleIds);
		for (const FName RuleId : EnabledResourceRuleIds)
		{
			const FSRNaturalStructureSpawnRuleOverride* Override =
				Planet->ProfileNaturalStructureSpawnRuleOverrides.FindByPredicate(
					[RuleId](const FSRNaturalStructureSpawnRuleOverride& Candidate)
					{
						return Candidate.RuleId == RuleId;
					});
			ResourceDistributionSummaries.Add(FString::Printf(
				TEXT("%s[%d-%d]"),
				*RuleId.ToString().Replace(TEXT("ResourceV2."), TEXT("")),
				Override ? Override->MinimumGuaranteedCount : 0,
				Override ? Override->MaxCount : 0));
		}
		ResourceDistributionSummaries.Sort();
		UE_LOG(LogTemp, Display,
			TEXT("Planet %s | Name=%s | Weight=%.2f | Profile=%s | Shape=%s | Scale=%.2f | Ocean=%s | Atmosphere=%s | Height=%.2f | OceanThreshold=%.3f | Continent=%.2f | Mountain=%.2f/%.2f | Valley=%.2f | River=%.2f | Lake=%.2f | TempFreq/Bias=%.2f/%+.2f | MoistureFreq/Bias=%.2f/%+.2f | Detail=%.2f/%.2f | Noise=%.2f/%d/%.2f | Resources=%s | Overrides=%d"),
			*Planet->GetPathName(),
			*Planet->VariableName.ToString(),
			Planet->GenerationWeight,
			*GetObjectPath(Planet->TerrainProfileDataAsset.Get()),
			*GetObjectPath(Planet->ShapeDataAsset.Get()),
			Planet->Scale,
			Planet->bHasOcean ? TEXT("true") : TEXT("false"),
			Planet->bHasAtmosphere ? TEXT("true") : TEXT("false"),
			Terrain.DynamicMeshHeight,
			Terrain.OceanThreshold,
			Terrain.ContinentFrequency,
			Terrain.MountainFrequency,
			Terrain.MountainStrength,
			Terrain.ValleyStrength,
			Terrain.RiverStrength,
			Terrain.LakeStrength,
			Terrain.TemperatureFrequency,
			Terrain.TemperatureBias,
			Terrain.MoistureFrequency,
			Terrain.MoistureBias,
			Terrain.DetailFrequency,
			Terrain.DetailStrength,
			Terrain.NoiseStrength,
			Terrain.NoiseOctaves,
			Terrain.NoisePersistence,
			*FString::Join(ResourceDistributionSummaries, TEXT(",")),
			Planet->ProfileNaturalStructureSpawnRuleOverrides.Num());
		LogProfile(Planet->TerrainProfileDataAsset.Get());
	}

	FSRBiomePlacementRule MakePlacementRule(
		ESRBiomePlacementMetric Metric,
		ESRBiomePlacementComparison Comparison,
		float Threshold,
		float MaxThreshold = 1.0f)
	{
		FSRBiomePlacementRule Rule;
		Rule.Metric = Metric;
		Rule.Comparison = Comparison;
		Rule.bUseMetricDefaultThresholds = false;
		Rule.Threshold = Threshold;
		Rule.MaxThreshold = MaxThreshold;
		Rule.RefreshMetricDefaults(false);
		return Rule;
	}

	USRPlanetBiomeDataAsset* ConfigureBiome(
		const FString& AssetName,
		const TCHAR* BiomeId,
		ESRBiomeWaterRole WaterRole,
		const TArray<FSRBiomePlacementRule>& PlacementRules,
		float SpawnWeight,
		float RegionSize,
		int32 Priority,
		bool bCanOverride,
		float OverrideMinScore,
		bool& bInOutSuccess)
	{
		bool bCreated = false;
		USRPlanetBiomeDataAsset* Biome = LoadOrCreateAsset<USRPlanetBiomeDataAsset>(
			FString::Printf(TEXT("%s/%s"), BiomeRoot, *AssetName),
			bCreated);
		if (!IsValid(Biome))
		{
			bInOutSuccess = false;
			return nullptr;
		}

		Biome->BiomeId = FName(BiomeId);
		Biome->WaterRole = WaterRole;
		Biome->PlacementRestrictions.Reset();
		Biome->PlacementRules = PlacementRules;
		Biome->SpawnWeight = FMath::Max(0.01f, SpawnWeight);
		Biome->RegionSize = FMath::Clamp(RegionSize, 0.01f, 1.0f);
		Biome->Priority = Priority;
		Biome->bCanOverrideLowerPriorityBiomes = bCanOverride;
		Biome->OverrideMinScore = FMath::Max(0.0f, OverrideMinScore);
		Biome->NaturalStructureSpawnRules.Reset();
		bInOutSuccess &= SaveAsset(Biome);
		UE_LOG(LogTemp, Display,
			TEXT("%s biome asset %s"),
			bCreated ? TEXT("Created") : TEXT("Updated"),
			*Biome->GetPathName());
		return Biome;
	}

	USRPlanetTerrainProfileDataAsset* ConfigureProfile(
		const TCHAR* ProfileAssetName,
		const TCHAR* ProfileId,
		const TArray<USRPlanetBiomeDataAsset*>& Biomes,
		const TArray<FSRProfileNaturalStructureSpawnRule>& NaturalStructureRules,
		bool& bInOutSuccess)
	{
		bool bCreated = false;
		USRPlanetTerrainProfileDataAsset* Profile =
			LoadOrCreateAsset<USRPlanetTerrainProfileDataAsset>(
				FString::Printf(TEXT("%s/%s"), ProfileRoot, ProfileAssetName),
				bCreated);
		if (!IsValid(Profile))
		{
			bInOutSuccess = false;
			return nullptr;
		}

		Profile->ProfileId = FName(ProfileId);
		Profile->Biomes.Reset(Biomes.Num());
		for (USRPlanetBiomeDataAsset* Biome : Biomes)
		{
			if (IsValid(Biome))
			{
				FSRPlanetProfileBiomeEntry& Entry = Profile->Biomes.AddDefaulted_GetRef();
				Entry.BiomeDataAsset = Biome;
			}
		}
		Profile->ProfileNaturalStructureSpawnRules = NaturalStructureRules;
		bInOutSuccess &= SaveAsset(Profile);
		UE_LOG(LogTemp, Display,
			TEXT("%s terrain profile %s with %d biomes and %d resource rules"),
			bCreated ? TEXT("Created") : TEXT("Updated"),
			*Profile->GetPathName(),
			Profile->Biomes.Num(),
			Profile->ProfileNaturalStructureSpawnRules.Num());
		return Profile;
	}

	void CopyPlanetTemplate(const USRPlanetDataAsset& Source, USRPlanetDataAsset& Target)
	{
		if (&Source == &Target)
		{
			return;
		}
		Target.BodyCategory = Source.BodyCategory;
		Target.Scale = Source.Scale;
		Target.ShapeDataAsset = Source.ShapeDataAsset;
		Target.Material = Source.Material;
		Target.ToonOutlineSettings = Source.ToonOutlineSettings;
		Target.Mass = Source.Mass;
		Target.GravityRatio = Source.GravityRatio;
		Target.GravityRadiusRatio = Source.GravityRadiusRatio;
		Target.SurfaceGridHeightOffset = Source.SurfaceGridHeightOffset;
		Target.DynamicMeshGeneration = Source.DynamicMeshGeneration;
		Target.bHasOcean = Source.bHasOcean;
		Target.OceanScaleMultiplier = Source.OceanScaleMultiplier;
		Target.OceanMaterial = Source.OceanMaterial;
		Target.bHasAtmosphere = Source.bHasAtmosphere;
		Target.AtmosphereScaleMultiplier = Source.AtmosphereScaleMultiplier;
		Target.AtmosphereMaterial = Source.AtmosphereMaterial;
	}

	void SetEnvironmentPalette(FSRDynamicMeshGeneration& Terrain, FName EnvironmentId)
	{
		Terrain.bApplyTemperatureStateSurfaceColor = true;
		if (EnvironmentId == TEXT("LavaOcean"))
		{
			Terrain.TemperatureStateSurfaceColorBlendAlpha = 0.78f;
			Terrain.FrozenTemperatureStateSurfaceColor = FLinearColor(0.20f, 0.08f, 0.06f, 1.0f);
			Terrain.ColdTemperatureStateSurfaceColor = FLinearColor(0.35f, 0.10f, 0.05f, 1.0f);
			Terrain.NormalTemperatureStateSurfaceColor = FLinearColor(0.62f, 0.16f, 0.03f, 1.0f);
			Terrain.HotTemperatureStateSurfaceColor = FLinearColor(1.00f, 0.38f, 0.02f, 1.0f);
			Terrain.OverheatedTemperatureStateSurfaceColor = FLinearColor(1.00f, 0.05f, 0.01f, 1.0f);
		}
		else if (EnvironmentId == TEXT("FrozenOcean"))
		{
			Terrain.TemperatureStateSurfaceColorBlendAlpha = 0.74f;
			Terrain.FrozenTemperatureStateSurfaceColor = FLinearColor(0.72f, 0.95f, 1.00f, 1.0f);
			Terrain.ColdTemperatureStateSurfaceColor = FLinearColor(0.24f, 0.62f, 1.00f, 1.0f);
			Terrain.NormalTemperatureStateSurfaceColor = FLinearColor(0.30f, 0.54f, 0.72f, 1.0f);
			Terrain.HotTemperatureStateSurfaceColor = FLinearColor(0.52f, 0.68f, 0.78f, 1.0f);
			Terrain.OverheatedTemperatureStateSurfaceColor = FLinearColor(0.72f, 0.78f, 0.82f, 1.0f);
		}
		else if (EnvironmentId == TEXT("AridDesert"))
		{
			Terrain.TemperatureStateSurfaceColorBlendAlpha = 0.58f;
			Terrain.FrozenTemperatureStateSurfaceColor = FLinearColor(0.56f, 0.48f, 0.34f, 1.0f);
			Terrain.ColdTemperatureStateSurfaceColor = FLinearColor(0.66f, 0.52f, 0.28f, 1.0f);
			Terrain.NormalTemperatureStateSurfaceColor = FLinearColor(0.78f, 0.58f, 0.20f, 1.0f);
			Terrain.HotTemperatureStateSurfaceColor = FLinearColor(1.00f, 0.62f, 0.12f, 1.0f);
			Terrain.OverheatedTemperatureStateSurfaceColor = FLinearColor(0.92f, 0.30f, 0.05f, 1.0f);
		}
		else if (EnvironmentId == TEXT("ToxicWetland"))
		{
			Terrain.TemperatureStateSurfaceColorBlendAlpha = 0.64f;
			Terrain.FrozenTemperatureStateSurfaceColor = FLinearColor(0.20f, 0.52f, 0.44f, 1.0f);
			Terrain.ColdTemperatureStateSurfaceColor = FLinearColor(0.16f, 0.66f, 0.30f, 1.0f);
			Terrain.NormalTemperatureStateSurfaceColor = FLinearColor(0.42f, 0.82f, 0.12f, 1.0f);
			Terrain.HotTemperatureStateSurfaceColor = FLinearColor(0.72f, 0.80f, 0.06f, 1.0f);
			Terrain.OverheatedTemperatureStateSurfaceColor = FLinearColor(0.72f, 0.20f, 0.82f, 1.0f);
		}
		else if (EnvironmentId == TEXT("Badlands"))
		{
			Terrain.TemperatureStateSurfaceColorBlendAlpha = 0.54f;
			Terrain.FrozenTemperatureStateSurfaceColor = FLinearColor(0.34f, 0.28f, 0.25f, 1.0f);
			Terrain.ColdTemperatureStateSurfaceColor = FLinearColor(0.46f, 0.31f, 0.22f, 1.0f);
			Terrain.NormalTemperatureStateSurfaceColor = FLinearColor(0.62f, 0.36f, 0.20f, 1.0f);
			Terrain.HotTemperatureStateSurfaceColor = FLinearColor(0.82f, 0.38f, 0.12f, 1.0f);
			Terrain.OverheatedTemperatureStateSurfaceColor = FLinearColor(0.72f, 0.18f, 0.08f, 1.0f);
		}
		else
		{
			Terrain.TemperatureStateSurfaceColorBlendAlpha = 0.48f;
			Terrain.FrozenTemperatureStateSurfaceColor = FLinearColor(0.12f, 0.82f, 1.0f, 1.0f);
			Terrain.ColdTemperatureStateSurfaceColor = FLinearColor(0.16f, 0.32f, 1.0f, 1.0f);
			Terrain.NormalTemperatureStateSurfaceColor = FLinearColor(0.50f, 0.86f, 0.42f, 1.0f);
			Terrain.HotTemperatureStateSurfaceColor = FLinearColor(1.0f, 0.50f, 0.08f, 1.0f);
			Terrain.OverheatedTemperatureStateSurfaceColor = FLinearColor(1.0f, 0.08f, 0.03f, 1.0f);
		}
	}

	void ConfigurePlanetTerrain(
		USRPlanetDataAsset& Planet,
		const TCHAR* DisplayName,
		FName EnvironmentId,
		float GenerationWeight,
		USRPlanetTerrainProfileDataAsset& Profile,
		bool bHasOcean,
		float DynamicMeshHeight,
		float OceanThreshold,
		float ContinentFrequency,
		float MountainFrequency,
		float MountainStrength,
		float ValleyStrength,
		float RiverStrength,
		float LakeStrength,
		float TemperatureFrequency,
		float TemperatureBias,
		float MoistureFrequency,
		float MoistureBias,
		float DetailFrequency,
		float DetailStrength,
		float NoiseStrength,
		int32 NoiseOctaves,
		float NoisePersistence)
	{
		Planet.VariableName = FText::FromString(DisplayName);
		Planet.GenerationWeight = FMath::Max(0.0f, GenerationWeight);
		Planet.TerrainProfileDataAsset = &Profile;
		Planet.bHasOcean = bHasOcean;
		Planet.bHasAtmosphere = true;
		FSRDynamicMeshGeneration& Terrain = Planet.DynamicMeshGeneration;
		Terrain.bDynamicMeshGeneration = true;
		Terrain.bMinecraft = false;
		Terrain.bClampTerrainHeightToOceanLevel = false;
		Terrain.DynamicMeshHeight = DynamicMeshHeight;
		Terrain.OceanThreshold = OceanThreshold;
		Terrain.ContinentFrequency = ContinentFrequency;
		Terrain.MountainFrequency = MountainFrequency;
		Terrain.MountainStrength = MountainStrength;
		Terrain.ValleyStrength = ValleyStrength;
		Terrain.RiverStrength = RiverStrength;
		Terrain.LakeStrength = LakeStrength;
		Terrain.TemperatureFrequency = TemperatureFrequency;
		Terrain.TemperatureBias = FMath::Clamp(TemperatureBias, -1.0f, 1.0f);
		Terrain.MoistureFrequency = MoistureFrequency;
		Terrain.MoistureBias = FMath::Clamp(MoistureBias, -1.0f, 1.0f);
		Terrain.DetailFrequency = DetailFrequency;
		Terrain.DetailStrength = DetailStrength;
		Terrain.NoiseStrength = NoiseStrength;
		Terrain.NoiseOctaves = NoiseOctaves;
		Terrain.NoisePersistence = NoisePersistence;
		SetEnvironmentPalette(Terrain, EnvironmentId);
		Profile.ApplyToDynamicMeshGeneration(Terrain);
	}

	void ConfigureResourceDistribution(
		USRPlanetDataAsset& Planet,
		const USRPlanetTerrainProfileDataAsset& Profile,
		FName PrimaryResourceId,
		FName SecondaryResourceId,
		FName UtilityResourceId)
	{
		Planet.ProfileNaturalStructureSpawnRuleOverrides.Reset(
			Profile.ProfileNaturalStructureSpawnRules.Num());
		for (const FSRProfileNaturalStructureSpawnRule& Rule : Profile.ProfileNaturalStructureSpawnRules)
		{
			FSRNaturalStructureSpawnRuleOverride& Override =
				Planet.ProfileNaturalStructureSpawnRuleOverrides.AddDefaulted_GetRef();
			Override.RuleId = Rule.RuleId;
			Override.bEnabled = Rule.bEnabled;
			Override.SpawnChancePerCell = Rule.SpawnChancePerCell;
			Override.MaxCount = Rule.MaxCount;
			Override.MinimumGuaranteedCount = Rule.MinimumGuaranteedCount;
			Override.MinCellSpacing = Rule.MinCellSpacing;

			const FName PrimaryRuleId(*FString::Printf(
				TEXT("ResourceV2.%s"),
				*PrimaryResourceId.ToString()));
			const FName SecondaryRuleId(*FString::Printf(
				TEXT("ResourceV2.%s"),
				*SecondaryResourceId.ToString()));
			const FName UtilityRuleId(*FString::Printf(
				TEXT("ResourceV2.%s"),
				*UtilityResourceId.ToString()));
			if (Rule.RuleId == PrimaryRuleId)
			{
				Override.bEnabled = true;
				Override.SpawnChancePerCell = FMath::Min(1.0f, Rule.SpawnChancePerCell * 1.50f);
				Override.MaxCount = 10;
				Override.MinimumGuaranteedCount = 6;
				Override.MinCellSpacing = 6;
			}
			else if (Rule.RuleId == SecondaryRuleId)
			{
				Override.bEnabled = true;
				Override.SpawnChancePerCell = FMath::Min(1.0f, Rule.SpawnChancePerCell);
				Override.MaxCount = 7;
				Override.MinimumGuaranteedCount = 4;
				Override.MinCellSpacing = 8;
			}
			else if (Rule.RuleId == UtilityRuleId)
			{
				Override.bEnabled = true;
				Override.SpawnChancePerCell = FMath::Min(1.0f, Rule.SpawnChancePerCell * 1.15f);
				Override.MaxCount = 8;
				Override.MinimumGuaranteedCount = 5;
				Override.MinCellSpacing = 7;
			}
			else if (Rule.RuleId.ToString().StartsWith(TEXT("ResourceV2.")))
			{
				Override.bEnabled = false;
				Override.SpawnChancePerCell = 0.0f;
				Override.MaxCount = 0;
				Override.MinimumGuaranteedCount = 0;
			}
		}
	}

	USRPlanetDataAsset* LoadOrCreatePlanet(
		const FString& PackageName,
		const USRPlanetDataAsset& Template,
		bool& bOutCreated,
		bool& bInOutSuccess)
	{
		USRPlanetDataAsset* Planet = LoadOrCreateAsset<USRPlanetDataAsset>(PackageName, bOutCreated);
		if (!IsValid(Planet))
		{
			bInOutSuccess = false;
			return nullptr;
		}
		CopyPlanetTemplate(Template, *Planet);
		return Planet;
	}

	bool ConfigureGeneratorCatalog(const TArray<USRPlanetDataAsset*>& PlanetCatalog)
	{
		bool bSuccess = true;
		UBlueprint* GeneratorBlueprint = LoadObject<UBlueprint>(nullptr, GeneratorBlueprintPath);
		ASRSolarSystemGenerator* GeneratorCDO = IsValid(GeneratorBlueprint)
			&& IsValid(GeneratorBlueprint->GeneratedClass)
			? Cast<ASRSolarSystemGenerator>(GeneratorBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		if (!IsValid(GeneratorBlueprint) || !IsValid(GeneratorCDO))
		{
			UE_LOG(LogTemp, Error, TEXT("Could not load BP_SolarSystemGenerator defaults."));
			return false;
		}

		GeneratorBlueprint->Modify();
		GeneratorCDO->ConfigurePlanetEnvironmentCatalogForEditor(
			PlanetCatalog,
			4,
			5,
			7,
			1,
			3,
			MakeRequiredSystemResourceRuleIds());
		FBlueprintEditorUtils::MarkBlueprintAsModified(GeneratorBlueprint);
		bSuccess &= SaveAsset(GeneratorBlueprint);

		UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(SolarSystemMapPackagePath);
		if (!IsValid(World))
		{
			UE_LOG(LogTemp, Error, TEXT("Could not load SolarSystem map for catalog authoring."));
			return false;
		}

		int32 UpdatedGeneratorCount = 0;
		for (ULevel* Level : World->GetLevels())
		{
			if (!IsValid(Level))
			{
				continue;
			}
			for (AActor* Actor : Level->Actors)
			{
				ASRSolarSystemGenerator* Generator = Cast<ASRSolarSystemGenerator>(Actor);
				if (!IsValid(Generator))
				{
					continue;
				}
				Generator->ConfigurePlanetEnvironmentCatalogForEditor(
					PlanetCatalog,
					4,
					5,
					7,
					1,
					3,
					MakeRequiredSystemResourceRuleIds());
				Generator->PostEditChange();
				++UpdatedGeneratorCount;
			}
		}
		if (UpdatedGeneratorCount != 1)
		{
			UE_LOG(LogTemp, Error,
				TEXT("Expected one SolarSystemGenerator level instance, found %d."),
				UpdatedGeneratorCount);
			bSuccess = false;
		}
		bSuccess &= UEditorLoadingAndSavingUtils::SaveMap(World, SolarSystemMapPackagePath);
		return bSuccess;
	}

	bool ValidateGeneratedCatalog(const TArray<USRPlanetDataAsset*>& PlanetCatalog)
	{
		bool bSuccess = PlanetCatalog.Num() == 6;
		const TArray<FName> RequiredSystemResourceRuleIds = MakeRequiredSystemResourceRuleIds();
		TMap<FName, int32> SourceEnvironmentCountByRuleId;
		TSet<const USRPlanetTerrainProfileDataAsset*> UniqueProfiles;
		TSet<FString> UniqueClimateSignatures;
		for (USRPlanetDataAsset* Planet : PlanetCatalog)
		{
			if (!IsValid(Planet)
				|| !IsValid(Planet->ShapeDataAsset.Get())
				|| !IsValid(Planet->TerrainProfileDataAsset.Get())
				|| Planet->GenerationWeight <= 0.0f
				|| Planet->TerrainProfileDataAsset->Biomes.Num() < 3
				|| Planet->TerrainProfileDataAsset->ProfileNaturalStructureSpawnRules.Num() < 7)
			{
				UE_LOG(LogTemp, Error,
					TEXT("Invalid generated planet environment: %s"),
					*GetObjectPath(Planet));
				bSuccess = false;
				continue;
			}
			UniqueProfiles.Add(Planet->TerrainProfileDataAsset.Get());
			TSet<FName> EnabledResourceRuleIds;
			FSRPlanetEnvironmentSelector::GetEnabledResourceRuleIds(
				Planet,
				EnabledResourceRuleIds);
			if (EnabledResourceRuleIds.Num() != 3)
			{
				UE_LOG(LogTemp, Error,
					TEXT("Planet environment %s must expose exactly two fuel cards and one utility resource, found %d effective rules."),
					*Planet->VariableName.ToString(),
					EnabledResourceRuleIds.Num());
				bSuccess = false;
			}
			for (const FName RuleId : EnabledResourceRuleIds)
			{
				++SourceEnvironmentCountByRuleId.FindOrAdd(RuleId);
				const FSRNaturalStructureSpawnRuleOverride* Override =
					Planet->ProfileNaturalStructureSpawnRuleOverrides.FindByPredicate(
						[RuleId](const FSRNaturalStructureSpawnRuleOverride& Candidate)
						{
							return Candidate.RuleId == RuleId;
						});
				if (!Override
					|| Override->MinimumGuaranteedCount <= 0
					|| (Override->MaxCount > 0
						&& Override->MinimumGuaranteedCount > Override->MaxCount))
				{
					UE_LOG(LogTemp, Error,
						TEXT("Planet environment %s has no valid guaranteed count for %s."),
						*Planet->VariableName.ToString(),
						*RuleId.ToString());
					bSuccess = false;
				}
			}
			UniqueClimateSignatures.Add(FString::Printf(
				TEXT("%.3f:%.3f:%.3f:%.3f:%.3f"),
				Planet->DynamicMeshGeneration.OceanThreshold,
				Planet->DynamicMeshGeneration.TemperatureBias,
				Planet->DynamicMeshGeneration.MoistureBias,
				Planet->DynamicMeshGeneration.MountainStrength,
				Planet->DynamicMeshGeneration.RiverStrength));

			constexpr int32 SampleCount = 768;
			double TemperatureTotal = 0.0;
			double MoistureTotal = 0.0;
			int32 WaterSamples = 0;
			for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
			{
				const double Unit = (static_cast<double>(SampleIndex) + 0.5)
					/ static_cast<double>(SampleCount);
				const double Z = 1.0 - 2.0 * Unit;
				const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
				const double Angle = UE_DOUBLE_TWO_PI * Unit * 193.0;
				const FVector Direction(
					Radius * FMath::Cos(Angle),
					Radius * FMath::Sin(Angle),
					Z);
				const FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(
					Direction,
					Planet->DynamicMeshGeneration);
				TemperatureTotal += Sample.Temperature;
				MoistureTotal += Sample.Moisture;
				WaterSamples += FSRPlanetTerrainGenerator::IsOceanLevelWaterSample(Sample) ? 1 : 0;
			}
			UE_LOG(LogTemp, Display,
				TEXT("Environment sample %s | mean temperature=%.3f moisture=%.3f water-role=%.1f%%"),
				*Planet->VariableName.ToString(),
				TemperatureTotal / SampleCount,
				MoistureTotal / SampleCount,
				100.0 * static_cast<double>(WaterSamples) / SampleCount);
		}
		bSuccess &= UniqueProfiles.Num() == PlanetCatalog.Num();
		bSuccess &= UniqueClimateSignatures.Num() == PlanetCatalog.Num();
		for (const FName RequiredRuleId : RequiredSystemResourceRuleIds)
		{
			const int32 SourceEnvironmentCount =
				SourceEnvironmentCountByRuleId.FindRef(RequiredRuleId);
			UE_LOG(LogTemp, Display,
				TEXT("Resource portfolio source environments %s=%d"),
				*RequiredRuleId.ToString(),
				SourceEnvironmentCount);
			bSuccess &= SourceEnvironmentCount >= 2;
		}

		TMap<const USRPlanetDataAsset*, int32> AppearanceCounts;
		for (int32 Seed = 1; Seed <= 96; ++Seed)
		{
			FRandomStream RandomStream(Seed);
			TArray<const USRPlanetDataAsset*> Candidates;
			for (USRPlanetDataAsset* Planet : PlanetCatalog)
			{
				Candidates.Add(Planet);
			}
			TArray<const USRPlanetDataAsset*> Selected;
			FSRPlanetEnvironmentSelectionReport SelectionReport;
			const int32 RequestedCount = 5 + Seed % 3;
			FSRPlanetEnvironmentSelector::SelectWithResourceCoverage(
				Candidates,
				RequestedCount,
				4,
				RequiredSystemResourceRuleIds,
				RandomStream,
				Selected,
				SelectionReport);
			TSet<const USRPlanetDataAsset*> UniqueSelected;
			for (const USRPlanetDataAsset* Planet : Selected)
			{
				UniqueSelected.Add(Planet);
			}
			if (Selected.Num() != RequestedCount
				|| UniqueSelected.Num() < 4
				|| !SelectionReport.bResourceCoverageSatisfied)
			{
				bSuccess = false;
			}
			for (const USRPlanetDataAsset* Planet : Selected)
			{
				++AppearanceCounts.FindOrAdd(Planet);
			}
		}
		for (USRPlanetDataAsset* Planet : PlanetCatalog)
		{
			const int32 AppearanceCount = AppearanceCounts.FindRef(Planet);
			UE_LOG(LogTemp, Display,
				TEXT("96-seed appearance %s=%d"),
				*Planet->VariableName.ToString(),
				AppearanceCount);
			bSuccess &= AppearanceCount > 0;
		}
		return bSuccess;
	}

	bool GeneratePlanetEnvironmentContent()
	{
		bool bSuccess = true;
		USRPlanetTerrainProfileDataAsset* EarthProfile =
			LoadObject<USRPlanetTerrainProfileDataAsset>(nullptr, EarthProfilePath);
		USRPlanetDataAsset* Temperate = LoadObject<USRPlanetDataAsset>(
			nullptr,
			*MakeObjectPath(TemperatePlanetPackage));
		USRPlanetDataAsset* LavaOcean = LoadObject<USRPlanetDataAsset>(
			nullptr,
			*MakeObjectPath(LavaOceanPlanetPackage));
		if (!IsValid(EarthProfile) || !IsValid(Temperate) || !IsValid(LavaOcean))
		{
			UE_LOG(LogTemp, Error,
				TEXT("Planet environment generation requires Earth, Temperate, and LavaOcean templates."));
			return false;
		}

		const auto OceanRule = MakePlacementRule(
			ESRBiomePlacementMetric::OceanDepthMask,
			ESRBiomePlacementComparison::GreaterOrEqual,
			0.36f);
		const auto CoastRule = MakePlacementRule(
			ESRBiomePlacementMetric::CoastMask,
			ESRBiomePlacementComparison::GreaterOrEqual,
			0.38f);
		const auto MountainRule = MakePlacementRule(
			ESRBiomePlacementMetric::MountainMask,
			ESRBiomePlacementComparison::GreaterOrEqual,
			0.46f);

		USRPlanetBiomeDataAsset* Basalt = ConfigureBiome(
			TEXT("DA_Biome_BasaltPlain"), TEXT("BasaltPlain"), ESRBiomeWaterRole::None,
			{}, 1.0f, 0.42f, 0, false, 0.65f, bSuccess);
		USRPlanetBiomeDataAsset* MagmaOcean = ConfigureBiome(
			TEXT("DA_Biome_MagmaOcean"), TEXT("MagmaOcean"), ESRBiomeWaterRole::Ocean,
			{ OceanRule }, 1.2f, 0.58f, 30, true, 0.0f, bSuccess);
		USRPlanetBiomeDataAsset* AshCoast = ConfigureBiome(
			TEXT("DA_Biome_AshCoast"), TEXT("AshCoast"), ESRBiomeWaterRole::Coast,
			{ CoastRule }, 1.1f, 0.20f, 25, true, 0.0f, bSuccess);
		USRPlanetBiomeDataAsset* ObsidianRidge = ConfigureBiome(
			TEXT("DA_Biome_ObsidianRidge"), TEXT("ObsidianRidge"), ESRBiomeWaterRole::None,
			{ MountainRule, MakePlacementRule(ESRBiomePlacementMetric::Temperature, ESRBiomePlacementComparison::GreaterOrEqual, 0.58f) },
			1.3f, 0.24f, 12, true, 0.42f, bSuccess);

		USRPlanetBiomeDataAsset* DustBarrens = ConfigureBiome(
			TEXT("DA_Biome_DustBarrens"), TEXT("DustBarrens"), ESRBiomeWaterRole::None,
			{}, 1.0f, 0.50f, 0, false, 0.65f, bSuccess);
		USRPlanetBiomeDataAsset* ErodedCanyon = ConfigureBiome(
			TEXT("DA_Biome_ErodedCanyon"), TEXT("ErodedCanyon"), ESRBiomeWaterRole::None,
			{ MakePlacementRule(ESRBiomePlacementMetric::Erosion, ESRBiomePlacementComparison::GreaterOrEqual, 0.58f) },
			1.1f, 0.32f, 8, true, 0.46f, bSuccess);
		USRPlanetBiomeDataAsset* IronCrag = ConfigureBiome(
			TEXT("DA_Biome_IronCrag"), TEXT("IronCrag"), ESRBiomeWaterRole::None,
			{ MountainRule }, 1.25f, 0.26f, 12, true, 0.42f, bSuccess);

		USRPlanetBiomeDataAsset* Snowfield = ConfigureBiome(
			TEXT("DA_Biome_Snowfield"), TEXT("Snowfield"), ESRBiomeWaterRole::None,
			{}, 1.0f, 0.52f, 0, false, 0.65f, bSuccess);
		USRPlanetBiomeDataAsset* CryoOcean = ConfigureBiome(
			TEXT("DA_Biome_CryoOcean"), TEXT("CryoOcean"), ESRBiomeWaterRole::Ocean,
			{ OceanRule }, 1.15f, 0.56f, 30, true, 0.0f, bSuccess);
		USRPlanetBiomeDataAsset* IceShelf = ConfigureBiome(
			TEXT("DA_Biome_IceShelf"), TEXT("IceShelf"), ESRBiomeWaterRole::Coast,
			{ CoastRule }, 1.2f, 0.25f, 25, true, 0.0f, bSuccess);
		USRPlanetBiomeDataAsset* Glacier = ConfigureBiome(
			TEXT("DA_Biome_Glacier"), TEXT("Glacier"), ESRBiomeWaterRole::None,
			{ MakePlacementRule(ESRBiomePlacementMetric::Temperature, ESRBiomePlacementComparison::LessOrEqual, 0.34f), MountainRule },
			1.35f, 0.28f, 12, true, 0.40f, bSuccess);

		USRPlanetBiomeDataAsset* DuneSea = ConfigureBiome(
			TEXT("DA_Biome_DuneSea"), TEXT("DuneSea"), ESRBiomeWaterRole::None,
			{}, 1.0f, 0.58f, 0, false, 0.65f, bSuccess);
		USRPlanetBiomeDataAsset* SaltFlat = ConfigureBiome(
			TEXT("DA_Biome_SaltFlat"), TEXT("SaltFlat"), ESRBiomeWaterRole::None,
			{ MakePlacementRule(ESRBiomePlacementMetric::HeightAlpha, ESRBiomePlacementComparison::LessOrEqual, 0.16f),
				MakePlacementRule(ESRBiomePlacementMetric::Moisture, ESRBiomePlacementComparison::LessOrEqual, 0.34f) },
			1.2f, 0.36f, 8, true, 0.44f, bSuccess);
		USRPlanetBiomeDataAsset* Mesa = ConfigureBiome(
			TEXT("DA_Biome_Mesa"), TEXT("Mesa"), ESRBiomeWaterRole::None,
			{ MountainRule }, 1.25f, 0.27f, 12, true, 0.42f, bSuccess);

		USRPlanetBiomeDataAsset* ToxicMire = ConfigureBiome(
			TEXT("DA_Biome_ToxicMire"), TEXT("ToxicMire"), ESRBiomeWaterRole::None,
			{}, 1.0f, 0.44f, 0, false, 0.65f, bSuccess);
		USRPlanetBiomeDataAsset* AcidOcean = ConfigureBiome(
			TEXT("DA_Biome_AcidOcean"), TEXT("AcidOcean"), ESRBiomeWaterRole::Ocean,
			{ OceanRule }, 1.25f, 0.52f, 30, true, 0.0f, bSuccess);
		USRPlanetBiomeDataAsset* FungalCoast = ConfigureBiome(
			TEXT("DA_Biome_FungalCoast"), TEXT("FungalCoast"), ESRBiomeWaterRole::Coast,
			{ CoastRule }, 1.15f, 0.24f, 25, true, 0.0f, bSuccess);
		USRPlanetBiomeDataAsset* SporeMarsh = ConfigureBiome(
			TEXT("DA_Biome_SporeMarsh"), TEXT("SporeMarsh"), ESRBiomeWaterRole::None,
			{ MakePlacementRule(ESRBiomePlacementMetric::Moisture, ESRBiomePlacementComparison::GreaterOrEqual, 0.64f),
				MakePlacementRule(ESRBiomePlacementMetric::InlandMask, ESRBiomePlacementComparison::GreaterOrEqual, 0.36f) },
			1.35f, 0.34f, 12, true, 0.42f, bSuccess);

		const TArray<FSRProfileNaturalStructureSpawnRule> ResourceRules =
			EarthProfile->ProfileNaturalStructureSpawnRules;
		USRPlanetTerrainProfileDataAsset* LavaProfile = ConfigureProfile(
			TEXT("DA_Profile_LavaOcean"), TEXT("LavaOcean"),
			{ Basalt, MagmaOcean, AshCoast, ObsidianRidge }, ResourceRules, bSuccess);
		USRPlanetTerrainProfileDataAsset* BadlandsProfile = ConfigureProfile(
			TEXT("DA_Profile_Badlands"), TEXT("Badlands"),
			{ DustBarrens, ErodedCanyon, IronCrag }, ResourceRules, bSuccess);
		USRPlanetTerrainProfileDataAsset* FrozenProfile = ConfigureProfile(
			TEXT("DA_Profile_FrozenOcean"), TEXT("FrozenOcean"),
			{ Snowfield, CryoOcean, IceShelf, Glacier }, ResourceRules, bSuccess);
		USRPlanetTerrainProfileDataAsset* AridProfile = ConfigureProfile(
			TEXT("DA_Profile_AridDesert"), TEXT("AridDesert"),
			{ DuneSea, SaltFlat, Mesa }, ResourceRules, bSuccess);
		USRPlanetTerrainProfileDataAsset* ToxicProfile = ConfigureProfile(
			TEXT("DA_Profile_ToxicWetland"), TEXT("ToxicWetland"),
			{ ToxicMire, AcidOcean, FungalCoast, SporeMarsh }, ResourceRules, bSuccess);
		if (!bSuccess || !IsValid(LavaProfile) || !IsValid(BadlandsProfile)
			|| !IsValid(FrozenProfile) || !IsValid(AridProfile) || !IsValid(ToxicProfile))
		{
			return false;
		}

		bool bCreated = false;
		USRPlanetDataAsset* Badlands = LoadOrCreatePlanet(
			BadlandsPlanetPackage, *Temperate, bCreated, bSuccess);
		USRPlanetDataAsset* FrozenOcean = LoadOrCreatePlanet(
			FrozenOceanPlanetPackage, *Temperate, bCreated, bSuccess);
		USRPlanetDataAsset* AridDesert = LoadOrCreatePlanet(
			AridDesertPlanetPackage, *Temperate, bCreated, bSuccess);
		USRPlanetDataAsset* ToxicWetland = LoadOrCreatePlanet(
			ToxicWetlandPlanetPackage, *Temperate, bCreated, bSuccess);
		if (!bSuccess || !IsValid(Badlands) || !IsValid(FrozenOcean)
			|| !IsValid(AridDesert) || !IsValid(ToxicWetland))
		{
			return false;
		}

		ConfigurePlanetTerrain(*Temperate, TEXT("Temperate"), TEXT("Temperate"), 1.35f,
			*EarthProfile, true, 16.0f, 0.86f, 1.10f, 5.5f, 1.30f, 0.28f, 0.32f, 0.20f,
			1.40f, 0.00f, 2.00f, 0.05f, 14.0f, 0.28f, 0.14f, 4, 0.48f);
		ConfigureResourceDistribution(
			*Temperate, *EarthProfile,
			TEXT("VerdantSpore"), TEXT("HeliosIron"), TEXT("BiomassFeedstock"));

		ConfigurePlanetTerrain(*LavaOcean, TEXT("Lava Ocean"), TEXT("LavaOcean"), 0.85f,
			*LavaProfile, true, 26.0f, 0.96f, 1.65f, 8.5f, 2.70f, 0.15f, 0.05f, 0.02f,
			1.10f, 0.38f, 1.40f, -0.35f, 24.0f, 0.60f, 0.28f, 5, 0.55f);
		ConfigureResourceDistribution(
			*LavaOcean, *LavaProfile,
			TEXT("AuroraPlasma"), TEXT("HeliosIron"), TEXT("CommonOre"));

		ConfigurePlanetTerrain(*Badlands, TEXT("Badlands"), TEXT("Badlands"), 1.05f,
			*BadlandsProfile, false, 30.0f, -0.32f, 1.35f, 9.0f, 2.40f, 0.65f, 0.05f, 0.00f,
			1.80f, 0.12f, 2.80f, -0.30f, 22.0f, 0.70f, 0.35f, 5, 0.56f);
		ConfigureResourceDistribution(
			*Badlands, *BadlandsProfile,
			TEXT("NullPearl"), TEXT("VerdantSpore"), TEXT("CommonOre"));

		ConfigurePlanetTerrain(*FrozenOcean, TEXT("Frozen Ocean"), TEXT("FrozenOcean"), 1.00f,
			*FrozenProfile, true, 20.0f, 0.92f, 0.85f, 4.5f, 1.40f, 0.20f, 0.12f, 0.34f,
			0.75f, -0.42f, 1.30f, 0.05f, 10.0f, 0.20f, 0.10f, 4, 0.45f);
		ConfigureResourceDistribution(
			*FrozenOcean, *FrozenProfile,
			TEXT("EchoQuartz"), TEXT("NullPearl"), TEXT("CommonOre"));

		ConfigurePlanetTerrain(*AridDesert, TEXT("Arid Desert"), TEXT("AridDesert"), 1.15f,
			*AridProfile, false, 18.0f, -0.38f, 0.75f, 3.4f, 1.00f, 0.20f, 0.00f, 0.00f,
			1.20f, 0.18f, 3.00f, -0.48f, 9.0f, 0.25f, 0.10f, 4, 0.44f);
		ConfigureResourceDistribution(
			*AridDesert, *AridProfile,
			TEXT("HeliosIron"), TEXT("EchoQuartz"), TEXT("CommonOre"));

		ConfigurePlanetTerrain(*ToxicWetland, TEXT("Toxic Wetland"), TEXT("ToxicWetland"), 0.80f,
			*ToxicProfile, true, 13.0f, 0.82f, 2.20f, 6.0f, 1.00f, 0.45f, 0.65f, 0.55f,
			2.40f, 0.08f, 1.00f, 0.42f, 16.0f, 0.40f, 0.25f, 5, 0.54f);
		ConfigureResourceDistribution(
			*ToxicWetland, *ToxicProfile,
			TEXT("VerdantSpore"), TEXT("AuroraPlasma"), TEXT("BiomassFeedstock"));

		const TArray<USRPlanetDataAsset*> PlanetCatalog = {
			Temperate,
			AridDesert,
			FrozenOcean,
			Badlands,
			LavaOcean,
			ToxicWetland,
		};
		for (USRPlanetDataAsset* Planet : PlanetCatalog)
		{
			bSuccess &= SaveAsset(Planet);
		}
		bSuccess &= ConfigureGeneratorCatalog(PlanetCatalog);
		bSuccess &= ValidateGeneratedCatalog(PlanetCatalog);
		return bSuccess;
	}

	void InspectAuthoredState()
	{
		UE_LOG(LogTemp, Display, TEXT("=== Planet environment authored-state inspection ==="));
		for (const FString& PlanetPath : PlanetObjectPaths)
		{
			LogPlanet(LoadObject<USRPlanetDataAsset>(nullptr, *PlanetPath));
		}

		if (UBlueprint* GeneratorBlueprint = LoadObject<UBlueprint>(nullptr, GeneratorBlueprintPath))
		{
			LogGenerator(IsValid(GeneratorBlueprint->GeneratedClass)
				? GeneratorBlueprint->GeneratedClass->GetDefaultObject()
				: nullptr);
		}

		if (UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(SolarSystemMapPackagePath))
		{
			int32 GeneratorCount = 0;
			for (ULevel* Level : World->GetLevels())
			{
				if (!IsValid(Level))
				{
					continue;
				}
				for (AActor* Actor : Level->Actors)
				{
					if (IsValid(Actor) && Actor->IsA<ASRSolarSystemGenerator>())
					{
						++GeneratorCount;
						LogGenerator(Actor);
					}
				}
			}
			UE_LOG(LogTemp, Display,
				TEXT("SolarSystem level generator instances=%d"),
				GeneratorCount);
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("Could not load SolarSystem map for generator inspection."));
		}
	}
}

#endif

USRGeneratePlanetEnvironmentContentCommandlet::USRGeneratePlanetEnvironmentContentCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 USRGeneratePlanetEnvironmentContentCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	if (FParse::Param(*Params, TEXT("InspectOnly")))
	{
		InspectAuthoredState();
		return 0;
	}

	UE_LOG(LogTemp, Display, TEXT("=== Generating test-play planet environments ==="));
	const bool bSuccess = GeneratePlanetEnvironmentContent();
	InspectAuthoredState();
	UE_LOG(LogTemp, Display,
		TEXT("Planet environment content generation %s."),
		bSuccess ? TEXT("succeeded") : TEXT("failed"));
	return bSuccess ? 0 : 1;
#else
	return 1;
#endif
}
