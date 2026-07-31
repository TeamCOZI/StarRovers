#include "Editor/SRGeneratePatternContentCommandlet.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceDataAsset.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRMoonDataAsset.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Celestial/SRPlanetShapeDataAsset.h"
#include "Celestial/SRStarDataAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "Pattern/SRPatternEnvironmentDataAsset.h"
#include "Pattern/SRPatternGenerationProfileDataAsset.h"
#include "Pattern/SRPatternGenerationValidator.h"
#include "Pattern/SRStellarPatternContract.h"
#include "Simulation/SRRunModifierDataAssets.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRSolarSystemGenerator.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetBiomeDataAsset.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/SavePackage.h"
#include "Utility/SRLog.h"

namespace StarRovers::Editor::PatternContent
{
	constexpr const TCHAR* ResourceRoot = TEXT("/Game/StarRovers/Pattern/DataAssets/Resources");
	constexpr const TCHAR* FacilityRoot = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities");
	constexpr const TCHAR* EnvironmentRoot = TEXT("/Game/StarRovers/Pattern/DataAssets/Environments");
	constexpr const TCHAR* FacilityStructureRoot = TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern");
	constexpr const TCHAR* DepositStructureRoot = TEXT("/Game/StarRovers/Structure/DataAssets/Natural/Pattern");

	constexpr const TCHAR* StarIronResourcePackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Resources/DA_Resource_StarIron");
	constexpr const TCHAR* BloomSapResourcePackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Resources/DA_Resource_BloomSap");
	constexpr const TCHAR* PrismShardResourcePackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Resources/DA_Resource_PrismShard");
	constexpr const TCHAR* TidalOreResourcePackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Resources/DA_Resource_TidalOre");
	constexpr const TCHAR* SolarMyceliumResourcePackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Resources/DA_Resource_SolarMycelium");
	constexpr const TCHAR* StellarWeaveResourcePackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Resources/DA_Resource_StellarWeave");

	constexpr const TCHAR* PatternExtractorFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_PatternExtractor");
	constexpr const TCHAR* PatternHubFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_PatternHub");
	constexpr const TCHAR* VectorEastFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_VectorShifterEast");
	constexpr const TCHAR* VectorWestFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_VectorShifterWest");
	constexpr const TCHAR* VectorNorthFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_VectorShifterNorth");
	constexpr const TCHAR* VectorSouthFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_VectorShifterSouth");
	constexpr const TCHAR* CenterlineFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_CenterlineShifter");
	constexpr const TCHAR* StellarLoomFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_StellarLoom");
	constexpr const TCHAR* LatticeSeparatorFacilityPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Facilities/DA_Facility_LatticeSeparator");

	constexpr const TCHAR* CrushingGravityEnvironmentPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Environments/DA_Environment_CrushingGravity");
	constexpr const TCHAR* PlasmaJetstreamEnvironmentPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Environments/DA_Environment_PlasmaJetstream");
	constexpr const TCHAR* SporeBloomEnvironmentPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Environments/DA_Environment_SporeBloom");
	constexpr const TCHAR* MagneticShearEnvironmentPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Environments/DA_Environment_MagneticShear");

	constexpr const TCHAR* GenerationProfilePackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Generation/DA_PatternGeneration_Default");
	constexpr const TCHAR* FoundationTechnologyPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Technology/DA_Tech_PatternFoundation");
	constexpr const TCHAR* ExpansionTechnologyPackage = TEXT("/Game/StarRovers/Pattern/DataAssets/Technology/DA_Tech_PatternExpansion");
	constexpr const TCHAR* SolarSystemGeneratorBlueprintPath = TEXT("/Game/StarRovers/Generation/Blueprints/BP_SolarSystemGenerator.BP_SolarSystemGenerator");
	constexpr const TCHAR* PlayerControllerBlueprintPath = TEXT("/Game/StarRovers/Core/Blueprints/BP_SRPlayerController.BP_SRPlayerController");
	constexpr const TCHAR* SolarSystemMapPackage = TEXT("/Game/Levels/SolarSystem");
	constexpr const TCHAR* ConveyorStructurePath = TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/DA_Structure_ConveyorBelt.DA_Structure_ConveyorBelt");
	constexpr const TCHAR* PlanetShapePath = TEXT("/Game/StarRovers/Celestial/DataAssets/PlanetShapes/DA_PlanetShape_Cube64.DA_PlanetShape_Cube64");
	constexpr const TCHAR* MoonShapePath = TEXT("/Game/StarRovers/Celestial/DataAssets/PlanetShapes/DA_PlanetShape_Cube64.DA_PlanetShape_Cube64");
	constexpr const TCHAR* PlanetTerrainProfilePath = TEXT("/Game/StarRovers/Surface/DataAssets/TerrainProfiles/DA_Profile_Earth.DA_Profile_Earth");
	constexpr const TCHAR* BadlandsMaterialPath = TEXT("/Game/Materials/Planet/Badlands/M_BadLands1.M_BadLands1");
	constexpr const TCHAR* CinderMaterialPath = TEXT("/Game/Materials/Planet/M_Ground.M_Ground");
	constexpr const TCHAR* VerdantMaterialPath = TEXT("/Game/Materials/Planet/M_Planet.M_Planet");
	constexpr const TCHAR* LavaOceanMaterialPath = TEXT("/Game/Materials/Planet/LavaOcean/M_LavaOcean_Ocean.M_LavaOcean_Ocean");
	constexpr const TCHAR* WaterMaterialPath = TEXT("/Game/Materials/Planet/Temperate/M_Temperate_Water2.M_Temperate_Water2");
	constexpr const TCHAR* AtmosphereMaterialPath = TEXT("/Game/Materials/Planet/Temperate/M_Temperate_Atmosphere_2.M_Temperate_Atmosphere_2");

	struct FPatternContentState
	{
		bool bApply = false;
		int32 ChangedAssetCount = 0;
		int32 CreatedAssetCount = 0;
		int32 RenamedAssetCount = 0;
		TMap<TObjectPtr<UPackage>, TObjectPtr<UObject>> PackagesToSave;
		TArray<FString> Errors;

		void MarkChanged(UObject* Asset)
		{
			if (!bApply || !IsValid(Asset))
			{
				return;
			}
			Asset->Modify();
			Asset->MarkPackageDirty();
			PackagesToSave.FindOrAdd(Asset->GetOutermost()) = Asset;
			++ChangedAssetCount;
		}

		void AddError(const FString& Error)
		{
			Errors.Add(Error);
			UE_LOG(LogTemp, Error, TEXT("Pattern content: %s"), *Error);
		}
	};

	TWeakObjectPtr<UWorld> LoadedSolarSystemEditorWorld;

	template <typename TAsset>
	TAsset* LoadOrCreateAsset(const TCHAR* LongPackageName, FPatternContentState& State)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(LongPackageName);
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), LongPackageName, *AssetName);
		if (TAsset* ExistingAsset = LoadObject<TAsset>(nullptr, *ObjectPath))
		{
			return ExistingAsset;
		}
		if (!State.bApply)
		{
			State.AddError(FString::Printf(TEXT("Required asset is missing: %s"), *ObjectPath));
			return nullptr;
		}

		UPackage* Package = CreatePackage(LongPackageName);
		if (!Package)
		{
			State.AddError(FString::Printf(TEXT("Could not create package: %s"), LongPackageName));
			return nullptr;
		}
		TAsset* NewAsset = NewObject<TAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		if (!NewAsset)
		{
			State.AddError(FString::Printf(TEXT("Could not create asset: %s"), *ObjectPath));
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(NewAsset);
		NewAsset->MarkPackageDirty();
		State.PackagesToSave.Add(Package, NewAsset);
		++State.CreatedAssetCount;
		return NewAsset;
	}

	FString MakeObjectPath(const TCHAR* LongPackageName)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(LongPackageName);
		return FString::Printf(TEXT("%s.%s"), LongPackageName, *AssetName);
	}

	template <typename TAsset>
	TAsset* LoadRenamedAsset(
		const TCHAR* LegacyObjectPath,
		const TCHAR* NewLongPackageName,
		FPatternContentState& State)
	{
		const FString NewObjectPath = MakeObjectPath(NewLongPackageName);
		if (TAsset* ExistingAsset = LoadObject<TAsset>(nullptr, *NewObjectPath))
		{
			return ExistingAsset;
		}

		TAsset* LegacyAsset = LoadObject<TAsset>(nullptr, LegacyObjectPath);
		if (!IsValid(LegacyAsset))
		{
			State.AddError(FString::Printf(
				TEXT("Required renamed asset is missing at both '%s' and '%s'."),
				LegacyObjectPath,
				*NewObjectPath));
			return nullptr;
		}
		if (!State.bApply)
		{
			State.AddError(FString::Printf(TEXT("Asset still requires semantic rename: %s"), LegacyObjectPath));
			return nullptr;
		}

		const FString NewAssetName = FPackageName::GetLongPackageAssetName(NewLongPackageName);
		const FString LegacyAssetName = LegacyAsset->GetName();
		UPackage* LegacyPackage = LegacyAsset->GetOutermost();
		ObjectTools::FPackageGroupName PackageGroupName;
		PackageGroupName.PackageName = NewLongPackageName;
		PackageGroupName.ObjectName = NewAssetName;
		TSet<UPackage*> PackagesUserRefusedToFullyLoad;
		FText RenameError;
		if (!ObjectTools::RenameSingleObject(
			LegacyAsset,
			PackageGroupName,
			PackagesUserRefusedToFullyLoad,
			RenameError,
			nullptr,
			true))
		{
			State.AddError(FString::Printf(
				TEXT("Could not rename '%s' to '%s': %s"),
				LegacyObjectPath,
				*NewObjectPath,
				*RenameError.ToString()));
			return nullptr;
		}
		State.MarkChanged(LegacyAsset);
		if (IsValid(LegacyPackage))
		{
			LegacyPackage->MarkPackageDirty();
			if (UObjectRedirector* Redirector = FindObject<UObjectRedirector>(LegacyPackage, *LegacyAssetName))
			{
				State.PackagesToSave.FindOrAdd(LegacyPackage) = Redirector;
			}
		}
		++State.RenamedAssetCount;
		return LegacyAsset;
	}

	template <typename TAsset>
	TArray<TAsset*> LoadAssetsUnder(const FName PackagePath)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(PackagePath);
		Filter.ClassPaths.Add(TAsset::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> AssetData;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, AssetData);
		AssetData.Sort([](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.GetObjectPathString() < Right.GetObjectPathString();
		});

		TArray<TAsset*> Assets;
		Assets.Reserve(AssetData.Num());
		for (const FAssetData& Entry : AssetData)
		{
			if (TAsset* Asset = Cast<TAsset>(Entry.GetAsset()))
			{
				Assets.Add(Asset);
			}
		}
		return Assets;
	}

	UWorld* LoadSolarSystemEditorWorld()
	{
		if (LoadedSolarSystemEditorWorld.IsValid())
		{
			return LoadedSolarSystemEditorWorld.Get();
		}
		const FString MapFilename = FPackageName::LongPackageNameToFilename(
			SolarSystemMapPackage,
			FPackageName::GetMapPackageExtension());
		LoadedSolarSystemEditorWorld = UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
		return LoadedSolarSystemEditorWorld.Get();
	}

	const TCHAR* LexToString(ESRFacilityOperationKind OperationKind)
	{
		switch (OperationKind)
		{
		case ESRFacilityOperationKind::Process:
			return TEXT("Transform");
		case ESRFacilityOperationKind::Synthesize:
			return TEXT("Synthesize");
		case ESRFacilityOperationKind::Separate:
			return TEXT("Separate");
		case ESRFacilityOperationKind::Mine:
			return TEXT("Mine");
		default:
			return TEXT("Unknown");
		}
	}

	void LogCurrentContentCatalog()
	{
		for (USRResourceDataAsset* Resource : LoadAssetsUnder<USRResourceDataAsset>(TEXT("/Game/StarRovers")))
		{
			if (!IsValid(Resource))
			{
				continue;
			}
			FString GlyphCounts;
			for (const FSRSourceGlyphCount& Entry : Resource->SourceGlyphCounts)
			{
				GlyphCounts += FString::Printf(TEXT("%d:%d,"), static_cast<int32>(Entry.Glyph), Entry.Count);
			}
			UE_LOG(LogTemp, Display,
				TEXT("Pattern catalog Resource Path=%s Id=%s Display=%s Glyphs=[%s] SeedSalt=%d"),
				*Resource->GetPathName(),
				*Resource->ResourceId.ToString(),
				*Resource->DisplayName.ToString(),
				*GlyphCounts,
				Resource->SourcePatternSeedSalt);
		}

		for (USRFacilityDataAsset* Facility : LoadAssetsUnder<USRFacilityDataAsset>(TEXT("/Game/StarRovers")))
		{
			if (!IsValid(Facility))
			{
				continue;
			}
			UE_LOG(LogTemp, Display,
				TEXT("Pattern catalog Facility Path=%s Id=%s Display=%s Kind=%d Operation=%s Rarity=%d Inputs=%d Outputs=%d InputCapacity=%d OutputCapacity=%d Seconds=%.2f"),
				*Facility->GetPathName(),
				*Facility->FacilityId.ToString(),
				*Facility->DisplayName.ToString(),
				static_cast<int32>(Facility->FacilityKind),
				LexToString(Facility->OperationKind),
				static_cast<int32>(Facility->Rarity),
				Facility->InputInventory.SlotCount,
				Facility->OutputInventory.SlotCount,
				Facility->InputInventory.SlotCapacity,
				Facility->OutputInventory.SlotCapacity,
				Facility->BaseProcessSeconds);
		}

		for (USRStructureDataAsset* Structure : LoadAssetsUnder<USRStructureDataAsset>(TEXT("/Game/StarRovers/Structure/DataAssets")))
		{
			if (!IsValid(Structure) || (!IsValid(Structure->FacilityDataAsset.Get()) && !Structure->bIsResourceDeposit))
			{
				continue;
			}
			UE_LOG(LogTemp, Display,
				TEXT("Pattern catalog Structure Path=%s Id=%s Display=%s Available=%s Facility=%s Deposit=%s InputPorts=%d OutputPorts=%d Footprint=%dx%d"),
				*Structure->GetPathName(),
				*Structure->StructureId.ToString(),
				*Structure->DisplayName.ToString(),
				Structure->bAvailableForConstruction ? TEXT("true") : TEXT("false"),
				*GetPathNameSafe(Structure->FacilityDataAsset.Get()),
				*GetPathNameSafe(Structure->DepositResourceDataAsset.Get()),
				Structure->InputPorts.Num(),
				Structure->OutputPorts.Num(),
				Structure->FootprintCellsX,
				Structure->FootprintCellsY);
		}

		for (USRPlanetDataAsset* Planet : LoadAssetsUnder<USRPlanetDataAsset>(TEXT("/Game/StarRovers/Celestial/DataAssets/Planets")))
		{
			const FSRPatternEnvironmentSpec Environment = IsValid(Planet->PatternEnvironmentDataAsset.Get())
				? Planet->PatternEnvironmentDataAsset->BuildEnvironmentSpec()
				: FSRPatternEnvironmentSpec();
			UE_LOG(LogTemp, Display,
				TEXT("Pattern catalog Environment Path=%s Variable=%s Id=%s Effects=%d"),
				*Planet->GetPathName(),
				*Planet->VariableName.ToString(),
				*Environment.EnvironmentId.ToString(),
				Environment.Effects.Num());
		}
		for (USRMoonDataAsset* Moon : LoadAssetsUnder<USRMoonDataAsset>(TEXT("/Game/StarRovers/Celestial/DataAssets/Moons")))
		{
			const FSRPatternEnvironmentSpec Environment = IsValid(Moon->PatternEnvironmentDataAsset.Get())
				? Moon->PatternEnvironmentDataAsset->BuildEnvironmentSpec()
				: FSRPatternEnvironmentSpec();
			UE_LOG(LogTemp, Display,
				TEXT("Pattern catalog Environment Path=%s Variable=%s Id=%s Effects=%d"),
				*Moon->GetPathName(),
				*Moon->VariableName.ToString(),
				*Environment.EnvironmentId.ToString(),
				Environment.Effects.Num());
		}
	}

	struct FPatternContentAssets
	{
		TObjectPtr<USRResourceDataAsset> StarIron = nullptr;
		TObjectPtr<USRResourceDataAsset> BloomSap = nullptr;
		TObjectPtr<USRResourceDataAsset> PrismShard = nullptr;
		TObjectPtr<USRResourceDataAsset> TidalOre = nullptr;
		TObjectPtr<USRResourceDataAsset> SolarMycelium = nullptr;
		TObjectPtr<USRResourceDataAsset> StellarWeave = nullptr;

		TObjectPtr<USRFacilityDataAsset> PatternExtractor = nullptr;
		TObjectPtr<USRFacilityDataAsset> PatternHub = nullptr;
		TObjectPtr<USRFacilityDataAsset> VectorEast = nullptr;
		TObjectPtr<USRFacilityDataAsset> VectorWest = nullptr;
		TObjectPtr<USRFacilityDataAsset> VectorNorth = nullptr;
		TObjectPtr<USRFacilityDataAsset> VectorSouth = nullptr;
		TObjectPtr<USRFacilityDataAsset> Centerline = nullptr;
		TObjectPtr<USRFacilityDataAsset> StellarLoom = nullptr;
		TObjectPtr<USRFacilityDataAsset> LatticeSeparator = nullptr;

		TObjectPtr<USRStructureDataAsset> PatternExtractorStructure = nullptr;
		TObjectPtr<USRStructureDataAsset> PatternHubStructure = nullptr;
		TObjectPtr<USRStructureDataAsset> VectorEastStructure = nullptr;
		TObjectPtr<USRStructureDataAsset> VectorWestStructure = nullptr;
		TObjectPtr<USRStructureDataAsset> VectorNorthStructure = nullptr;
		TObjectPtr<USRStructureDataAsset> VectorSouthStructure = nullptr;
		TObjectPtr<USRStructureDataAsset> CenterlineStructure = nullptr;
		TObjectPtr<USRStructureDataAsset> StellarLoomStructure = nullptr;
		TObjectPtr<USRStructureDataAsset> LatticeSeparatorStructure = nullptr;

		TObjectPtr<USRStructureDataAsset> StarIronDeposit = nullptr;
		TObjectPtr<USRStructureDataAsset> BloomSapDeposit = nullptr;
		TObjectPtr<USRStructureDataAsset> PrismShardDeposit = nullptr;
		TObjectPtr<USRStructureDataAsset> TidalOreDeposit = nullptr;
		TObjectPtr<USRStructureDataAsset> SolarMyceliumDeposit = nullptr;

		TObjectPtr<USRPlanetDataAsset> Gravemantle = nullptr;
		TObjectPtr<USRPlanetDataAsset> Cinderstream = nullptr;
		TObjectPtr<USRPlanetDataAsset> VerdantCradle = nullptr;
		TObjectPtr<USRMoonDataAsset> Ironwake = nullptr;
		TObjectPtr<USRPlanetShapeDataAsset> PlanetShape = nullptr;
		TObjectPtr<USRPlanetShapeDataAsset> MoonShape = nullptr;
		TObjectPtr<USRPlanetTerrainProfileDataAsset> PlanetTerrainProfile = nullptr;
		TObjectPtr<UMaterialInterface> BadlandsMaterial = nullptr;
		TObjectPtr<UMaterialInterface> CinderMaterial = nullptr;
		TObjectPtr<UMaterialInterface> VerdantMaterial = nullptr;
		TObjectPtr<UMaterialInterface> LavaOceanMaterial = nullptr;
		TObjectPtr<UMaterialInterface> WaterMaterial = nullptr;
		TObjectPtr<UMaterialInterface> AtmosphereMaterial = nullptr;

		TObjectPtr<USRPatternEnvironmentDataAsset> CrushingGravity = nullptr;
		TObjectPtr<USRPatternEnvironmentDataAsset> PlasmaJetstream = nullptr;
		TObjectPtr<USRPatternEnvironmentDataAsset> SporeBloom = nullptr;
		TObjectPtr<USRPatternEnvironmentDataAsset> MagneticShear = nullptr;
	};

	void LoadAndRenameCoreAssets(FPatternContentAssets& Assets, FPatternContentState& State)
	{
		Assets.StarIron = LoadRenamedAsset<USRResourceDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Resources/DA_Resource_Territe.DA_Resource_Territe"),
			StarIronResourcePackage, State);
		Assets.BloomSap = LoadRenamedAsset<USRResourceDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Resources/DA_Resource_Aquid.DA_Resource_Aquid"),
			BloomSapResourcePackage, State);
		Assets.PrismShard = LoadRenamedAsset<USRResourceDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Resources/DA_Resource_Nitain.DA_Resource_Nitain"),
			PrismShardResourcePackage, State);
		Assets.TidalOre = LoadRenamedAsset<USRResourceDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Resources/DA_Resource_Waste.DA_Resource_Waste"),
			TidalOreResourcePackage, State);
		Assets.SolarMycelium = LoadOrCreateAsset<USRResourceDataAsset>(SolarMyceliumResourcePackage, State);
		Assets.StellarWeave = LoadRenamedAsset<USRResourceDataAsset>(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Resources/DA_Resource_StellarComposite.DA_Resource_StellarComposite"),
			StellarWeaveResourcePackage, State);

		Assets.PatternExtractor = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/DA_Facility_Miner.DA_Facility_Miner"),
			PatternExtractorFacilityPackage, State);
		Assets.PatternHub = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/DA_Facility_Hub.DA_Facility_Hub"),
			PatternHubFacilityPackage, State);
		Assets.VectorEast = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/Starting/DA_Facility_SP1.DA_Facility_SP1"),
			VectorEastFacilityPackage, State);
		Assets.VectorWest = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/Basic/Basic_Processor/DA_Faciliity_BP1.DA_Faciliity_BP1"),
			VectorWestFacilityPackage, State);
		Assets.VectorNorth = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/Basic/Basic_Processor/DA_Faciliity_BP2.DA_Faciliity_BP2"),
			VectorNorthFacilityPackage, State);
		Assets.VectorSouth = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/Basic/Basic_Processor/DA_Faciliity_BP3.DA_Faciliity_BP3"),
			VectorSouthFacilityPackage, State);
		Assets.Centerline = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/Advanced/Advanced_Processor/DA_Facility_AP1.DA_Facility_AP1"),
			CenterlineFacilityPackage, State);
		Assets.StellarLoom = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/Basic/Basic_Synthesizer/DA_Facility_BS2.DA_Facility_BS2"),
			StellarLoomFacilityPackage, State);
		Assets.LatticeSeparator = LoadRenamedAsset<USRFacilityDataAsset>(
			TEXT("/Game/StarRovers/Automation/DataAssets/Facilities/Basic/Basic_Processor/DA_Faciliity_BP7.DA_Faciliity_BP7"),
			LatticeSeparatorFacilityPackage, State);

		Assets.PatternExtractorStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/DA_Structure_Miner.DA_Structure_Miner"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_PatternExtractor"), State);
		Assets.PatternHubStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/DA_Structure_Hub.DA_Structure_Hub"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_PatternHub"), State);
		Assets.VectorEastStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Starter/DA_Structure_SP1.DA_Structure_SP1"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_VectorShifterEast"), State);
		Assets.VectorWestStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Basic/Basic_Processor/DA_Structure_BP1.DA_Structure_BP1"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_VectorShifterWest"), State);
		Assets.VectorNorthStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Basic/Basic_Processor/DA_Structure_BP2.DA_Structure_BP2"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_VectorShifterNorth"), State);
		Assets.VectorSouthStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Basic/Basic_Processor/DA_Structure_BP3.DA_Structure_BP3"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_VectorShifterSouth"), State);
		Assets.CenterlineStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Advanced/Advanced_Processor/DA_Structure_AP1.DA_Structure_AP1"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_CenterlineShifter"), State);
		Assets.StellarLoomStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Basic/Basic_Synthesizer/DA_Structure_BS2.DA_Structure_BS2"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_StellarLoom"), State);
		Assets.LatticeSeparatorStructure = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Basic/Basic_Processor/DA_Structure_BP7.DA_Structure_BP7"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_LatticeSeparator"), State);

		Assets.StarIronDeposit = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_TerriteLode.DA_Structure_TerriteLode"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/Pattern/DA_Deposit_StarIron"), State);
		Assets.BloomSapDeposit = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_AquidLode.DA_Structure_AquidLode"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/Pattern/DA_Deposit_BloomSap"), State);
		Assets.PrismShardDeposit = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_NitainLode.DA_Structure_NitainLode"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/Pattern/DA_Deposit_PrismShard"), State);
		Assets.TidalOreDeposit = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_Rock1.DA_Structure_Rock1"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/Pattern/DA_Deposit_TidalOre"), State);
		Assets.SolarMyceliumDeposit = LoadRenamedAsset<USRStructureDataAsset>(
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_Tree1.DA_Structure_Tree1"),
			TEXT("/Game/StarRovers/Structure/DataAssets/Natural/Pattern/DA_Deposit_SolarMycelium"), State);

		Assets.Gravemantle = LoadRenamedAsset<USRPlanetDataAsset>(
			TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_BadLands.DA_Planet_BadLands"),
			TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_Gravemantle"), State);
		Assets.Cinderstream = LoadRenamedAsset<USRPlanetDataAsset>(
			TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_LavaOcean.DA_Planet_LavaOcean"),
			TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_Cinderstream"), State);
		Assets.VerdantCradle = LoadRenamedAsset<USRPlanetDataAsset>(
			TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_Temperate.DA_Planet_Temperate"),
			TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_VerdantCradle"), State);
		Assets.Ironwake = LoadRenamedAsset<USRMoonDataAsset>(
			TEXT("/Game/StarRovers/Celestial/DataAssets/Moons/DA_Moon_BadLands.DA_Moon_BadLands"),
			TEXT("/Game/StarRovers/Celestial/DataAssets/Moons/DA_Moon_Ironwake"), State);
		Assets.PlanetShape = LoadObject<USRPlanetShapeDataAsset>(nullptr, PlanetShapePath);
		Assets.MoonShape = LoadObject<USRPlanetShapeDataAsset>(nullptr, MoonShapePath);
		Assets.PlanetTerrainProfile = LoadObject<USRPlanetTerrainProfileDataAsset>(nullptr, PlanetTerrainProfilePath);
		Assets.BadlandsMaterial = LoadObject<UMaterialInterface>(nullptr, BadlandsMaterialPath);
		Assets.CinderMaterial = LoadObject<UMaterialInterface>(nullptr, CinderMaterialPath);
		Assets.VerdantMaterial = LoadObject<UMaterialInterface>(nullptr, VerdantMaterialPath);
		Assets.LavaOceanMaterial = LoadObject<UMaterialInterface>(nullptr, LavaOceanMaterialPath);
		Assets.WaterMaterial = LoadObject<UMaterialInterface>(nullptr, WaterMaterialPath);
		Assets.AtmosphereMaterial = LoadObject<UMaterialInterface>(nullptr, AtmosphereMaterialPath);
		if (!IsValid(Assets.PlanetShape)
			|| !IsValid(Assets.MoonShape)
			|| !IsValid(Assets.PlanetTerrainProfile)
			|| !IsValid(Assets.BadlandsMaterial)
			|| !IsValid(Assets.CinderMaterial)
			|| !IsValid(Assets.VerdantMaterial)
			|| !IsValid(Assets.LavaOceanMaterial)
			|| !IsValid(Assets.WaterMaterial)
			|| !IsValid(Assets.AtmosphereMaterial))
		{
			State.AddError(TEXT("The baked shapes, terrain profile, and celestial materials required by Pattern content are incomplete."));
		}

		Assets.CrushingGravity = LoadOrCreateAsset<USRPatternEnvironmentDataAsset>(CrushingGravityEnvironmentPackage, State);
		Assets.PlasmaJetstream = LoadOrCreateAsset<USRPatternEnvironmentDataAsset>(PlasmaJetstreamEnvironmentPackage, State);
		Assets.SporeBloom = LoadOrCreateAsset<USRPatternEnvironmentDataAsset>(SporeBloomEnvironmentPackage, State);
		Assets.MagneticShear = LoadOrCreateAsset<USRPatternEnvironmentDataAsset>(MagneticShearEnvironmentPackage, State);
	}

	FSRSourceGlyphCount MakeGlyphCount(ESRGlyphType Glyph, int32 Count)
	{
		FSRSourceGlyphCount Result;
		Result.Glyph = Glyph;
		Result.Count = Count;
		return Result;
	}

	void ConfigureResource(
		USRResourceDataAsset* Resource,
		FName ResourceId,
		const TCHAR* DisplayName,
		const TCHAR* Description,
		std::initializer_list<FSRSourceGlyphCount> GlyphCounts,
		int32 SeedSalt,
		FPatternContentState& State)
	{
		if (!IsValid(Resource))
		{
			return;
		}
		State.MarkChanged(Resource);
		if (!State.bApply)
		{
			return;
		}
		Resource->ResourceId = ResourceId;
		Resource->DisplayName = FText::FromString(DisplayName);
		Resource->Description = FText::FromString(Description);
		Resource->SourceGlyphCounts = GlyphCounts;
		Resource->SourcePatternSeedSalt = SeedSalt;
		Resource->DefaultPreviewPatternSeed = 1337 + SeedSalt;
	}

	void ConfigureResources(FPatternContentAssets& Assets, FPatternContentState& State)
	{
		ConfigureResource(
			Assets.StarIron,
			TEXT("StarIron"), TEXT("Star Iron"), TEXT("Dense stellar iron carrying reflective Crystal inclusions."),
			{ MakeGlyphCount(ESRGlyphType::Metal, 2), MakeGlyphCount(ESRGlyphType::Crystal, 2) }, 101, State);
		ConfigureResource(
			Assets.BloomSap,
			TEXT("BloomSap"), TEXT("Bloom Sap"), TEXT("A living fluid whose Organic colonies spread after each processing cycle."),
			{ MakeGlyphCount(ESRGlyphType::Organic, 2), MakeGlyphCount(ESRGlyphType::Fluid, 2) }, 211, State);
		ConfigureResource(
			Assets.PrismShard,
			TEXT("PrismShard"), TEXT("Prism Shard"), TEXT("Slippery Crystal fragments charged with volatile Plasma."),
			{ MakeGlyphCount(ESRGlyphType::Crystal, 2), MakeGlyphCount(ESRGlyphType::Plasma, 2) }, 307, State);
		ConfigureResource(
			Assets.TidalOre,
			TEXT("TidalOre"), TEXT("Tidal Ore"), TEXT("Heavy Metal nodules suspended in a deterministic Fluid matrix."),
			{ MakeGlyphCount(ESRGlyphType::Fluid, 2), MakeGlyphCount(ESRGlyphType::Metal, 2) }, 401, State);
		ConfigureResource(
			Assets.SolarMycelium,
			TEXT("SolarMycelium"), TEXT("Solar Mycelium"), TEXT("Organic filaments that contain jumping Plasma spores."),
			{ MakeGlyphCount(ESRGlyphType::Plasma, 2), MakeGlyphCount(ESRGlyphType::Organic, 2) }, 503, State);
		ConfigureResource(
			Assets.StellarWeave,
			TEXT("StellarWeave"), TEXT("Stellar Weave"), TEXT("The cargo identity assigned by a Stellar Loom without replacing the synthesized Pattern."),
			{
				MakeGlyphCount(ESRGlyphType::Metal, 1), MakeGlyphCount(ESRGlyphType::Organic, 1),
				MakeGlyphCount(ESRGlyphType::Crystal, 1), MakeGlyphCount(ESRGlyphType::Fluid, 1),
				MakeGlyphCount(ESRGlyphType::Plasma, 1)
			},
			607,
			State);
	}

	void ConfigureTransform(
		FSRPatternTransformOperatorSpec& Operator,
		ESRPatternDirection Direction,
		bool bCenterRowOnly,
		ESRPatternFluidSidePreference FluidSidePreference)
	{
		Operator.SelectionMask.Reset(!bCenterRowOnly);
		if (bCenterRowOnly)
		{
			for (int32 Column = 0; Column < StarRovers::Pattern::GridSize; ++Column)
			{
				Operator.SelectionMask.SetCellActive(StarRovers::Pattern::GridSize / 2, Column, true);
			}
		}
		Operator.Direction = Direction;
		Operator.FluidSidePreference = FluidSidePreference;
		Operator.OrganicGrowthsPerComponent = 1;
	}

	bool IsPatternFacilityValid(const USRFacilityDataAsset& Facility)
	{
		if (Facility.FacilityId.IsNone() || Facility.DisplayName.IsEmpty())
		{
			return false;
		}
		if (Facility.FacilityKind == ESRFacilityKind::Hub)
		{
			return Facility.InputInventory.SlotCount > 0
				&& Facility.OutputInventory.SlotCount > 0;
		}
		switch (Facility.OperationKind)
		{
		case ESRFacilityOperationKind::Process:
			return Facility.InputInventory.SlotCount >= 1
				&& Facility.OutputInventory.SlotCount >= 1
				&& FSRPatternFacilityResolver::IsValidTransformOperatorSpec(Facility.TransformOperator);
		case ESRFacilityOperationKind::Synthesize:
			return Facility.InputInventory.SlotCount >= 2
				&& Facility.OutputInventory.SlotCount >= 1
				&& IsValid(Facility.SynthesisOutputResource.Get());
		case ESRFacilityOperationKind::Separate:
			return Facility.InputInventory.SlotCount >= 1
				&& Facility.OutputInventory.SlotCount >= 2
				&& FSRPatternFacilityResolver::IsValidSeparationOperatorSpec(Facility.SeparationOperator);
		case ESRFacilityOperationKind::Mine:
			return Facility.OutputInventory.SlotCount >= 1;
		default:
			return false;
		}
	}

	void ConfigureFacilityIdentity(
		USRFacilityDataAsset* Facility,
		FName FacilityId,
		const TCHAR* DisplayName,
		const TCHAR* Description,
		ESRFacilityKind FacilityKind,
		ESRFacilityOperationKind OperationKind,
		ESRFacilityRarity Rarity,
		float ProcessSeconds,
		int32 InputSlots,
		int32 OutputSlots,
		int32 SlotCapacity,
		FPatternContentState& State)
	{
		if (!IsValid(Facility))
		{
			return;
		}
		State.MarkChanged(Facility);
		if (!State.bApply)
		{
			return;
		}
		Facility->FacilityId = FacilityId;
		Facility->DisplayName = FText::FromString(DisplayName);
		Facility->Description = FText::FromString(Description);
		Facility->FacilityKind = FacilityKind;
		Facility->OperationKind = OperationKind;
		Facility->Rarity = Rarity;
		Facility->BaseProcessSeconds = FMath::Max(0.25f, ProcessSeconds);
		Facility->InputInventory.SlotCount = FMath::Max(0, InputSlots);
		Facility->OutputInventory.SlotCount = FMath::Max(0, OutputSlots);
		Facility->InputInventory.SlotCapacity = FMath::Max(1, SlotCapacity);
		Facility->OutputInventory.SlotCapacity = FMath::Max(1, SlotCapacity);
		Facility->SynthesisOutputResource = nullptr;
	}

	void ConfigureFacilityStructure(
		USRStructureDataAsset* Structure,
		USRFacilityDataAsset* Facility,
		FName StructureId,
		const TCHAR* DisplayName,
		const TCHAR* Description,
		FPatternContentState& State)
	{
		if (!IsValid(Structure) || !IsValid(Facility))
		{
			return;
		}
		State.MarkChanged(Structure);
		if (!State.bApply)
		{
			return;
		}
		Structure->StructureId = StructureId;
		Structure->DisplayName = FText::FromString(DisplayName);
		Structure->Description = FText::FromString(Description);
		Structure->bAvailableForConstruction = true;
		Structure->bIsResourceDeposit = false;
		Structure->DepositResourceDataAsset = nullptr;
		Structure->FacilityDataAsset = Facility;
		for (FSRStructurePortSpec& Port : Structure->InputPorts)
		{
			Port.RoutingFilter = FSRPatternRoutingFilter();
		}
		for (FSRStructurePortSpec& Port : Structure->OutputPorts)
		{
			Port.RoutingFilter = FSRPatternRoutingFilter();
		}
	}

	void ConfigureFacilities(
		FPatternContentAssets& Assets,
		TArray<USRFacilityDataAsset*>& OutGenerationFacilities,
		TArray<FName>& OutFoundationUnlocks,
		TArray<FName>& OutExpansionUnlocks,
		FPatternContentState& State)
	{
		const bool bHasAllFacilityAssets =
			IsValid(Assets.PatternExtractor) && IsValid(Assets.PatternHub)
			&& IsValid(Assets.VectorEast) && IsValid(Assets.VectorWest)
			&& IsValid(Assets.VectorNorth) && IsValid(Assets.VectorSouth)
			&& IsValid(Assets.Centerline) && IsValid(Assets.StellarLoom)
			&& IsValid(Assets.LatticeSeparator)
			&& IsValid(Assets.PatternExtractorStructure) && IsValid(Assets.PatternHubStructure)
			&& IsValid(Assets.VectorEastStructure) && IsValid(Assets.VectorWestStructure)
			&& IsValid(Assets.VectorNorthStructure) && IsValid(Assets.VectorSouthStructure)
			&& IsValid(Assets.CenterlineStructure) && IsValid(Assets.StellarLoomStructure)
			&& IsValid(Assets.LatticeSeparatorStructure) && IsValid(Assets.StellarWeave);
		if (!bHasAllFacilityAssets)
		{
			State.AddError(TEXT("The semantic Pattern facility set is incomplete."));
			return;
		}
		ConfigureFacilityIdentity(Assets.PatternExtractor, TEXT("PatternExtractor"), TEXT("Pattern Extractor"),
			TEXT("Copies the mining point's fixed Pattern into the automation network."),
			ESRFacilityKind::Standard, ESRFacilityOperationKind::Mine, ESRFacilityRarity::Starting,
			2.5f, 0, 1, 8, State);
		ConfigureFacilityIdentity(Assets.PatternHub, TEXT("PatternHub"), TEXT("Pattern Hub"),
			TEXT("Buffers, filters, and dispatches Pattern cargo between surface and space routes."),
			ESRFacilityKind::Hub, ESRFacilityOperationKind::Process, ESRFacilityRarity::Starting,
			1.0f, 20, 20, 16, State);
		ConfigureFacilityIdentity(Assets.VectorEast, TEXT("VectorShifterEast"), TEXT("Vector Shifter East"),
			TEXT("Moves every selected glyph one command toward the east."),
			ESRFacilityKind::Standard, ESRFacilityOperationKind::Process, ESRFacilityRarity::Starting,
			2.0f, 1, 1, 8, State);
		ConfigureFacilityIdentity(Assets.VectorWest, TEXT("VectorShifterWest"), TEXT("Vector Shifter West"),
			TEXT("Moves every selected glyph one command toward the west."),
			ESRFacilityKind::Standard, ESRFacilityOperationKind::Process, ESRFacilityRarity::Starting,
			2.0f, 1, 1, 8, State);
		ConfigureFacilityIdentity(Assets.VectorNorth, TEXT("VectorShifterNorth"), TEXT("Vector Shifter North"),
			TEXT("Moves every selected glyph one command toward the north."),
			ESRFacilityKind::Standard, ESRFacilityOperationKind::Process, ESRFacilityRarity::Starting,
			2.0f, 1, 1, 8, State);
		ConfigureFacilityIdentity(Assets.VectorSouth, TEXT("VectorShifterSouth"), TEXT("Vector Shifter South"),
			TEXT("Moves every selected glyph one command toward the south."),
			ESRFacilityKind::Standard, ESRFacilityOperationKind::Process, ESRFacilityRarity::Starting,
			2.0f, 1, 1, 8, State);
		ConfigureFacilityIdentity(Assets.Centerline, TEXT("CenterlineShifter"), TEXT("Centerline Shifter"),
			TEXT("Moves only the center row east for fine Pattern correction."),
			ESRFacilityKind::Standard, ESRFacilityOperationKind::Process, ESRFacilityRarity::Advanced,
			1.5f, 1, 1, 6, State);
		ConfigureFacilityIdentity(Assets.StellarLoom, TEXT("StellarLoom"), TEXT("Stellar Loom"),
			TEXT("Synthesizes Star Iron and Bloom Sap into one Stellar Weave Pattern."),
			ESRFacilityKind::Standard, ESRFacilityOperationKind::Synthesize, ESRFacilityRarity::Basic,
			5.0f, 2, 1, 4, State);
		ConfigureFacilityIdentity(Assets.LatticeSeparator, TEXT("LatticeSeparator"), TEXT("Lattice Separator"),
			TEXT("Routes alternating Pattern columns into two non-duplicating outputs."),
			ESRFacilityKind::Standard, ESRFacilityOperationKind::Separate, ESRFacilityRarity::Advanced,
			3.5f, 1, 2, 6, State);

		if (State.bApply)
		{
			ConfigureTransform(Assets.VectorEast->TransformOperator, ESRPatternDirection::Right, false, ESRPatternFluidSidePreference::ClockwiseFirst);
			ConfigureTransform(Assets.VectorWest->TransformOperator, ESRPatternDirection::Left, false, ESRPatternFluidSidePreference::CounterClockwiseFirst);
			ConfigureTransform(Assets.VectorNorth->TransformOperator, ESRPatternDirection::Up, false, ESRPatternFluidSidePreference::CounterClockwiseFirst);
			ConfigureTransform(Assets.VectorSouth->TransformOperator, ESRPatternDirection::Down, false, ESRPatternFluidSidePreference::ClockwiseFirst);
			ConfigureTransform(Assets.Centerline->TransformOperator, ESRPatternDirection::Right, true, ESRPatternFluidSidePreference::ClockwiseFirst);
			Assets.StellarLoom->SynthesisOutputResource = Assets.StellarWeave;
			Assets.LatticeSeparator->SeparationOperator.PrimaryOutputMask.Reset(false);
			for (int32 Row = 0; Row < StarRovers::Pattern::GridSize; ++Row)
			{
				for (int32 Column = 0; Column < StarRovers::Pattern::GridSize; Column += 2)
				{
					Assets.LatticeSeparator->SeparationOperator.PrimaryOutputMask.SetCellActive(Row, Column, true);
				}
			}
		}

		ConfigureFacilityStructure(Assets.PatternExtractorStructure, Assets.PatternExtractor, TEXT("PatternExtractor"), TEXT("Pattern Extractor"),
			TEXT("Mines a fixed source Pattern without rerolling its cells."), State);
		ConfigureFacilityStructure(Assets.PatternHubStructure, Assets.PatternHub, TEXT("PatternHub"), TEXT("Pattern Hub"),
			TEXT("Central Pattern buffer and inter-body dispatch hub."), State);
		ConfigureFacilityStructure(Assets.VectorEastStructure, Assets.VectorEast, TEXT("VectorShifterEast"), TEXT("Vector Shifter East"),
			TEXT("Coarse whole-board eastward alignment."), State);
		ConfigureFacilityStructure(Assets.VectorWestStructure, Assets.VectorWest, TEXT("VectorShifterWest"), TEXT("Vector Shifter West"),
			TEXT("Coarse whole-board westward alignment."), State);
		ConfigureFacilityStructure(Assets.VectorNorthStructure, Assets.VectorNorth, TEXT("VectorShifterNorth"), TEXT("Vector Shifter North"),
			TEXT("Coarse whole-board northward alignment."), State);
		ConfigureFacilityStructure(Assets.VectorSouthStructure, Assets.VectorSouth, TEXT("VectorShifterSouth"), TEXT("Vector Shifter South"),
			TEXT("Coarse whole-board southward alignment."), State);
		ConfigureFacilityStructure(Assets.CenterlineStructure, Assets.Centerline, TEXT("CenterlineShifter"), TEXT("Centerline Shifter"),
			TEXT("Fast center-row correction for contract finishing."), State);
		ConfigureFacilityStructure(Assets.StellarLoomStructure, Assets.StellarLoom, TEXT("StellarLoom"), TEXT("Stellar Loom"),
			TEXT("Combines two world-specific source Patterns into Stellar Weave."), State);
		ConfigureFacilityStructure(Assets.LatticeSeparatorStructure, Assets.LatticeSeparator, TEXT("LatticeSeparator"), TEXT("Lattice Separator"),
			TEXT("Splits alternating columns for recovery and branch routing."), State);

		if (State.bApply)
		{
			if (Assets.StellarLoomStructure->InputPorts.Num() >= 2)
			{
				Assets.StellarLoomStructure->InputPorts[0].RoutingFilter.ResourceId = TEXT("StarIron");
				Assets.StellarLoomStructure->InputPorts[1].RoutingFilter.ResourceId = TEXT("BloomSap");
			}
			if (!Assets.LatticeSeparatorStructure->InputPorts.IsEmpty())
			{
				Assets.LatticeSeparatorStructure->InputPorts[0].RoutingFilter.ResourceId = TEXT("StellarWeave");
			}
		}

		// Generation must be solvable from the Foundation technology alone. Expansion
		// facilities remain in the construction catalog but are not assumed here.
		OutGenerationFacilities = {
			Assets.VectorEast, Assets.VectorWest, Assets.VectorNorth, Assets.VectorSouth,
			Assets.StellarLoom
		};
		OutFoundationUnlocks = {
			Assets.PatternExtractorStructure->StructureId,
			Assets.PatternHubStructure->StructureId,
			Assets.VectorEastStructure->StructureId,
			Assets.VectorWestStructure->StructureId,
			Assets.VectorNorthStructure->StructureId,
			Assets.VectorSouthStructure->StructureId,
			Assets.StellarLoomStructure->StructureId
		};
		if (USRStructureDataAsset* Conveyor = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/DA_Structure_ConveyorBelt.DA_Structure_ConveyorBelt")))
		{
			OutFoundationUnlocks.AddUnique(Conveyor->StructureId);
		}
		OutExpansionUnlocks = {
			Assets.CenterlineStructure->StructureId,
			Assets.LatticeSeparatorStructure->StructureId
		};
		OutFoundationUnlocks.Sort(FNameLexicalLess());
		OutExpansionUnlocks.Sort(FNameLexicalLess());
	}

	FSRPatternEnvironmentEffectSpec MakeEnvironmentEffect(
		ESRPatternEnvironmentEffectKind Kind,
		ESRPatternDirection Direction,
		ESRGlyphType Glyph,
		int32 Intensity)
	{
		FSRPatternEnvironmentEffectSpec Effect;
		Effect.EffectKind = Kind;
		Effect.Direction = Direction;
		Effect.AffectedGlyph = Glyph;
		Effect.Distance = FMath::Clamp(Intensity, 1, 4);
		Effect.MaxDriftSteps = FMath::Clamp(Intensity, 1, 4);
		Effect.OrganicGrowthsPerComponent = FMath::Clamp(Intensity, 1, 4);
		return Effect;
	}

	void ConfigureEnvironmentDataAsset(
		USRPatternEnvironmentDataAsset* Environment,
		FName EnvironmentId,
		const TCHAR* DisplayName,
		const TCHAR* Description,
		const FSRPatternEnvironmentEffectSpec& Effect,
		FPatternContentState& State)
	{
		if (!IsValid(Environment))
		{
			return;
		}
		State.MarkChanged(Environment);
		if (!State.bApply)
		{
			return;
		}
		Environment->EnvironmentId = EnvironmentId;
		Environment->DisplayName = FText::FromString(DisplayName);
		Environment->Description = FText::FromString(Description);
		Environment->Effects = { Effect };
	}

	void ConfigureEnvironments(FPatternContentAssets& Assets, FPatternContentState& State)
	{
		ConfigureEnvironmentDataAsset(
			Assets.CrushingGravity,
			TEXT("CrushingGravity"), TEXT("Crushing Gravity"),
			TEXT("Dense Metal is pulled one cell downward after processing while other glyphs retain their position."),
			MakeEnvironmentEffect(ESRPatternEnvironmentEffectKind::DirectionalPull, ESRPatternDirection::Down, ESRGlyphType::Metal, 1),
			State);
		ConfigureEnvironmentDataAsset(
			Assets.PlasmaJetstream,
			TEXT("PlasmaJetstream"), TEXT("Plasma Jetstream"),
			TEXT("Plasma continuously drifts east for up to two steps after processing."),
			MakeEnvironmentEffect(ESRPatternEnvironmentEffectKind::ContinuousDrift, ESRPatternDirection::Right, ESRGlyphType::Plasma, 2),
			State);
		ConfigureEnvironmentDataAsset(
			Assets.SporeBloom,
			TEXT("SporeBloom"), TEXT("Spore Bloom"),
			TEXT("Every Organic component attempts one additional growth after processing."),
			MakeEnvironmentEffect(ESRPatternEnvironmentEffectKind::OrganicBloom, ESRPatternDirection::Down, ESRGlyphType::Organic, 1),
			State);
		ConfigureEnvironmentDataAsset(
			Assets.MagneticShear,
			TEXT("MagneticShear"), TEXT("Magnetic Shear"),
			TEXT("Metal is pulled two cells west after processing while other glyphs retain their position."),
			MakeEnvironmentEffect(ESRPatternEnvironmentEffectKind::DirectionalPull, ESRPatternDirection::Left, ESRGlyphType::Metal, 2),
			State);

		if (!State.bApply)
		{
			return;
		}
		if (IsValid(Assets.Gravemantle))
		{
			State.MarkChanged(Assets.Gravemantle);
			Assets.Gravemantle->VariableName = FText::FromString(TEXT("Gravemantle"));
			Assets.Gravemantle->Scale = 22.0f;
			Assets.Gravemantle->Mass = 340.0f;
			Assets.Gravemantle->GravityRatio = 1.8f;
			Assets.Gravemantle->ShapeDataAsset = Assets.PlanetShape;
			Assets.Gravemantle->Material = Assets.BadlandsMaterial;
			Assets.Gravemantle->TerrainProfileDataAsset = Assets.PlanetTerrainProfile;
			Assets.Gravemantle->bHasOcean = false;
			Assets.Gravemantle->OceanMaterial = nullptr;
			Assets.Gravemantle->bHasAtmosphere = false;
			Assets.Gravemantle->AtmosphereMaterial = nullptr;
			Assets.Gravemantle->PatternEnvironmentDataAsset = Assets.CrushingGravity;
			Assets.Gravemantle->DynamicMeshGeneration.bRandomizeGenerationSeedEachRun = false;
		}
		if (IsValid(Assets.Cinderstream))
		{
			State.MarkChanged(Assets.Cinderstream);
			Assets.Cinderstream->VariableName = FText::FromString(TEXT("Cinderstream"));
			Assets.Cinderstream->Scale = 20.0f;
			Assets.Cinderstream->Mass = 220.0f;
			Assets.Cinderstream->GravityRatio = 1.1f;
			Assets.Cinderstream->ShapeDataAsset = Assets.PlanetShape;
			Assets.Cinderstream->Material = Assets.CinderMaterial;
			Assets.Cinderstream->TerrainProfileDataAsset = Assets.PlanetTerrainProfile;
			Assets.Cinderstream->bHasOcean = true;
			Assets.Cinderstream->OceanMaterial = Assets.LavaOceanMaterial;
			Assets.Cinderstream->bHasAtmosphere = true;
			Assets.Cinderstream->AtmosphereMaterial = Assets.AtmosphereMaterial;
			Assets.Cinderstream->PatternEnvironmentDataAsset = Assets.PlasmaJetstream;
			Assets.Cinderstream->DynamicMeshGeneration.bRandomizeGenerationSeedEachRun = false;
		}
		if (IsValid(Assets.VerdantCradle))
		{
			State.MarkChanged(Assets.VerdantCradle);
			Assets.VerdantCradle->VariableName = FText::FromString(TEXT("Verdant Cradle"));
			Assets.VerdantCradle->Scale = 20.0f;
			Assets.VerdantCradle->Mass = 180.0f;
			Assets.VerdantCradle->GravityRatio = 0.9f;
			Assets.VerdantCradle->ShapeDataAsset = Assets.PlanetShape;
			Assets.VerdantCradle->Material = Assets.VerdantMaterial;
			Assets.VerdantCradle->TerrainProfileDataAsset = Assets.PlanetTerrainProfile;
			Assets.VerdantCradle->bHasOcean = true;
			Assets.VerdantCradle->OceanMaterial = Assets.WaterMaterial;
			Assets.VerdantCradle->bHasAtmosphere = true;
			Assets.VerdantCradle->AtmosphereMaterial = Assets.AtmosphereMaterial;
			Assets.VerdantCradle->PatternEnvironmentDataAsset = Assets.SporeBloom;
			Assets.VerdantCradle->DynamicMeshGeneration.bRandomizeGenerationSeedEachRun = false;
		}
		if (IsValid(Assets.Ironwake))
		{
			State.MarkChanged(Assets.Ironwake);
			Assets.Ironwake->VariableName = FText::FromString(TEXT("Ironwake"));
			Assets.Ironwake->Scale = 6.0f;
			Assets.Ironwake->Mass = 45.0f;
			Assets.Ironwake->GravityRatio = 0.45f;
			Assets.Ironwake->ShapeDataAsset = Assets.MoonShape;
			Assets.Ironwake->Material = Assets.BadlandsMaterial;
			Assets.Ironwake->TerrainProfileDataAsset = Assets.PlanetTerrainProfile;
			Assets.Ironwake->bHasOcean = false;
			Assets.Ironwake->OceanMaterial = nullptr;
			Assets.Ironwake->bHasAtmosphere = false;
			Assets.Ironwake->AtmosphereMaterial = nullptr;
			Assets.Ironwake->PatternEnvironmentDataAsset = Assets.MagneticShear;
			Assets.Ironwake->DynamicMeshGeneration.bRandomizeGenerationSeedEachRun = false;
		}
	}

	void ConfigureDeposit(
		USRStructureDataAsset* Deposit,
		USRResourceDataAsset* Resource,
		FName StructureId,
		const TCHAR* DisplayName,
		const TCHAR* Description,
		int32 TotalAmount,
		FPatternContentState& State)
	{
		if (!IsValid(Deposit) || !IsValid(Resource))
		{
			return;
		}
		State.MarkChanged(Deposit);
		if (!State.bApply)
		{
			return;
		}
		Deposit->StructureId = StructureId;
		Deposit->DisplayName = FText::FromString(DisplayName);
		Deposit->Description = FText::FromString(Description);
		Deposit->FacilityDataAsset = nullptr;
		Deposit->bIsResourceDeposit = true;
		Deposit->DepositResourceDataAsset = Resource;
		Deposit->DepositTotalAmount = FMath::Max(1, TotalAmount);
		Deposit->InputPorts.Reset();
		Deposit->OutputPorts.Reset();
	}

	FSRProfileNaturalStructureSpawnRule MakeSpawnRule(
		FName RuleId,
		USRStructureDataAsset* Structure,
		float Chance,
		int32 MaxCount,
		int32 Spacing)
	{
		FSRProfileNaturalStructureSpawnRule Rule;
		Rule.RuleId = RuleId;
		Rule.bEnabled = true;
		Rule.StructureDataAsset = Structure;
		Rule.SpawnChancePerCell = Chance;
		Rule.MaxCount = MaxCount;
		Rule.MinCellSpacing = Spacing;
		return Rule;
	}

	FSRNaturalStructureSpawnRuleOverride MakeSpawnOverride(
		FName RuleId,
		bool bEnabled,
		float Chance,
		int32 MaxCount,
		int32 Spacing)
	{
		FSRNaturalStructureSpawnRuleOverride Override;
		Override.RuleId = RuleId;
		Override.bEnabled = bEnabled;
		Override.SpawnChancePerCell = Chance;
		Override.MaxCount = MaxCount;
		Override.MinCellSpacing = Spacing;
		return Override;
	}

	void ConfigureDepositsAndDistribution(FPatternContentAssets& Assets, FPatternContentState& State)
	{
		const bool bHasAllDistributionAssets =
			IsValid(Assets.StarIron) && IsValid(Assets.BloomSap)
			&& IsValid(Assets.PrismShard) && IsValid(Assets.TidalOre)
			&& IsValid(Assets.SolarMycelium)
			&& IsValid(Assets.StarIronDeposit) && IsValid(Assets.BloomSapDeposit)
			&& IsValid(Assets.PrismShardDeposit) && IsValid(Assets.TidalOreDeposit)
			&& IsValid(Assets.SolarMyceliumDeposit)
			&& IsValid(Assets.Gravemantle) && IsValid(Assets.Cinderstream)
			&& IsValid(Assets.VerdantCradle) && IsValid(Assets.Ironwake);
		if (!bHasAllDistributionAssets)
		{
			State.AddError(TEXT("The Pattern resource distribution set is incomplete."));
			return;
		}
		ConfigureDeposit(Assets.StarIronDeposit, Assets.StarIron, TEXT("DepositStarIron"), TEXT("Star Iron Seam"),
			TEXT("A finite mining point containing one fixed Metal and Crystal Pattern."), 160, State);
		ConfigureDeposit(Assets.BloomSapDeposit, Assets.BloomSap, TEXT("DepositBloomSap"), TEXT("Bloom Sap Colony"),
			TEXT("A finite mining point containing one fixed Organic and Fluid Pattern."), 140, State);
		ConfigureDeposit(Assets.PrismShardDeposit, Assets.PrismShard, TEXT("DepositPrismShard"), TEXT("Prism Shard Cluster"),
			TEXT("A finite mining point containing one fixed Crystal and Plasma Pattern."), 120, State);
		ConfigureDeposit(Assets.TidalOreDeposit, Assets.TidalOre, TEXT("DepositTidalOre"), TEXT("Tidal Ore Vent"),
			TEXT("A finite mining point containing one fixed Fluid and Metal Pattern."), 110, State);
		ConfigureDeposit(Assets.SolarMyceliumDeposit, Assets.SolarMycelium, TEXT("DepositSolarMycelium"), TEXT("Solar Mycelium Grove"),
			TEXT("A finite mining point containing one fixed Plasma and Organic Pattern."), 100, State);

		if (!State.bApply)
		{
			return;
		}

		const TSet<TObjectPtr<USRStructureDataAsset>> PatternDeposits = {
			Assets.StarIronDeposit, Assets.BloomSapDeposit, Assets.PrismShardDeposit,
			Assets.TidalOreDeposit, Assets.SolarMyceliumDeposit
		};
		for (USRPlanetBiomeDataAsset* Biome : LoadAssetsUnder<USRPlanetBiomeDataAsset>(TEXT("/Game/StarRovers/Surface/DataAssets/Biomes")))
		{
			if (!IsValid(Biome))
			{
				continue;
			}
			State.MarkChanged(Biome);
			Biome->NaturalStructureSpawnRules.RemoveAll([&PatternDeposits](const FSRProfileNaturalStructureSpawnRule& Rule)
			{
				return PatternDeposits.Contains(Rule.StructureDataAsset);
			});
		}

		USRPlanetTerrainProfileDataAsset* TerrainProfile = LoadObject<USRPlanetTerrainProfileDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Surface/DataAssets/TerrainProfiles/DA_Profile_Earth.DA_Profile_Earth"));
		if (!IsValid(TerrainProfile))
		{
			State.AddError(TEXT("Could not load the terrain profile used for Pattern resource distribution."));
			return;
		}
		State.MarkChanged(TerrainProfile);
		TerrainProfile->ProfileNaturalStructureSpawnRules.RemoveAll([&PatternDeposits](const FSRProfileNaturalStructureSpawnRule& Rule)
		{
			return PatternDeposits.Contains(Rule.StructureDataAsset)
				|| (IsValid(Rule.StructureDataAsset.Get()) && Rule.StructureDataAsset->bIsResourceDeposit);
		});
		TerrainProfile->ProfileNaturalStructureSpawnRules.Append({
			MakeSpawnRule(TEXT("SpawnStarIron"), Assets.StarIronDeposit, 0.020f, 8, 3),
			MakeSpawnRule(TEXT("SpawnBloomSap"), Assets.BloomSapDeposit, 0.020f, 8, 3),
			MakeSpawnRule(TEXT("SpawnPrismShard"), Assets.PrismShardDeposit, 0.020f, 8, 3),
			MakeSpawnRule(TEXT("SpawnTidalOre"), Assets.TidalOreDeposit, 0.015f, 6, 3),
			MakeSpawnRule(TEXT("SpawnSolarMycelium"), Assets.SolarMyceliumDeposit, 0.015f, 6, 3)
		});

		const auto BuildOverrides = [](bool bStarIron, bool bBloomSap, bool bPrismShard, bool bTidalOre, bool bSolarMycelium)
		{
			return TArray<FSRNaturalStructureSpawnRuleOverride>{
				MakeSpawnOverride(TEXT("SpawnStarIron"), bStarIron, 0.035f, 10, 3),
				MakeSpawnOverride(TEXT("SpawnBloomSap"), bBloomSap, 0.035f, 10, 2),
				MakeSpawnOverride(TEXT("SpawnPrismShard"), bPrismShard, 0.030f, 8, 3),
				MakeSpawnOverride(TEXT("SpawnTidalOre"), bTidalOre, 0.020f, 6, 3),
				MakeSpawnOverride(TEXT("SpawnSolarMycelium"), bSolarMycelium, 0.020f, 6, 2)
			};
		};
		Assets.Gravemantle->ProfileNaturalStructureSpawnRuleOverrides = BuildOverrides(true, false, false, true, false);
		Assets.Cinderstream->ProfileNaturalStructureSpawnRuleOverrides = BuildOverrides(false, false, true, false, true);
		Assets.VerdantCradle->ProfileNaturalStructureSpawnRuleOverrides = BuildOverrides(false, true, false, false, true);
		// Magnetic Shear is Metal-specific, so every enabled Ironwake deposit must
		// contain Metal. Tidal Ore supplies the secondary Fluid glyph without
		// breaking the environment-to-deposit guarantee.
		Assets.Ironwake->ProfileNaturalStructureSpawnRuleOverrides = BuildOverrides(true, false, false, true, false);
	}

	FSRPatternHandRule MakeSameGlyphHand(
		FName RuleId,
		const TCHAR* DisplayName,
		ESRPatternHandRarity Rarity,
		int32 BonusScore,
		std::initializer_list<FIntPoint> Cells)
	{
		FSRPatternHandRule Rule;
		Rule.RuleId = RuleId;
		Rule.DisplayName = FText::FromString(DisplayName);
		Rule.RuleKind = ESRPatternHandRuleKind::SameGlyphShape;
		Rule.Rarity = Rarity;
		Rule.TransformPolicy = ESRPatternHandTransformPolicy::RotateAndTranslate;
		Rule.BonusScore = BonusScore;
		Rule.MaximumMatches = 2;
		Rule.ShapeMask.Reset(false);
		for (const FIntPoint& Cell : Cells)
		{
			Rule.ShapeMask.SetCellActive(Cell.Y, Cell.X, true);
		}
		return Rule;
	}

	struct FStellarContractCell
	{
		int32 Row = 0;
		int32 Column = 0;
		ESRGlyphType Glyph = ESRGlyphType::Empty;
	};

	FSRStellarPatternContract MakeStellarPatternContract(
		FName ContractId,
		std::initializer_list<FStellarContractCell> RequiredCells)
	{
		FSRStellarPatternContract Contract;
		Contract.ContractId = ContractId;
		Contract.RequiredPattern.Reset();
		Contract.RequiredMask.Reset(false);
		for (const FStellarContractCell& Cell : RequiredCells)
		{
			Contract.RequiredPattern.SetGlyph(Cell.Row, Cell.Column, Cell.Glyph);
			Contract.RequiredMask.SetCellActive(Cell.Row, Cell.Column, true);
		}
		const int32 RequiredCellCount = static_cast<int32>(RequiredCells.size());
		Contract.BaseScorePerPattern = 8 + (RequiredCellCount * 2);
		Contract.RequiredScorePerCycle = Contract.BaseScorePerPattern * 4;
		Contract.RequiredScoreGrowthPerCycle = FMath::Max(3, RequiredCellCount);
		Contract.StellarHealthMaximum = 1000.0;
		Contract.StartingStellarHealth = 1000.0;
		Contract.InitialStellarHealthDecreasePerSecond = 0.25;
		Contract.StellarHealthDecreaseMultiplierPerPeriod = 1.05;
		Contract.StellarHealthRestoredPerPatternScore = 1.0;
		Contract.BonusRules = {
			MakeSameGlyphHand(TEXT("ThreeInLine"), TEXT("Three in a Line"), ESRPatternHandRarity::Uncommon, 6,
				{ FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(2, 0) }),
			MakeSameGlyphHand(TEXT("SolidBlock"), TEXT("Solid 2x2 Block"), ESRPatternHandRarity::Rare, 10,
				{ FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(1, 1) })
		};
		return Contract;
	}

	void ConfigureContractAndProfile(
		const TArray<USRFacilityDataAsset*>& Facilities,
		USRPatternGenerationProfileDataAsset*& OutProfile,
		FPatternContentState& State)
	{
		TArray<FSRStellarPatternContract> Contracts = {
			MakeStellarPatternContract(TEXT("MetalOrganicEast"), {
				{ 2, 2, ESRGlyphType::Metal }, { 2, 3, ESRGlyphType::Organic } }),
			MakeStellarPatternContract(TEXT("MetalOrganicWest"), {
				{ 2, 2, ESRGlyphType::Metal }, { 2, 1, ESRGlyphType::Organic } }),
			MakeStellarPatternContract(TEXT("MetalOrganicNorth"), {
				{ 2, 2, ESRGlyphType::Metal }, { 1, 2, ESRGlyphType::Organic } }),
			MakeStellarPatternContract(TEXT("MetalOrganicSouth"), {
				{ 2, 2, ESRGlyphType::Metal }, { 3, 2, ESRGlyphType::Organic } }),
			MakeStellarPatternContract(TEXT("MetalOrganicNorthEast"), {
				{ 2, 2, ESRGlyphType::Metal }, { 1, 3, ESRGlyphType::Organic } }),
			MakeStellarPatternContract(TEXT("MetalOrganicNorthWest"), {
				{ 2, 2, ESRGlyphType::Metal }, { 1, 1, ESRGlyphType::Organic } }),
			MakeStellarPatternContract(TEXT("MetalOrganicSouthEast"), {
				{ 2, 2, ESRGlyphType::Metal }, { 3, 3, ESRGlyphType::Organic } }),
			MakeStellarPatternContract(TEXT("MetalOrganicSouthWest"), {
				{ 2, 2, ESRGlyphType::Metal }, { 3, 1, ESRGlyphType::Organic } }),
			MakeStellarPatternContract(TEXT("MetalPlasmaEast"), {
				{ 2, 2, ESRGlyphType::Metal }, { 2, 3, ESRGlyphType::Plasma } }),
			MakeStellarPatternContract(TEXT("MetalPlasmaWest"), {
				{ 2, 2, ESRGlyphType::Metal }, { 2, 1, ESRGlyphType::Plasma } }),
			MakeStellarPatternContract(TEXT("MetalPlasmaNorth"), {
				{ 2, 2, ESRGlyphType::Metal }, { 1, 2, ESRGlyphType::Plasma } }),
			MakeStellarPatternContract(TEXT("MetalPlasmaSouth"), {
				{ 2, 2, ESRGlyphType::Metal }, { 3, 2, ESRGlyphType::Plasma } }),
			MakeStellarPatternContract(TEXT("CrystalFluidWest"), {
				{ 2, 0, ESRGlyphType::Crystal }, { 2, 1, ESRGlyphType::Fluid } }),
			MakeStellarPatternContract(TEXT("CrystalFluidEast"), {
				{ 2, 4, ESRGlyphType::Crystal }, { 2, 3, ESRGlyphType::Fluid } }),
			MakeStellarPatternContract(TEXT("CrystalFluidNorth"), {
				{ 0, 2, ESRGlyphType::Crystal }, { 1, 2, ESRGlyphType::Fluid } }),
			MakeStellarPatternContract(TEXT("CrystalFluidSouth"), {
				{ 4, 2, ESRGlyphType::Crystal }, { 3, 2, ESRGlyphType::Fluid } })
		};

		USRStarDataAsset* Star = LoadObject<USRStarDataAsset>(nullptr, TEXT("/Game/StarRovers/Celestial/DataAssets/Stars/DA_Star_MainSequenceStar.DA_Star_MainSequenceStar"));
		if (!Star)
		{
			State.AddError(TEXT("Could not load the primary Star Data Asset."));
		}
		else
		{
			State.MarkChanged(Star);
			if (State.bApply)
			{
				Star->DefaultStellarPatternContract = Contracts[0];
				Star->bRandomizeGenerationSeedEachRun = false;
			}
		}

		OutProfile = LoadOrCreateAsset<USRPatternGenerationProfileDataAsset>(GenerationProfilePackage, State);
		if (OutProfile)
		{
			State.MarkChanged(OutProfile);
			if (State.bApply)
			{
				OutProfile->CandidateStellarContracts = Contracts;
				OutProfile->AvailableFacilityDataAssets.Reset();
				for (USRFacilityDataAsset* Facility : Facilities)
				{
					OutProfile->AvailableFacilityDataAssets.AddUnique(Facility);
				}
				OutProfile->MaxOperationDepth = 8;
				OutProfile->MaxReachableStates = 8192;
				OutProfile->MaxValidationSourcesPerResourcePerBody = 1;
				OutProfile->bRequireInterBodyTransfer = true;
			}
		}
	}

	FSRRunModifierEffect MakeModifier(
		FName EffectId,
		ESRRunModifierEffectKind Kind,
		double Magnitude,
		ESRRunModifierFacilityScope Scope = ESRRunModifierFacilityScope::Any,
		ESRGlyphType Glyph = ESRGlyphType::Empty,
		FName ContractId = NAME_None)
	{
		FSRRunModifierEffect Effect;
		Effect.EffectId = EffectId;
		Effect.EffectKind = Kind;
		Effect.Magnitude = Magnitude;
		Effect.FacilityScope = Scope;
		Effect.AffectedGlyph = Glyph;
		Effect.ContractId = ContractId;
		return Effect;
	}

	USRTechnologyDataAsset* ConfigureTechnology(
		const TCHAR* PackageName,
		FName Id,
		const TCHAR* DisplayName,
		bool bDefault,
		const TArray<FName>& Unlocks,
		const TArray<FName>& Prerequisites,
		FPatternContentState& State)
	{
		USRTechnologyDataAsset* Asset = LoadOrCreateAsset<USRTechnologyDataAsset>(PackageName, State);
		if (Asset)
		{
			State.MarkChanged(Asset);
			if (State.bApply)
			{
				Asset->TechnologyId = Id;
				Asset->DisplayName = FText::FromString(DisplayName);
				Asset->Description = FText::FromString(TEXT("Guaranteed Pattern automation infrastructure."));
				Asset->bUnlockedByDefault = bDefault;
				Asset->UnlockedStructureIds = Unlocks;
				Asset->PrerequisiteTechnologyIds = Prerequisites;
				Asset->Effects.Reset();
			}
		}
		return Asset;
	}

	USRRunAugmentDataAsset* ConfigureAugment(
		const TCHAR* PackageName,
		FName Id,
		const TCHAR* DisplayName,
		ESRRunAugmentRarity Rarity,
		ESRRunAugmentOfferRole Role,
		int32 StackCap,
		std::initializer_list<FSRRunModifierEffect> Effects,
		FPatternContentState& State)
	{
		USRRunAugmentDataAsset* Asset = LoadOrCreateAsset<USRRunAugmentDataAsset>(PackageName, State);
		if (Asset)
		{
			State.MarkChanged(Asset);
			if (State.bApply)
			{
				Asset->AugmentId = Id;
				Asset->DisplayName = FText::FromString(DisplayName);
				Asset->Description = FText::FromString(TEXT("Changes which Pattern automation line is most efficient."));
				Asset->Rarity = Rarity;
				Asset->OfferRole = Role;
				Asset->MaximumStacks = StackCap;
				Asset->Effects = Effects;
			}
		}
		return Asset;
	}

	USRTrialDataAsset* ConfigureTrial(
		const TCHAR* PackageName,
		FName Id,
		const TCHAR* DisplayName,
		int32 Duration,
		std::initializer_list<FSRRunModifierEffect> Effects,
		FPatternContentState& State)
	{
		USRTrialDataAsset* Asset = LoadOrCreateAsset<USRTrialDataAsset>(PackageName, State);
		if (Asset)
		{
			State.MarkChanged(Asset);
			if (State.bApply)
			{
				Asset->TrialId = Id;
				Asset->DisplayName = FText::FromString(DisplayName);
				Asset->Description = FText::FromString(TEXT("A temporary risk/reward automation constraint."));
				Asset->DurationCycles = Duration;
				Asset->Effects = Effects;
			}
		}
		return Asset;
	}

	void ConfigureRunModifiers(
		const TArray<FName>& FoundationUnlocks,
		const TArray<FName>& ExpansionUnlocks,
		FPatternContentState& State)
	{
		ConfigureTechnology(
			FoundationTechnologyPackage,
			TEXT("PatternFoundation"), TEXT("Pattern Foundation"), true,
			FoundationUnlocks, {}, State);
		ConfigureTechnology(
			ExpansionTechnologyPackage,
			TEXT("PatternExpansion"), TEXT("Pattern Expansion"), false,
			ExpansionUnlocks, { FName(TEXT("PatternFoundation")) }, State);

		ConfigureAugment(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Augments/DA_Augment_RapidTransform"),
			TEXT("RapidTransform"), TEXT("Rapid Transform"), ESRRunAugmentRarity::Common, ESRRunAugmentOfferRole::Immediate, 3,
			{ MakeModifier(TEXT("RapidTransformTime"), ESRRunModifierEffectKind::FacilityProcessTimeMultiplier, 0.85, ESRRunModifierFacilityScope::Transform) }, State);
		ConfigureAugment(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Augments/DA_Augment_OrganicOvergrowth"),
			TEXT("OrganicOvergrowth"), TEXT("Organic Overgrowth"), ESRRunAugmentRarity::Rare, ESRRunAugmentOfferRole::Immediate, 2,
			{ MakeModifier(TEXT("OrganicGrowth"), ESRRunModifierEffectKind::TransformOrganicGrowthDelta, 1.0, ESRRunModifierFacilityScope::Transform, ESRGlyphType::Organic) }, State);
		ConfigureAugment(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Augments/DA_Augment_BonusLattice"),
			TEXT("BonusLattice"), TEXT("Bonus Lattice"), ESRRunAugmentRarity::Rare, ESRRunAugmentOfferRole::Synergy, 3,
			{ MakeModifier(TEXT("BonusLatticeScore"), ESRRunModifierEffectKind::StellarBonusScoreMultiplier, 1.25) }, State);
		ConfigureAugment(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Augments/DA_Augment_EnvironmentalResonance"),
			TEXT("EnvironmentalResonance"), TEXT("Environmental Resonance"), ESRRunAugmentRarity::Epic, ESRRunAugmentOfferRole::Synergy, 1,
			{
				MakeModifier(TEXT("EnvironmentIntensity"), ESRRunModifierEffectKind::EnvironmentIntensityDelta, 1.0),
				MakeModifier(TEXT("EnvironmentBaseScore"), ESRRunModifierEffectKind::StellarBaseScoreMultiplier, 1.20)
			}, State);
		ConfigureAugment(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Augments/DA_Augment_TransitMomentum"),
			TEXT("TransitMomentum"), TEXT("Transit Momentum"), ESRRunAugmentRarity::Common, ESRRunAugmentOfferRole::Pivot, 3,
			{ MakeModifier(TEXT("TransitTime"), ESRRunModifierEffectKind::LogisticsTravelTimeMultiplier, 0.80) }, State);
		ConfigureAugment(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Augments/DA_Augment_PressureRecovery"),
			TEXT("StellarRepair"), TEXT("Stellar Repair"), ESRRunAugmentRarity::Rare, ESRRunAugmentOfferRole::Pivot, 2,
			{ MakeModifier(TEXT("HealthRecovery"), ESRRunModifierEffectKind::StellarHealthRecoveryMultiplier, 1.50) }, State);

		ConfigureTrial(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Trials/DA_Trial_DenseAtmosphere"),
			TEXT("DenseAtmosphere"), TEXT("Dense Atmosphere"), 3,
			{
				MakeModifier(TEXT("DenseProcessTime"), ESRRunModifierEffectKind::FacilityProcessTimeMultiplier, 1.25),
				MakeModifier(TEXT("DenseBonusReward"), ESRRunModifierEffectKind::StellarBonusScoreMultiplier, 1.40)
			}, State);
		ConfigureTrial(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Trials/DA_Trial_StellarSurge"),
			TEXT("StellarSurge"), TEXT("Stellar Surge"), 2,
			{
				MakeModifier(TEXT("SurgeDemand"), ESRRunModifierEffectKind::StellarRequiredScoreMultiplier, 1.30),
				MakeModifier(TEXT("SurgeBaseReward"), ESRRunModifierEffectKind::StellarBaseScoreMultiplier, 1.20)
			}, State);
		ConfigureTrial(
			TEXT("/Game/StarRovers/Pattern/DataAssets/Trials/DA_Trial_UnstableOrbits"),
			TEXT("UnstableOrbits"), TEXT("Unstable Orbits"), 4,
			{
				MakeModifier(TEXT("OrbitTravelTime"), ESRRunModifierEffectKind::LogisticsTravelTimeMultiplier, 1.30),
				MakeModifier(TEXT("OrbitHealthRecovery"), ESRRunModifierEffectKind::StellarHealthRecoveryMultiplier, 1.50)
			}, State);
	}

	FSRAvailableStructureDataAssetCategories BuildAvailableStructureCatalog(
		const FPatternContentAssets& Assets,
		USRStructureDataAsset* Conveyor)
	{
		FSRAvailableStructureDataAssetCategories Catalog;
		Catalog.Starting.Processor = {
			Assets.VectorEastStructure,
			Assets.VectorWestStructure,
			Assets.VectorNorthStructure,
			Assets.VectorSouthStructure
		};
		Catalog.Starting.Synthesizer = { Assets.StellarLoomStructure };
		Catalog.Starting.Miner = { Assets.PatternExtractorStructure };
		Catalog.Starting.Conveyor = { Conveyor };
		Catalog.Starting.Hub = { Assets.PatternHubStructure };
		Catalog.Basic.Processor = {
			Assets.CenterlineStructure,
			Assets.LatticeSeparatorStructure
		};
		return Catalog;
	}

	void AppendOperationCategoryAssets(
		const FSRAvailableStructureDataAssetOperationCategory& Category,
		TArray<USRStructureDataAsset*>& OutAssets)
	{
		for (const TObjectPtr<USRStructureDataAsset>& Asset : Category.Processor)
		{
			OutAssets.Add(Asset.Get());
		}
		for (const TObjectPtr<USRStructureDataAsset>& Asset : Category.Synthesizer)
		{
			OutAssets.Add(Asset.Get());
		}
		for (const TObjectPtr<USRStructureDataAsset>& Asset : Category.Miner)
		{
			OutAssets.Add(Asset.Get());
		}
		for (const TObjectPtr<USRStructureDataAsset>& Asset : Category.Conveyor)
		{
			OutAssets.Add(Asset.Get());
		}
		for (const TObjectPtr<USRStructureDataAsset>& Asset : Category.Hub)
		{
			OutAssets.Add(Asset.Get());
		}
	}

	TArray<USRStructureDataAsset*> FlattenAvailableStructureCatalog(
		const FSRAvailableStructureDataAssetCategories& Catalog)
	{
		TArray<USRStructureDataAsset*> Assets;
		AppendOperationCategoryAssets(Catalog.Starting, Assets);
		AppendOperationCategoryAssets(Catalog.Basic, Assets);
		AppendOperationCategoryAssets(Catalog.Advance, Assets);
		AppendOperationCategoryAssets(Catalog.Expert, Assets);
		AppendOperationCategoryAssets(Catalog.Innovation, Assets);
		return Assets;
	}

	bool MatchesOperationCategory(
		const FSRAvailableStructureDataAssetOperationCategory& Actual,
		const FSRAvailableStructureDataAssetOperationCategory& Expected)
	{
		return Actual.Processor == Expected.Processor
			&& Actual.Synthesizer == Expected.Synthesizer
			&& Actual.Miner == Expected.Miner
			&& Actual.Conveyor == Expected.Conveyor
			&& Actual.Hub == Expected.Hub;
	}

	bool MatchesAvailableStructureCatalog(
		const FSRAvailableStructureDataAssetCategories& Actual,
		const FSRAvailableStructureDataAssetCategories& Expected)
	{
		return MatchesOperationCategory(Actual.Starting, Expected.Starting)
			&& MatchesOperationCategory(Actual.Basic, Expected.Basic)
			&& MatchesOperationCategory(Actual.Advance, Expected.Advance)
			&& MatchesOperationCategory(Actual.Expert, Expected.Expert)
			&& MatchesOperationCategory(Actual.Innovation, Expected.Innovation);
	}

	TArray<ASRSolarSystemGenerator*> CollectSolarSystemGenerators(UWorld* World)
	{
		TArray<ASRSolarSystemGenerator*> Generators;
		if (!World)
		{
			return Generators;
		}
		for (ULevel* Level : World->GetLevels())
		{
			if (!Level)
			{
				continue;
			}
			for (AActor* Actor : Level->Actors)
			{
				if (ASRSolarSystemGenerator* Generator = Cast<ASRSolarSystemGenerator>(Actor))
				{
					Generators.Add(Generator);
				}
			}
		}
		return Generators;
	}

	FPatternContentAssets LoadCurrentRuntimeStructureAssets();

	void ConfigureRuntimeEntryContent(FPatternContentState& State)
	{
		UWorld* SolarSystemWorld = LoadSolarSystemEditorWorld();
		const FPatternContentAssets Assets = LoadCurrentRuntimeStructureAssets();
		USRPatternGenerationProfileDataAsset* Profile = LoadObject<USRPatternGenerationProfileDataAsset>(
			nullptr,
			*MakeObjectPath(GenerationProfilePackage));
		UBlueprint* GeneratorBlueprint = LoadObject<UBlueprint>(nullptr, SolarSystemGeneratorBlueprintPath);
		ASRSolarSystemGenerator* GeneratorDefaults = GeneratorBlueprint && GeneratorBlueprint->GeneratedClass
			? Cast<ASRSolarSystemGenerator>(GeneratorBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		UBlueprint* PlayerControllerBlueprint = LoadObject<UBlueprint>(nullptr, PlayerControllerBlueprintPath);
		ASRPlayerController* PlayerControllerDefaults = PlayerControllerBlueprint && PlayerControllerBlueprint->GeneratedClass
			? Cast<ASRPlayerController>(PlayerControllerBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		TSubclassOf<ASRCelestialBody> StarClass = LoadClass<ASRCelestialBody>(nullptr, TEXT("/Game/StarRovers/Celestial/Blueprints/BP_Star.BP_Star_C"));
		TSubclassOf<ASRCelestialBody> PlanetClass = LoadClass<ASRCelestialBody>(nullptr, TEXT("/Game/StarRovers/Celestial/Blueprints/BP_Planet.BP_Planet_C"));
		USRStarDataAsset* Star = LoadObject<USRStarDataAsset>(nullptr, TEXT("/Game/StarRovers/Celestial/DataAssets/Stars/DA_Star_MainSequenceStar.DA_Star_MainSequenceStar"));
		USRStructureDataAsset* Conveyor = LoadObject<USRStructureDataAsset>(nullptr, ConveyorStructurePath);
		TArray<USRPlanetDataAsset*> Planets = LoadAssetsUnder<USRPlanetDataAsset>(TEXT("/Game/StarRovers/Celestial/DataAssets/Planets"));
		TArray<USRMoonDataAsset*> Moons = LoadAssetsUnder<USRMoonDataAsset>(TEXT("/Game/StarRovers/Celestial/DataAssets/Moons"));
		TArray<ASRSolarSystemGenerator*> MapGenerators = CollectSolarSystemGenerators(SolarSystemWorld);

		const FSRAvailableStructureDataAssetCategories StructureCatalog =
			BuildAvailableStructureCatalog(Assets, Conveyor);
		const TArray<USRStructureDataAsset*> CatalogAssets = FlattenAvailableStructureCatalog(StructureCatalog);
		const bool bHasInvalidCatalogAsset = CatalogAssets.ContainsByPredicate([](const USRStructureDataAsset* Asset)
		{
			return !IsValid(Asset);
		});
		if (!GeneratorBlueprint
			|| !GeneratorDefaults
			|| !PlayerControllerBlueprint
			|| !PlayerControllerDefaults
			|| !StarClass
			|| !PlanetClass
			|| !Star
			|| !Profile
			|| Planets.Num() != 3
			|| Moons.Num() != 1
			|| !SolarSystemWorld
			|| MapGenerators.Num() != 1
			|| CatalogAssets.Num() != 10
			|| bHasInvalidCatalogAsset)
		{
			State.AddError(FString::Printf(
				TEXT("Runtime entry content is incomplete: GeneratorBP=%s PlayerControllerBP=%s Profile=%s Planets=%d Moons=%d Map=%s Generators=%d Structures=%d InvalidStructure=%s"),
				GeneratorDefaults ? TEXT("valid") : TEXT("invalid"),
				PlayerControllerDefaults ? TEXT("valid") : TEXT("invalid"),
				Profile ? TEXT("valid") : TEXT("invalid"),
				Planets.Num(),
				Moons.Num(),
				SolarSystemWorld ? TEXT("valid") : TEXT("invalid"),
				MapGenerators.Num(),
				CatalogAssets.Num(),
				bHasInvalidCatalogAsset ? TEXT("true") : TEXT("false")));
			return;
		}

		State.MarkChanged(GeneratorBlueprint);
		State.MarkChanged(PlayerControllerBlueprint);
		State.MarkChanged(SolarSystemWorld);
		if (!State.bApply)
		{
			return;
		}

		GeneratorDefaults->ConfigurePatternContentForEditor(
			Profile, StarClass, PlanetClass, { Star }, Planets, Moons, 1000);
		MapGenerators[0]->ConfigurePatternContentForEditor(
			Profile, StarClass, PlanetClass, { Star }, Planets, Moons, 1000);
		PlayerControllerDefaults->ConfigureAvailableStructureDataAssetsForEditor(StructureCatalog);
	}

	FPatternContentAssets LoadCurrentRuntimeStructureAssets()
	{
		FPatternContentAssets Assets;
		Assets.PatternExtractorStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_PatternExtractor.DA_Structure_PatternExtractor"));
		Assets.PatternHubStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_PatternHub.DA_Structure_PatternHub"));
		Assets.VectorEastStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_VectorShifterEast.DA_Structure_VectorShifterEast"));
		Assets.VectorWestStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_VectorShifterWest.DA_Structure_VectorShifterWest"));
		Assets.VectorNorthStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_VectorShifterNorth.DA_Structure_VectorShifterNorth"));
		Assets.VectorSouthStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_VectorShifterSouth.DA_Structure_VectorShifterSouth"));
		Assets.CenterlineStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_CenterlineShifter.DA_Structure_CenterlineShifter"));
		Assets.StellarLoomStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_StellarLoom.DA_Structure_StellarLoom"));
		Assets.LatticeSeparatorStructure = LoadObject<USRStructureDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Pattern/DA_Structure_LatticeSeparator.DA_Structure_LatticeSeparator"));
		return Assets;
	}

	void ValidateRuntimeEntryContent(FPatternContentState& State)
	{
		USRPatternGenerationProfileDataAsset* Profile = LoadObject<USRPatternGenerationProfileDataAsset>(
			nullptr,
			*MakeObjectPath(GenerationProfilePackage));
		TSubclassOf<ASRCelestialBody> StarClass = LoadClass<ASRCelestialBody>(nullptr,
			TEXT("/Game/StarRovers/Celestial/Blueprints/BP_Star.BP_Star_C"));
		TSubclassOf<ASRCelestialBody> PlanetClass = LoadClass<ASRCelestialBody>(nullptr,
			TEXT("/Game/StarRovers/Celestial/Blueprints/BP_Planet.BP_Planet_C"));
		USRStarDataAsset* Star = LoadObject<USRStarDataAsset>(nullptr,
			TEXT("/Game/StarRovers/Celestial/DataAssets/Stars/DA_Star_MainSequenceStar.DA_Star_MainSequenceStar"));
		TArray<USRPlanetDataAsset*> Planets = LoadAssetsUnder<USRPlanetDataAsset>(
			TEXT("/Game/StarRovers/Celestial/DataAssets/Planets"));
		TArray<USRMoonDataAsset*> Moons = LoadAssetsUnder<USRMoonDataAsset>(
			TEXT("/Game/StarRovers/Celestial/DataAssets/Moons"));

		const FPatternContentAssets StructureAssets = LoadCurrentRuntimeStructureAssets();
		USRStructureDataAsset* Conveyor = LoadObject<USRStructureDataAsset>(nullptr, ConveyorStructurePath);
		const FSRAvailableStructureDataAssetCategories ExpectedStructureCatalog =
			BuildAvailableStructureCatalog(StructureAssets, Conveyor);
		const TArray<USRStructureDataAsset*> ExpectedStructures =
			FlattenAvailableStructureCatalog(ExpectedStructureCatalog);
		TSet<const USRStructureDataAsset*> UniqueExpectedStructures;
		bool bHasInvalidExpectedStructure = false;
		for (const USRStructureDataAsset* Structure : ExpectedStructures)
		{
			bHasInvalidExpectedStructure |= !IsValid(Structure);
			if (IsValid(Structure))
			{
				UniqueExpectedStructures.Add(Structure);
			}
		}
		if (ExpectedStructures.Num() != 10
			|| UniqueExpectedStructures.Num() != 10
			|| bHasInvalidExpectedStructure)
		{
			State.AddError(TEXT("The runtime construction catalog must contain the 9 unique Pattern facilities plus the conveyor."));
		}

		auto ValidateGenerator = [
			&State,
			Profile,
			StarClass,
			PlanetClass,
			Star,
			&Planets,
			&Moons](const ASRSolarSystemGenerator* Generator, const TCHAR* OwnerLabel)
		{
			FString FailureReason;
			if (!IsValid(Generator)
				|| !Generator->ValidatePatternContentConfiguration(FailureReason)
				|| !Generator->MatchesPatternContentForEditor(
					Profile, StarClass, PlanetClass, { Star }, Planets, Moons))
			{
				State.AddError(FString::Printf(
					TEXT("%s does not own the authoritative Pattern runtime content. Reason=%s"),
					OwnerLabel,
					FailureReason.IsEmpty() ? TEXT("references differ from the semantic catalog") : *FailureReason));
			}
		};

		UBlueprint* GeneratorBlueprint = LoadObject<UBlueprint>(nullptr, SolarSystemGeneratorBlueprintPath);
		ASRSolarSystemGenerator* GeneratorDefaults = GeneratorBlueprint && GeneratorBlueprint->GeneratedClass
			? Cast<ASRSolarSystemGenerator>(GeneratorBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		ValidateGenerator(GeneratorDefaults, TEXT("BP_SolarSystemGenerator defaults"));

		UWorld* SolarSystemWorld = LoadSolarSystemEditorWorld();
		TArray<ASRSolarSystemGenerator*> MapGenerators = CollectSolarSystemGenerators(SolarSystemWorld);
		if (MapGenerators.Num() != 1)
		{
			State.AddError(FString::Printf(
				TEXT("SolarSystem.umap must contain exactly one configured SolarSystem Generator; found %d."),
				MapGenerators.Num()));
		}
		else
		{
			ValidateGenerator(MapGenerators[0], TEXT("SolarSystem.umap Generator instance"));
		}

		UBlueprint* PlayerControllerBlueprint = LoadObject<UBlueprint>(nullptr, PlayerControllerBlueprintPath);
		ASRPlayerController* PlayerControllerDefaults = PlayerControllerBlueprint && PlayerControllerBlueprint->GeneratedClass
			? Cast<ASRPlayerController>(PlayerControllerBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		if (!PlayerControllerDefaults
			|| !MatchesAvailableStructureCatalog(
				PlayerControllerDefaults->GetAvailableStructureDataAssetsForEditor(),
				ExpectedStructureCatalog))
		{
			State.AddError(TEXT("BP_SRPlayerController does not expose the authoritative Pattern construction catalog."));
		}

		const USRSimulationSettings* SimulationSettings = GetDefault<USRSimulationSettings>();
		int32 LoadedTechnologyCount = 0;
		int32 LoadedAugmentCount = 0;
		int32 LoadedTrialCount = 0;
		if (SimulationSettings)
		{
			for (const TSoftObjectPtr<USRTechnologyDataAsset>& Reference : SimulationSettings->TechnologyDataAssets)
			{
				LoadedTechnologyCount += Reference.LoadSynchronous() ? 1 : 0;
			}
			for (const TSoftObjectPtr<USRRunAugmentDataAsset>& Reference : SimulationSettings->RunAugmentDataAssets)
			{
				LoadedAugmentCount += Reference.LoadSynchronous() ? 1 : 0;
			}
			for (const TSoftObjectPtr<USRTrialDataAsset>& Reference : SimulationSettings->TrialDataAssets)
			{
				LoadedTrialCount += Reference.LoadSynchronous() ? 1 : 0;
			}
		}
		if (!SimulationSettings
			|| SimulationSettings->TechnologyDataAssets.Num() != 2
			|| SimulationSettings->RunAugmentDataAssets.Num() != 6
			|| SimulationSettings->TrialDataAssets.Num() != 3
			|| LoadedTechnologyCount != 2
			|| LoadedAugmentCount != 6
			|| LoadedTrialCount != 3)
		{
			State.AddError(FString::Printf(
				TEXT("Simulation Settings did not load the complete modifier catalog: Technology=%d/2 Augment=%d/6 Trial=%d/3."),
				LoadedTechnologyCount,
				LoadedAugmentCount,
				LoadedTrialCount));
		}
	}

	void ValidateContent(FPatternContentState& State)
	{
		TSet<TObjectPtr<USRFacilityDataAsset>> ConstructibleFacilities;
		for (USRStructureDataAsset* Structure : LoadAssetsUnder<USRStructureDataAsset>(TEXT("/Game/StarRovers/Structure/DataAssets")))
		{
			if (IsValid(Structure)
				&& Structure->bAvailableForConstruction
				&& IsValid(Structure->FacilityDataAsset.Get()))
			{
				ConstructibleFacilities.Add(Structure->FacilityDataAsset.Get());
			}
		}

		const TArray<USRResourceDataAsset*> Resources = LoadAssetsUnder<USRResourceDataAsset>(ResourceRoot);
		if (Resources.Num() != 6)
		{
			State.AddError(FString::Printf(TEXT("Expected exactly 6 Pattern resource assets, found %d."), Resources.Num()));
		}
		TSet<FName> ResourceIds;
		for (USRResourceDataAsset* Resource : Resources)
		{
			FSRPattern Pattern;
			if (!IsValid(Resource)
				|| Resource->ResourceId.IsNone()
				|| ResourceIds.Contains(Resource->ResourceId)
				|| !Resource->TryGenerateSourcePattern(12345 + Resource->SourcePatternSeedSalt, Pattern))
			{
				State.AddError(FString::Printf(TEXT("Resource '%s' has an invalid source Pattern."), *GetNameSafe(Resource)));
			}
			if (IsValid(Resource))
			{
				ResourceIds.Add(Resource->ResourceId);
			}
		}
		if (!LoadAssetsUnder<USRResourceDataAsset>(TEXT("/Game/StarRovers/Automation/DataAssets/Resources")).IsEmpty())
		{
			State.AddError(TEXT("Legacy resource Data Assets remain under Automation/DataAssets/Resources."));
		}

		const TArray<USRFacilityDataAsset*> Facilities = LoadAssetsUnder<USRFacilityDataAsset>(FacilityRoot);
		if (Facilities.Num() != 9)
		{
			State.AddError(FString::Printf(TEXT("Expected exactly 9 Pattern facility assets, found %d."), Facilities.Num()));
		}
		TSet<FName> FacilityIds;
		for (USRFacilityDataAsset* Facility : Facilities)
		{
			if (!IsValid(Facility)
				|| FacilityIds.Contains(Facility->FacilityId)
				|| !ConstructibleFacilities.Contains(Facility)
				|| !IsPatternFacilityValid(*Facility))
			{
				State.AddError(FString::Printf(TEXT("Facility '%s' has invalid Pattern operation data."), *GetNameSafe(Facility)));
			}
			if (IsValid(Facility))
			{
				FacilityIds.Add(Facility->FacilityId);
			}
		}
		if (!LoadAssetsUnder<USRFacilityDataAsset>(TEXT("/Game/StarRovers/Automation/DataAssets/Facilities")).IsEmpty())
		{
			State.AddError(TEXT("Numbered legacy Facility Data Assets remain under Automation/DataAssets/Facilities."));
		}
		if (LoadAssetsUnder<USRStructureDataAsset>(FacilityStructureRoot).Num() != 9)
		{
			State.AddError(TEXT("The semantic Pattern facility Structure set must contain exactly 9 assets."));
		}

		const TArray<USRPatternEnvironmentDataAsset*> Environments =
			LoadAssetsUnder<USRPatternEnvironmentDataAsset>(EnvironmentRoot);
		if (Environments.Num() != 4)
		{
			State.AddError(FString::Printf(TEXT("Expected exactly 4 Pattern environment assets, found %d."), Environments.Num()));
		}
		TSet<FName> EnvironmentIds;
		for (USRPatternEnvironmentDataAsset* Environment : Environments)
		{
			if (!IsValid(Environment)
				|| EnvironmentIds.Contains(Environment->EnvironmentId)
				|| !Environment->IsEnvironmentValid())
			{
				State.AddError(FString::Printf(TEXT("Environment '%s' is invalid or duplicates an ID."), *GetNameSafe(Environment)));
			}
			if (IsValid(Environment))
			{
				EnvironmentIds.Add(Environment->EnvironmentId);
			}
		}

		const auto ValidateEnvironmentDepositGlyphs = [&State](const auto* Body)
		{
			if (!IsValid(Body)
				|| !IsValid(Body->PatternEnvironmentDataAsset.Get())
				|| !IsValid(Body->TerrainProfileDataAsset.Get()))
			{
				return;
			}

			TSet<ESRGlyphType> FeaturedGlyphs;
			for (const FSRPatternEnvironmentEffectSpec& Effect : Body->PatternEnvironmentDataAsset->Effects)
			{
				if (Effect.AffectedGlyph != ESRGlyphType::Empty)
				{
					FeaturedGlyphs.Add(Effect.AffectedGlyph);
				}
			}

			TSet<ESRGlyphType> EnabledDepositGlyphs;
			for (const FSRProfileNaturalStructureSpawnRule& Rule : Body->TerrainProfileDataAsset->ProfileNaturalStructureSpawnRules)
			{
				const FSRNaturalStructureSpawnRuleOverride* Override =
					Body->ProfileNaturalStructureSpawnRuleOverrides.FindByPredicate(
						[RuleId = Rule.RuleId](const FSRNaturalStructureSpawnRuleOverride& Candidate)
						{
							return Candidate.RuleId == RuleId;
						});
				if (!(Override ? Override->bEnabled : Rule.bEnabled))
				{
					continue;
				}

				const USRStructureDataAsset* Deposit = Rule.StructureDataAsset.Get();
				const USRResourceDataAsset* Resource = IsValid(Deposit) && Deposit->bIsResourceDeposit
					? Deposit->DepositResourceDataAsset.Get()
					: nullptr;
				if (!IsValid(Resource))
				{
					continue;
				}
				for (const FSRSourceGlyphCount& GlyphCount : Resource->SourceGlyphCounts)
				{
					if (GlyphCount.Glyph != ESRGlyphType::Empty && GlyphCount.Count > 0)
					{
						EnabledDepositGlyphs.Add(GlyphCount.Glyph);
					}
				}
			}

			for (const ESRGlyphType FeaturedGlyph : FeaturedGlyphs)
			{
				if (!EnabledDepositGlyphs.Contains(FeaturedGlyph))
				{
					State.AddError(FString::Printf(
						TEXT("Body '%s' has an environment affecting glyph %d but none of its enabled deposits contain that glyph."),
						*GetNameSafe(Body),
						static_cast<int32>(FeaturedGlyph)));
				}
			}

			int32 SecondaryGlyphCount = 0;
			for (const ESRGlyphType DepositGlyph : EnabledDepositGlyphs)
			{
				SecondaryGlyphCount += FeaturedGlyphs.Contains(DepositGlyph) ? 0 : 1;
			}
			if (!FeaturedGlyphs.IsEmpty() && (SecondaryGlyphCount < 1 || SecondaryGlyphCount > 2))
			{
				State.AddError(FString::Printf(
					TEXT("Body '%s' must expose one or two secondary deposit glyphs beside its environment glyph; found %d."),
					*GetNameSafe(Body),
					SecondaryGlyphCount));
			}
		};

		for (USRPlanetDataAsset* Planet : LoadAssetsUnder<USRPlanetDataAsset>(TEXT("/Game/StarRovers/Celestial/DataAssets/Planets")))
		{
			const USRPlanetShapeDataAsset* Shape = IsValid(Planet) ? Planet->ShapeDataAsset.Get() : nullptr;
			if (!IsValid(Shape)
				|| !IsValid(Shape->GetDynamicMeshBaseDataAsset())
				|| !Shape->IsDynamicMeshBaseShapeCompatible())
			{
				State.AddError(FString::Printf(TEXT("Planet '%s' has no valid baked Shape Data Asset."), *GetNameSafe(Planet)));
			}
			if (!IsValid(Planet)
				|| !IsValid(Planet->PatternEnvironmentDataAsset.Get())
				|| !Planet->PatternEnvironmentDataAsset->IsEnvironmentValid())
			{
				State.AddError(FString::Printf(TEXT("Planet '%s' has an invalid Pattern environment."), *GetNameSafe(Planet)));
			}
			ValidateEnvironmentDepositGlyphs(Planet);
		}
		for (USRMoonDataAsset* Moon : LoadAssetsUnder<USRMoonDataAsset>(TEXT("/Game/StarRovers/Celestial/DataAssets/Moons")))
		{
			const USRPlanetShapeDataAsset* Shape = IsValid(Moon) ? Moon->ShapeDataAsset.Get() : nullptr;
			if (!IsValid(Shape)
				|| !IsValid(Shape->GetDynamicMeshBaseDataAsset())
				|| !Shape->IsDynamicMeshBaseShapeCompatible())
			{
				State.AddError(FString::Printf(TEXT("Moon '%s' has no valid baked Shape Data Asset."), *GetNameSafe(Moon)));
			}
			if (!IsValid(Moon)
				|| !IsValid(Moon->PatternEnvironmentDataAsset.Get())
				|| !Moon->PatternEnvironmentDataAsset->IsEnvironmentValid())
			{
				State.AddError(FString::Printf(TEXT("Moon '%s' has an invalid Pattern environment."), *GetNameSafe(Moon)));
			}
			ValidateEnvironmentDepositGlyphs(Moon);
		}

		USRPatternGenerationProfileDataAsset* Profile = LoadOrCreateAsset<USRPatternGenerationProfileDataAsset>(GenerationProfilePackage, State);
		if (!Profile
			|| Profile->CandidateStellarContracts.Num() != 16
			|| Profile->AvailableFacilityDataAssets.Num() != 5
			|| Profile->MaxValidationSourcesPerResourcePerBody != 1
			|| !Profile->bRequireInterBodyTransfer)
		{
			State.AddError(TEXT("The default Pattern Generation Profile does not match the inter-body Stellar Weave content set."));
		}
		else
		{
			TSet<FName> ContractIds;
			for (const FSRStellarPatternContract& Contract : Profile->CandidateStellarContracts)
			{
				FString FailureReason;
				if (!FSRStellarPatternContractResolver::ValidateContract(Contract, FailureReason) || ContractIds.Contains(Contract.ContractId))
				{
					State.AddError(FString::Printf(TEXT("Generation contract '%s' is invalid: %s"), *Contract.ContractId.ToString(), *FailureReason));
				}
				ContractIds.Add(Contract.ContractId);
			}
		}

		for (USRRunAugmentDataAsset* Augment : LoadAssetsUnder<USRRunAugmentDataAsset>(TEXT("/Game/StarRovers/Pattern/DataAssets/Augments")))
		{
			for (const FSRRunModifierEffect& Effect : Augment->Effects)
			{
				FString FailureReason;
				if (!FSRRunModifierResolver::ValidateEffect(Effect, FailureReason))
				{
					State.AddError(FString::Printf(TEXT("Augment '%s' has an invalid effect: %s"), *Augment->AugmentId.ToString(), *FailureReason));
				}
			}
		}
		for (USRTrialDataAsset* Trial : LoadAssetsUnder<USRTrialDataAsset>(TEXT("/Game/StarRovers/Pattern/DataAssets/Trials")))
		{
			for (const FSRRunModifierEffect& Effect : Trial->Effects)
			{
				FString FailureReason;
				if (!FSRRunModifierResolver::ValidateEffect(Effect, FailureReason))
				{
					State.AddError(FString::Printf(TEXT("Trial '%s' has an invalid effect: %s"), *Trial->TrialId.ToString(), *FailureReason));
				}
			}
		}

		ValidateRuntimeEntryContent(State);
	}

	bool SaveChangedPackages(FPatternContentState& State)
	{
		for (const TPair<TObjectPtr<UPackage>, TObjectPtr<UObject>>& Pair : State.PackagesToSave)
		{
			UPackage* Package = Pair.Key.Get();
			UObject* Asset = Pair.Value.Get();
			if (!Package || !Asset)
			{
				State.AddError(TEXT("A changed Pattern content package lost its asset before save."));
				continue;
			}
			if (UWorld* World = Cast<UWorld>(Asset))
			{
				if (!UEditorLoadingAndSavingUtils::SaveMap(World, Package->GetName()))
				{
					State.AddError(FString::Printf(
						TEXT("Failed to save Pattern content map: %s"),
						*Package->GetName()));
				}
				continue;
			}

			const FString Filename = FPackageName::LongPackageNameToFilename(
				Package->GetName(),
				FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
			{
				State.AddError(FString::Printf(TEXT("Failed to save Pattern content package: %s"), *Filename));
			}
		}
		return State.Errors.IsEmpty();
	}
}

USRGeneratePatternContentCommandlet::USRGeneratePatternContentCommandlet()
{
	IsEditor = true;
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
}

int32 USRGeneratePatternContentCommandlet::Main(const FString& Params)
{
	using namespace StarRovers::Editor::PatternContent;
	if (FParse::Param(*Params, TEXT("CatalogOnly")))
	{
		LogCurrentContentCatalog();
		return 0;
	}
	FPatternContentState State;
	State.bApply = FParse::Param(*Params, TEXT("Apply"));
	const bool bInspectOnly = FParse::Param(*Params, TEXT("InspectOnly"));
	if (State.bApply == bInspectOnly)
	{
		UE_LOG(LogTemp, Error, TEXT("Specify exactly one of -Apply or -InspectOnly."));
		return 1;
	}

	FPatternContentAssets Assets;
	TArray<USRFacilityDataAsset*> GenerationFacilities;
	TArray<FName> FoundationUnlocks;
	TArray<FName> ExpansionUnlocks;
	USRPatternGenerationProfileDataAsset* Profile = nullptr;
	if (State.bApply)
	{
		LoadAndRenameCoreAssets(Assets, State);
		ConfigureResources(Assets, State);
		ConfigureFacilities(Assets, GenerationFacilities, FoundationUnlocks, ExpansionUnlocks, State);
		ConfigureEnvironments(Assets, State);
		ConfigureDepositsAndDistribution(Assets, State);
		ConfigureContractAndProfile(GenerationFacilities, Profile, State);
		ConfigureRunModifiers(FoundationUnlocks, ExpansionUnlocks, State);
		if (!SaveChangedPackages(State))
		{
			return 1;
		}
		State.PackagesToSave.Reset();
		ConfigureRuntimeEntryContent(State);
		if (!SaveChangedPackages(State))
		{
			return 1;
		}
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().ScanPathsSynchronous(
			{ TEXT("/Game/StarRovers") }, true);
	}

	ValidateContent(State);
	UE_LOG(LogTemp, Display,
		TEXT("Pattern content migration %s: created=%d renamed=%d changed=%d validationErrors=%d"),
		State.bApply ? TEXT("applied") : TEXT("inspected"),
		State.CreatedAssetCount,
		State.RenamedAssetCount,
		State.ChangedAssetCount,
		State.Errors.Num());
	return State.Errors.IsEmpty() ? 0 : 1;
}

#else

USRGeneratePatternContentCommandlet::USRGeneratePatternContentCommandlet()
{
	IsEditor = false;
	UseCommandletResultAsExitCode = true;
}

int32 USRGeneratePatternContentCommandlet::Main(const FString& Params)
{
	return 1;
}

#endif
