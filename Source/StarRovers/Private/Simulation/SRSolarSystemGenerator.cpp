#include "Simulation/SRSolarSystemGenerator.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRMoonDataAsset.h"
#include "Celestial/SRPlanet.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Celestial/SRStarDataAsset.h"
#include "Celestial/SRStar.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Gravity/SRGravityParent.h"
#include "Misc/Guid.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructure.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetBiomeDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"
#include "PCGComponent.h"
#include "TimerManager.h"
#include "UI/SRLoadingScreenWidget.h"
#include "Utility/SRMemoryDiagnostics.h"
#include "Utility/SRTimingLog.h"

namespace
{
	double SRSolarNowSeconds()
	{
		return FPlatformTime::Seconds();
	}

	double SRSolarElapsedMilliseconds(double StartSeconds)
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

	int32 CreateRuntimeRandomGenerationSeed()
	{
		const FGuid Guid = FGuid::NewGuid();
		uint32 Hash = HashCombine(Guid.A, Guid.B);
		Hash = HashCombine(Hash, Guid.C);
		Hash = HashCombine(Hash, Guid.D);
		Hash = HashCombine(Hash, ::GetTypeHash(FPlatformTime::Cycles64()));
		Hash = HashCombine(Hash, ::GetTypeHash(FMath::Rand()));
		return static_cast<int32>((Hash % static_cast<uint32>(TNumericLimits<int32>::Max() - 1)) + 1);
	}

	void ApplyResolvedGenerationSeed(FSRCelestialBodyData& InOutData, int32 ResolvedSeed)
	{
		InOutData.GenerationSeed = ResolvedSeed;
		InOutData.DynamicMeshGeneration.GenerationSeed = ResolvedSeed;
	}

	bool ShouldRandomizeBodyGenerationSeed(const FSRCelestialBodyData& BodyData)
	{
		return BodyData.bRandomizeGenerationSeedEachRun
			|| BodyData.DynamicMeshGeneration.bRandomizeGenerationSeedEachRun;
	}

	void LogGeneratorMissingData(const UObject* SourceObject, const TCHAR* FieldName)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Solar system generation requires %s on '%s'."),
			FieldName ? FieldName : TEXT("<UnknownField>"),
			IsValid(SourceObject) ? *SourceObject->GetName() : TEXT("<InvalidObject>"));
	}

	bool TryComputeScaledBodyRadiusFromCelestialBodyRequest(const FSRCelestialBodyGenerateRequest& CelestialBodyRequest, float& OutRadius)
	{
		OutRadius = 0.0f;
		const UStaticMesh* MeshAsset = CelestialBodyRequest.BodyData.StaticMesh.Get();
		if (!IsValid(MeshAsset))
		{
			UE_LOG(LogTemp, Error, TEXT("Solar system generation requires StaticMesh for '%s'."), *CelestialBodyRequest.BodyData.VariableName.ToString());
			return false;
		}

		OutRadius = FMath::Max(0.0f, MeshAsset->GetBounds().SphereRadius * FMath::Max(0.0f, CelestialBodyRequest.BodyData.Scale));
		return true;
	}

	float ComputeScaledBodyRadius(const ASRCelestialBody* CelestialBody)
	{
		if (!IsValid(CelestialBody))
		{
			return 0.0f;
		}

		const FSRCelestialBodyData BodyData = CelestialBody->GetData();
		return IsValid(BodyData.StaticMesh.Get())
			? BodyData.StaticMesh->GetBounds().SphereRadius * FMath::Max(0.0f, BodyData.Scale)
			: 0.0f;
	}

	float ComputeGravityRadiusFromCelestialBodyRequest(const FSRCelestialBodyGenerateRequest& CelestialBodyRequest)
	{
		return FMath::Max(0.0f, CelestialBodyRequest.BodyData.Mass)
			* FMath::Max(0.0f, CelestialBodyRequest.BodyData.GravityRadiusRatio);
	}

	bool TryGetRequiredVariableName(const UObject* DataAsset, const FText& VariableName, FText& OutVariableName)
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

	bool TryBuildDataFromDataAsset(
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
		return true;
	}

	void ApplyClassDefaultRuntimeVisualSettings(
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

	TSubclassOf<ASRCelestialBody> ValidateRuntimeCelestialClass(
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

		if (!IsValid(OutRequest.BodyData.StaticMesh))
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

ASRSolarSystemGenerator::ASRSolarSystemGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GenerationSeed = 1000;
	bRandomizeGenerationSeedEachRun = false;
	MinPlanet = 3;
	MaxPlanet = 7;
	MinMoon = 0;
	MaxMoon = 1;
	PlanetInitialOrbit = 30000.0f;
	PlanetOrbitIncrease = 20000.0f;
	MoonInitialOrbit = 6000.0f;
	MoonOrbitIncrease = 4000.0f;
	bGenerateNaturalStructures = true;
	LoadingScreenWidgetClass = USRLoadingScreenWidget::StaticClass();
	LoadingScreenZOrder = 10000;
	bEnableMemoryDiagnostics = true;
}

void ASRSolarSystemGenerator::BeginPlay()
{
	Super::BeginPlay();
	EnsureMemoryDiagnosticTrackedClasses();
	LogMemoryDiagnosticsSnapshot(TEXT("SolarSystemGenerator.BeginPlay.BeforeGeneration"));

	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	StartRuntimeSystemGenerationWithLoadingScreen();
}

void ASRSolarSystemGenerator::Destroyed()
{
	bRuntimeGenerationInProgress = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredGenerateRuntimeSystemTimerHandle);
	}
	ClearRuntimeGeneratedBodies();
	HideLoadingScreen();
	Super::Destroyed();
}

ASRCelestialBody* ASRSolarSystemGenerator::GenerateRuntimeSystem()
{
	FSRTimingLogSession TimingLogSession(TEXT("GenerateRuntimeSystem"));
	const double TotalStart = SRSolarNowSeconds();
	if (!GetWorld())
	{
		return nullptr;
	}

	TArray<FSRGenerationStageTiming> StageTimings;
	auto LogStageTiming = [&StageTimings](const TCHAR* StageName, double Milliseconds, const FString& Suffix = FString())
	{
		StageTimings.Add({ FString(StageName), Milliseconds });
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.%s %.2f ms%s"), StageName, Milliseconds, *Suffix));
	};

	LogMemoryDiagnosticsSnapshot(TEXT("GenerateRuntimeSystem.BeforeClear"));
	double StageStart = SRSolarNowSeconds();
	ClearRuntimeGeneratedBodies();
	LogStageTiming(TEXT("ClearRuntimeGeneratedBodies"), SRSolarElapsedMilliseconds(StageStart));

	const int32 RuntimeGenerationSeed = bRandomizeGenerationSeedEachRun
		? CreateRuntimeRandomGenerationSeed()
		: GenerationSeed;
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("GenerateRuntimeSystem.Seed Configured=%d Runtime=%d Randomized=%s"),
		GenerationSeed,
		RuntimeGenerationSeed,
		bRandomizeGenerationSeedEachRun ? TEXT("true") : TEXT("false")));

	FRandomStream RandomStream(RuntimeGenerationSeed);
	const USRStarDataAsset* SelectedStarDataAsset = nullptr;
	StageStart = SRSolarNowSeconds();
	RuntimeStarBody = SpawnPrimaryStar(RandomStream, SelectedStarDataAsset);
	LogStageTiming(TEXT("SpawnPrimaryStar"), SRSolarElapsedMilliseconds(StageStart));
	if (!IsValid(RuntimeStarBody))
	{
		return nullptr;
	}

	StageStart = SRSolarNowSeconds();
	SpawnPlanets(RuntimeStarBody, SelectedStarDataAsset, RandomStream, RuntimePlanetBodies);
	LogStageTiming(TEXT("SpawnPlanets"), SRSolarElapsedMilliseconds(StageStart), FString::Printf(TEXT(" Planets=%d Moons=%d"), RuntimePlanetBodies.Num(), RuntimeMoonBodies.Num()));
	StageStart = SRSolarNowSeconds();
	PrepareRuntimeGeneratedDynamicMeshes();
	LogStageTiming(TEXT("PrepareRuntimeGeneratedDynamicMeshes"), SRSolarElapsedMilliseconds(StageStart));
	StageStart = SRSolarNowSeconds();
	GenerateRuntimeNaturalStructures(RuntimeGenerationSeed);
	LogStageTiming(TEXT("GenerateRuntimeNaturalStructures"), SRSolarElapsedMilliseconds(StageStart));
	StageStart = SRSolarNowSeconds();
	if (USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = GetWorld()->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
	{
		CelestialBodyRegistry->SetPrimaryStarActor(RuntimeStarBody);
	}
	LogStageTiming(TEXT("Registry"), SRSolarElapsedMilliseconds(StageStart));
	const FSRGenerationStageTiming* SlowestStageTiming = nullptr;
	for (const FSRGenerationStageTiming& StageTiming : StageTimings)
	{
		if (!SlowestStageTiming || StageTiming.Milliseconds > SlowestStageTiming->Milliseconds)
		{
			SlowestStageTiming = &StageTiming;
		}
	}
	if (SlowestStageTiming)
	{
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.Bottleneck Stage=%s %.2f ms"), *SlowestStageTiming->Name, SlowestStageTiming->Milliseconds));
	}
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.Total %.2f ms"), SRSolarElapsedMilliseconds(TotalStart)));
	LogMemoryDiagnosticsSnapshot(TEXT("GenerateRuntimeSystem.AfterComplete"));

	return RuntimeStarBody;
}

void ASRSolarSystemGenerator::ClearRuntimeGeneratedBodies()
{
	LogMemoryDiagnosticsSnapshot(TEXT("ClearRuntimeGeneratedBodies.BeforeDestroy"));
	const bool bHadRuntimeGeneratedObjects =
		IsValid(RuntimeStarBody)
		|| !RuntimePlanetBodies.IsEmpty()
		|| !RuntimeMoonBodies.IsEmpty()
		|| !RuntimeNaturalStructureActors.IsEmpty();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredGenerateRuntimeSystemTimerHandle);
	}
	DestroyRuntimeNaturalStructures();
	DestroyTrackedActors(RuntimeMoonBodies);
	DestroyTrackedActors(RuntimePlanetBodies);
	DestroyTrackedActor(RuntimeStarBody);
	LogMemoryDiagnosticsSnapshot(TEXT("ClearRuntimeGeneratedBodies.AfterDestroyRefsCleared"));
	if (UWorld* World = GetWorld())
	{
		if (USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
		{
			CelestialBodyRegistry->SetPrimaryStarActor(nullptr);
		}

		if (bHadRuntimeGeneratedObjects && World->IsGameWorld())
		{
			if (GEngine)
			{
				GEngine->ForceGarbageCollection(true);
				UE_LOG(LogTemp, Display, TEXT("Requested garbage collection after clearing runtime generated celestial bodies."));
				LogMemoryDiagnosticsSnapshot(TEXT("ClearRuntimeGeneratedBodies.AfterGCRequest"));
				TArray<FString> ExtraLines;
				ASRCelestialBody::AppendRuntimeMemoryDiagnostics(ExtraLines);
				FSRMemoryDiagnostics::LogSnapshotNextTick(World, TEXT("ClearRuntimeGeneratedBodies.AfterGCTick"), ExtraLines);
			}
		}
		else if (bEnableMemoryDiagnostics)
		{
			LogMemoryDiagnosticsSnapshot(bHadRuntimeGeneratedObjects
				? TEXT("ClearRuntimeGeneratedBodies.GCSkipped.NonGameWorld")
				: TEXT("ClearRuntimeGeneratedBodies.GCSkipped.NoRuntimeGeneratedObjects"));
		}
	}
}

void ASRSolarSystemGenerator::EnsureMemoryDiagnosticTrackedClasses() const
{
	static bool bRegistered = false;
	if (bRegistered)
	{
		return;
	}

	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRCelestialBody"), ASRCelestialBody::StaticClass(), TEXT("ASRCelestialBody"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRPlanet"), ASRPlanet::StaticClass(), TEXT("ASRPlanet"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRStar"), ASRStar::StaticClass(), TEXT("ASRStar"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("UDynamicMeshComponent"), UDynamicMeshComponent::StaticClass(), TEXT("UDynamicMeshComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USRPlanetSurfaceGrid"), USRPlanetSurfaceGrid::StaticClass(), TEXT("USRPlanetSurfaceGrid"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USRStructureInstanceManagerComponent"), USRStructureInstanceManagerComponent::StaticClass(), TEXT("USRStructureInstanceManagerComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRStructure"), ASRStructure::StaticClass(), TEXT("ASRStructure"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("ASRConveyorBeltActor"), ASRConveyorBeltActor::StaticClass(), TEXT("ASRConveyorBeltActor"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("UHierarchicalInstancedStaticMeshComponent"), UHierarchicalInstancedStaticMeshComponent::StaticClass(), TEXT("UHierarchicalInstancedStaticMeshComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("UPCGComponent"), UPCGComponent::StaticClass(), TEXT("UPCGComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USplineComponent"), USplineComponent::StaticClass(), TEXT("USplineComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USplineMeshComponent"), USplineMeshComponent::StaticClass(), TEXT("USplineMeshComponent"));
	FSRMemoryDiagnostics::RegisterTrackedClass(TEXT("USRLoadingScreenWidget"), USRLoadingScreenWidget::StaticClass(), TEXT("USRLoadingScreenWidget"));
	bRegistered = true;
}

void ASRSolarSystemGenerator::LogMemoryDiagnosticsSnapshot(const FString& Label) const
{
	if (!bEnableMemoryDiagnostics)
	{
		return;
	}

	EnsureMemoryDiagnosticTrackedClasses();

	TArray<FString> ExtraLines;
	ExtraLines.Add(FString::Printf(
		TEXT("GeneratorRefs Star=%s Planets=%d Moons=%d NaturalStructureActors=%d LoadingScreen=%s"),
		*GetNameSafe(RuntimeStarBody.Get()),
		RuntimePlanetBodies.Num(),
		RuntimeMoonBodies.Num(),
		RuntimeNaturalStructureActors.Num(),
		*GetNameSafe(LoadingScreenWidget.Get())));

	if (const UWorld* World = GetWorld())
	{
		ExtraLines.Add(FString::Printf(
			TEXT("GeneratorTimer DeferredGenerationActive=%s"),
			World->GetTimerManager().IsTimerActive(DeferredGenerateRuntimeSystemTimerHandle) ? TEXT("true") : TEXT("false")));
	}

	ASRCelestialBody::AppendRuntimeMemoryDiagnostics(ExtraLines);
	FSRMemoryDiagnostics::LogSnapshot(GetWorld(), Label, ExtraLines);
}

void ASRSolarSystemGenerator::StartRuntimeSystemGenerationWithLoadingScreen()
{
	ShowLoadingScreen();
	UpdateLoadingProgress(0.0f, NSLOCTEXT("StarRoversLoadingScreen", "Initializing", "Initializing generation..."));

	ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::GenerateRuntimeSystemDeferred, 0.20f);
}

void ASRSolarSystemGenerator::GenerateRuntimeSystemDeferred()
{
	BeginRuntimeSystemGenerationDeferred();
}

void ASRSolarSystemGenerator::BeginRuntimeSystemGenerationDeferred()
{
	if (!GetWorld())
	{
		HideLoadingScreen();
		return;
	}

	bRuntimeGenerationInProgress = true;
	AsyncGenerationStageTimings.Reset();
	AsyncGenerationTotalStart = SRSolarNowSeconds();
	FSRTimingLog::BeginSession(TEXT("GenerateRuntimeSystem"));

	UpdateLoadingProgress(0.02f, NSLOCTEXT("StarRoversLoadingScreen", "Clearing", "Clearing previous system..."));
	LogMemoryDiagnosticsSnapshot(TEXT("GenerateRuntimeSystem.BeforeClear"));
	AsyncCurrentStageStart = SRSolarNowSeconds();
	ClearRuntimeGeneratedBodies();
	LogAsyncGenerationStageTiming(TEXT("ClearRuntimeGeneratedBodies"), SRSolarElapsedMilliseconds(AsyncCurrentStageStart));

	AsyncRuntimeGenerationSeed = bRandomizeGenerationSeedEachRun
		? CreateRuntimeRandomGenerationSeed()
		: GenerationSeed;
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("GenerateRuntimeSystem.Seed Configured=%d Runtime=%d Randomized=%s"),
		GenerationSeed,
		AsyncRuntimeGenerationSeed,
		bRandomizeGenerationSeedEachRun ? TEXT("true") : TEXT("false")));

	AsyncGenerationRandomStream = FRandomStream(AsyncRuntimeGenerationSeed);
	AsyncSelectedStarDataAsset = nullptr;

	UpdateLoadingProgress(0.08f, NSLOCTEXT("StarRoversLoadingScreen", "SpawningStar", "Creating primary star..."));
	AsyncCurrentStageStart = SRSolarNowSeconds();
	RuntimeStarBody = SpawnPrimaryStar(AsyncGenerationRandomStream, AsyncSelectedStarDataAsset);
	LogAsyncGenerationStageTiming(TEXT("SpawnPrimaryStar"), SRSolarElapsedMilliseconds(AsyncCurrentStageStart));
	if (!IsValid(RuntimeStarBody))
	{
		FinishRuntimeSystemGeneration();
		return;
	}

	UpdateLoadingProgress(0.14f, NSLOCTEXT("StarRoversLoadingScreen", "SpawningPlanets", "Creating planets..."));
	AsyncCurrentStageStart = SRSolarNowSeconds();
	SpawnPlanets(RuntimeStarBody, AsyncSelectedStarDataAsset, AsyncGenerationRandomStream, RuntimePlanetBodies);
	LogAsyncGenerationStageTiming(
		TEXT("SpawnPlanets"),
		SRSolarElapsedMilliseconds(AsyncCurrentStageStart),
		FString::Printf(TEXT(" Planets=%d Moons=%d"), RuntimePlanetBodies.Num(), RuntimeMoonBodies.Num()));

	AsyncPrepareBodyIndex = 0;
	AsyncPreparePlanetCount = 0;
	AsyncPrepareMoonCount = 0;
	AsyncPreparePlanetTotalMs = 0.0;
	AsyncPrepareMoonTotalMs = 0.0;
	AsyncPrepareSlowestBodyMs = 0.0;
	AsyncPrepareSlowestBodyName = TEXT("None");
	AsyncPrepareSlowestBodyDetailLines.Reset();
	AsyncDynamicMeshTotalStart = SRSolarNowSeconds();
	UpdateLoadingProgress(0.20f, NSLOCTEXT("StarRoversLoadingScreen", "PreparingSurfaces", "Preparing planet surfaces..."));
	ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::ContinueRuntimeDynamicMeshPreparation);
}

void ASRSolarSystemGenerator::ContinueRuntimeDynamicMeshPreparation()
{
	const int32 TotalBodies = RuntimePlanetBodies.Num() + RuntimeMoonBodies.Num();
	if (AsyncPrepareBodyIndex < RuntimePlanetBodies.Num())
	{
		ASRCelestialBody* Body = RuntimePlanetBodies[AsyncPrepareBodyIndex].Get();
		if (IsValid(Body))
		{
			TArray<FString> BodyDetailLines;
			const double BodyStart = SRSolarNowSeconds();
			{
				FSRTimingLogScopedCapture CaptureBodyDetailLogs(BodyDetailLines);
				Body->PrepareCelestialBodyDynamicMesh();
			}
			const double BodyMs = SRSolarElapsedMilliseconds(BodyStart);
			AsyncPreparePlanetTotalMs += BodyMs;
			++AsyncPreparePlanetCount;
			if (BodyMs > AsyncPrepareSlowestBodyMs)
			{
				AsyncPrepareSlowestBodyMs = BodyMs;
				AsyncPrepareSlowestBodyName = GetNameSafe(Body);
				AsyncPrepareSlowestBodyDetailLines = MoveTemp(BodyDetailLines);
			}
		}

		++AsyncPrepareBodyIndex;
		const float BodyProgress = TotalBodies > 0
			? static_cast<float>(AsyncPrepareBodyIndex) / static_cast<float>(TotalBodies)
			: 1.0f;
		UpdateLoadingProgress(
			FMath::Lerp(0.20f, 0.82f, BodyProgress),
			FText::FromString(FString::Printf(TEXT("Preparing planet surfaces... %d / %d"), FMath::Min(AsyncPrepareBodyIndex, TotalBodies), TotalBodies)));
		ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::ContinueRuntimeDynamicMeshPreparation);
		return;
	}

	const int32 MoonIndex = AsyncPrepareBodyIndex - RuntimePlanetBodies.Num();
	if (RuntimeMoonBodies.IsValidIndex(MoonIndex))
	{
		ASRCelestialBody* Body = RuntimeMoonBodies[MoonIndex].Get();
		if (IsValid(Body))
		{
			TArray<FString> BodyDetailLines;
			const double BodyStart = SRSolarNowSeconds();
			{
				FSRTimingLogScopedCapture CaptureBodyDetailLogs(BodyDetailLines);
				Body->PrepareCelestialBodyDynamicMesh();
			}
			const double BodyMs = SRSolarElapsedMilliseconds(BodyStart);
			AsyncPrepareMoonTotalMs += BodyMs;
			++AsyncPrepareMoonCount;
			if (BodyMs > AsyncPrepareSlowestBodyMs)
			{
				AsyncPrepareSlowestBodyMs = BodyMs;
				AsyncPrepareSlowestBodyName = GetNameSafe(Body);
				AsyncPrepareSlowestBodyDetailLines = MoveTemp(BodyDetailLines);
			}
		}

		++AsyncPrepareBodyIndex;
		const float BodyProgress = TotalBodies > 0
			? static_cast<float>(AsyncPrepareBodyIndex) / static_cast<float>(TotalBodies)
			: 1.0f;
		UpdateLoadingProgress(
			FMath::Lerp(0.20f, 0.82f, BodyProgress),
			FText::FromString(FString::Printf(TEXT("Preparing celestial surfaces... %d / %d"), FMath::Min(AsyncPrepareBodyIndex, TotalBodies), TotalBodies)));
		ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::ContinueRuntimeDynamicMeshPreparation);
		return;
	}

	FSRTimingLog::AddLine(FString::Printf(
		TEXT("PrepareRuntimeGeneratedDynamicMeshes.Total %.2f ms Bodies=%d Planets=%d PlanetTotal=%.2f ms Moons=%d MoonTotal=%.2f ms Slowest=%s SlowestMs=%.2f"),
		SRSolarElapsedMilliseconds(AsyncDynamicMeshTotalStart),
		AsyncPreparePlanetCount + AsyncPrepareMoonCount,
		AsyncPreparePlanetCount,
		AsyncPreparePlanetTotalMs,
		AsyncPrepareMoonCount,
		AsyncPrepareMoonTotalMs,
		*AsyncPrepareSlowestBodyName,
		AsyncPrepareSlowestBodyMs));
	if (!AsyncPrepareSlowestBodyDetailLines.IsEmpty())
	{
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("PrepareRuntimeGeneratedDynamicMeshes.SlowestDetail Body=%s Lines=%d"),
			*AsyncPrepareSlowestBodyName,
			AsyncPrepareSlowestBodyDetailLines.Num()));
		for (const FString& DetailLine : AsyncPrepareSlowestBodyDetailLines)
		{
			FSRTimingLog::AddLine(FString::Printf(TEXT("PrepareRuntimeGeneratedDynamicMeshes.SlowestDetail.%s"), *DetailLine));
		}
	}
	LogAsyncGenerationStageTiming(TEXT("PrepareRuntimeGeneratedDynamicMeshes"), SRSolarElapsedMilliseconds(AsyncDynamicMeshTotalStart));

	AsyncNaturalStructureRandomStream = FRandomStream(AsyncRuntimeGenerationSeed + 7919);
	AsyncNaturalPlanetIndex = 0;
	AsyncNaturalPlanetCount = 0;
	AsyncNaturalPlanetTotalMs = 0.0;
	AsyncNaturalSlowestBodyMs = 0.0;
	AsyncNaturalSlowestBodyName = TEXT("None");
	AsyncNaturalStructuresTotalStart = SRSolarNowSeconds();
	AsyncCurrentStageStart = SRSolarNowSeconds();
	DestroyRuntimeNaturalStructures();
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeNaturalStructures.DestroyExisting %.2f ms"), SRSolarElapsedMilliseconds(AsyncCurrentStageStart)));

	UpdateLoadingProgress(0.84f, NSLOCTEXT("StarRoversLoadingScreen", "GeneratingStructures", "Placing natural structures..."));
	ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::ContinueRuntimeNaturalStructureGeneration);
}

void ASRSolarSystemGenerator::ContinueRuntimeNaturalStructureGeneration()
{
	if (!bGenerateNaturalStructures)
	{
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeNaturalStructures.Total %.2f ms Disabled"), SRSolarElapsedMilliseconds(AsyncNaturalStructuresTotalStart)));
		LogAsyncGenerationStageTiming(TEXT("GenerateRuntimeNaturalStructures"), SRSolarElapsedMilliseconds(AsyncNaturalStructuresTotalStart));
		FinishRuntimeSystemGeneration();
		return;
	}

	if (RuntimePlanetBodies.IsValidIndex(AsyncNaturalPlanetIndex))
	{
		ASRCelestialBody* PlanetBody = RuntimePlanetBodies[AsyncNaturalPlanetIndex].Get();
		if (IsValid(PlanetBody))
		{
			const double BodyStart = SRSolarNowSeconds();
			{
				FSRTimingLogScopedSuppress SuppressBodyDetailLogs;
				GenerateNaturalStructuresForBody(PlanetBody, AsyncNaturalStructureRandomStream);
			}
			const double BodyMs = SRSolarElapsedMilliseconds(BodyStart);
			AsyncNaturalPlanetTotalMs += BodyMs;
			++AsyncNaturalPlanetCount;
			if (BodyMs > AsyncNaturalSlowestBodyMs)
			{
				AsyncNaturalSlowestBodyMs = BodyMs;
				AsyncNaturalSlowestBodyName = GetNameSafe(PlanetBody);
			}
		}

		++AsyncNaturalPlanetIndex;
		const float NaturalProgress = RuntimePlanetBodies.IsEmpty()
			? 1.0f
			: static_cast<float>(AsyncNaturalPlanetIndex) / static_cast<float>(RuntimePlanetBodies.Num());
		UpdateLoadingProgress(
			FMath::Lerp(0.84f, 0.96f, NaturalProgress),
			FText::FromString(FString::Printf(TEXT("Placing natural structures... %d / %d"), FMath::Min(AsyncNaturalPlanetIndex, RuntimePlanetBodies.Num()), RuntimePlanetBodies.Num())));
		ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::ContinueRuntimeNaturalStructureGeneration);
		return;
	}

	FSRTimingLog::AddLine(FString::Printf(
		TEXT("GenerateRuntimeNaturalStructures.Total %.2f ms Planets=%d PlanetTotal=%.2f ms Slowest=%s SlowestMs=%.2f"),
		SRSolarElapsedMilliseconds(AsyncNaturalStructuresTotalStart),
		AsyncNaturalPlanetCount,
		AsyncNaturalPlanetTotalMs,
		*AsyncNaturalSlowestBodyName,
		AsyncNaturalSlowestBodyMs));
	LogAsyncGenerationStageTiming(TEXT("GenerateRuntimeNaturalStructures"), SRSolarElapsedMilliseconds(AsyncNaturalStructuresTotalStart));
	FinishRuntimeSystemGeneration();
}

void ASRSolarSystemGenerator::FinishRuntimeSystemGeneration()
{
	UpdateLoadingProgress(0.98f, NSLOCTEXT("StarRoversLoadingScreen", "Finalizing", "Finalizing star system..."));
	AsyncCurrentStageStart = SRSolarNowSeconds();
	if (UWorld* World = GetWorld())
	{
		if (USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
		{
			CelestialBodyRegistry->SetPrimaryStarActor(RuntimeStarBody);
		}
	}
	LogAsyncGenerationStageTiming(TEXT("Registry"), SRSolarElapsedMilliseconds(AsyncCurrentStageStart));

	const FSRAsyncGenerationStageTiming* SlowestStageTiming = nullptr;
	for (const FSRAsyncGenerationStageTiming& StageTiming : AsyncGenerationStageTimings)
	{
		if (!SlowestStageTiming || StageTiming.Milliseconds > SlowestStageTiming->Milliseconds)
		{
			SlowestStageTiming = &StageTiming;
		}
	}
	if (SlowestStageTiming)
	{
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.Bottleneck Stage=%s %.2f ms"), *SlowestStageTiming->Name, SlowestStageTiming->Milliseconds));
	}
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.Total %.2f ms"), SRSolarElapsedMilliseconds(AsyncGenerationTotalStart)));
	LogMemoryDiagnosticsSnapshot(TEXT("GenerateRuntimeSystem.AfterComplete"));

	UpdateLoadingProgress(1.0f, NSLOCTEXT("StarRoversLoadingScreen", "Complete", "Complete"));
	FSRTimingLog::EndSessionAndLog();
	bRuntimeGenerationInProgress = false;
	ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::HideLoadingScreen, 0.05f);
}

void ASRSolarSystemGenerator::ShowLoadingScreen()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !LoadingScreenWidgetClass)
	{
		return;
	}

	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	LoadingScreenWidget = CreateWidget<USRLoadingScreenWidget>(PlayerController, LoadingScreenWidgetClass);
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->AddToViewport(LoadingScreenZOrder);
		LoadingScreenWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void ASRSolarSystemGenerator::HideLoadingScreen()
{
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
	}
}

void ASRSolarSystemGenerator::UpdateLoadingProgress(float Progress, const FText& StatusText)
{
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->SetLoadingProgress(Progress, StatusText);
	}
}

void ASRSolarSystemGenerator::ScheduleLoadingGenerationStep(void (ASRSolarSystemGenerator::*StepFunction)(), float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (DelaySeconds > KINDA_SMALL_NUMBER)
	{
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUObject(this, StepFunction);
		World->GetTimerManager().SetTimer(DeferredGenerateRuntimeSystemTimerHandle, TimerDelegate, DelaySeconds, false);
	}
	else
	{
		DeferredGenerateRuntimeSystemTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, StepFunction);
	}
}

void ASRSolarSystemGenerator::LogAsyncGenerationStageTiming(const TCHAR* StageName, double Milliseconds, const FString& Suffix)
{
	AsyncGenerationStageTimings.Add({ FString(StageName), Milliseconds });
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.%s %.2f ms%s"), StageName, Milliseconds, *Suffix));
}

ASRCelestialBody* ASRSolarSystemGenerator::SpawnPrimaryStar(FRandomStream& RandomStream, const USRStarDataAsset*& OutSelectedStarDataAsset)
{
	OutSelectedStarDataAsset = nullptr;
	UWorld* World = GetWorld();
	const TSubclassOf<ASRCelestialBody> ResolvedPrimaryStarClass = ValidateRuntimeCelestialClass(StarClass, TEXT("StarClass"));
	if (!World || !ResolvedPrimaryStarClass)
	{
		return nullptr;
	}

	const USRStarDataAsset* SelectedStarDataAsset = ResolveRandomDataAssetStrict(StarDataAssets, RandomStream, TEXT("star"));
	if (!IsValid(SelectedStarDataAsset))
	{
		return nullptr;
	}
	OutSelectedStarDataAsset = SelectedStarDataAsset;

	FSRCelestialBodyGenerateRequest StarCelestialBodyRequest;
	if (!TryBuildRequestFromDataAsset(ResolvedPrimaryStarClass, SelectedStarDataAsset, StarCelestialBodyRequest))
	{
		return nullptr;
	}
	StarCelestialBodyRequest.BodyData.ParentBody = nullptr;
	StarCelestialBodyRequest.BodyData.OrbitRadius = 0.0f;
	StarCelestialBodyRequest.BodyData.OrbitPeriod = 0.0f;
	StarCelestialBodyRequest.BodyData.InitialAngle = 0.0f;
	if (ShouldRandomizeBodyGenerationSeed(StarCelestialBodyRequest.BodyData))
	{
		ApplyResolvedGenerationSeed(StarCelestialBodyRequest.BodyData, CreateRuntimeRandomGenerationSeed());
	}

	return SpawnOrbitingBody(ResolvedPrimaryStarClass, StarCelestialBodyRequest, nullptr);
}

ASRCelestialBody* ASRSolarSystemGenerator::SpawnOrbitingBody(const TSubclassOf<ASRCelestialBody>& BodyClass, const FSRCelestialBodyGenerateRequest& CelestialBodyRequest, ASRCelestialBody* ParentBody)
{
	UWorld* World = GetWorld();
	if (!World || !BodyClass)
	{
		return nullptr;
	}

	const FVector SpawnLocation = IsValid(ParentBody)
		? ComputeOrbitWorldLocation(ParentBody, CelestialBodyRequest.BodyData.OrbitRadius, CelestialBodyRequest.BodyData.InitialAngle)
		: GetActorLocation();
	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	ASRCelestialBody* GeneratedCelestialBody = World->SpawnActorDeferred<ASRCelestialBody>(
		BodyClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(GeneratedCelestialBody))
	{
		return nullptr;
	}

	GeneratedCelestialBody->SetData(CelestialBodyRequest.BodyData);
	GeneratedCelestialBody->FinishSpawning(SpawnTransform);

#if WITH_EDITOR
	if (!CelestialBodyRequest.BodyData.VariableName.IsEmpty())
	{
		GeneratedCelestialBody->SetActorLabel(CelestialBodyRequest.BodyData.VariableName.ToString());
	}
#endif

	return GeneratedCelestialBody;
}

void ASRSolarSystemGenerator::BuildOrbitingBodyRequests(
	ASRCelestialBody* ParentBody,
	int32 RequestedBodyCount,
	const TArray<FSRCelestialBodyGenerateRequest>& CandidateCelestialBodyRequests,
	FRandomStream& RandomStream,
	TArray<FSRCelestialBodyGenerateRequest>& OutResolvedCelestialBodyRequests) const
{
	OutResolvedCelestialBodyRequests.Reset();
	if (!IsValid(ParentBody) || RequestedBodyCount <= 0 || CandidateCelestialBodyRequests.IsEmpty())
	{
		return;
	}

	const int32 CandidateCount = FMath::Min(RequestedBodyCount, CandidateCelestialBodyRequests.Num());
	TArray<FSRCelestialBodyGenerateRequest> CandidateCelestialBodies;
	CandidateCelestialBodies.Reserve(CandidateCount);
	for (int32 Index = 0; Index < CandidateCount; ++Index)
	{
		CandidateCelestialBodies.Add(CandidateCelestialBodyRequests[Index]);
	}

	TArray<float> PackedOrbitRadii;
	if (!TrySolvePackedOrbitRadii(ParentBody, CandidateCelestialBodies, PackedOrbitRadii))
	{
		return;
	}

	for (int32 Index = 0; Index < CandidateCelestialBodies.Num(); ++Index)
	{
		CandidateCelestialBodies[Index].BodyData.OrbitRadius = PackedOrbitRadii[Index];
		CandidateCelestialBodies[Index].BodyData.InitialAngle = RandomStream.FRandRange(0.0f, 360.0f);
		const int32 ResolvedGenerationSeed = ShouldRandomizeBodyGenerationSeed(CandidateCelestialBodies[Index].BodyData)
			? CreateRuntimeRandomGenerationSeed()
			: RandomStream.RandRange(1, TNumericLimits<int32>::Max() - 1);
		ApplyResolvedGenerationSeed(CandidateCelestialBodies[Index].BodyData, ResolvedGenerationSeed);
	}

	OutResolvedCelestialBodyRequests = MoveTemp(CandidateCelestialBodies);
}

bool ASRSolarSystemGenerator::TrySolvePackedOrbitRadii(ASRCelestialBody* ParentBody, const TArray<FSRCelestialBodyGenerateRequest>& CelestialBodyRequests, TArray<float>& OutOrbitRadii) const
{
	OutOrbitRadii.Reset();

	if (!IsValid(ParentBody) || CelestialBodyRequests.IsEmpty())
	{
		return false;
	}

	const float ParentBodyRadius = ComputeScaledBodyRadius(ParentBody);
	const USRGravityParent* ParentGravityParent = ParentBody->GetGravityParent();
	if (!IsValid(ParentGravityParent))
	{
		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires GravityParent on '%s'."), *ParentBody->GetName());
		return false;
	}
	const float ParentGravityRadius = ParentGravityParent->GetGravityRadius();

	TArray<FSROrbitInfo> OrbitInfos;
	OrbitInfos.Reserve(CelestialBodyRequests.Num());

	const bool bParentIsStar = ParentBody->GetBodyCategory() == ESRCelestialBodyCategory::Star;
	const float InitialOrbit = bParentIsStar ? PlanetInitialOrbit : MoonInitialOrbit;
	const float OrbitIncrease = bParentIsStar ? PlanetOrbitIncrease : MoonOrbitIncrease;

	for (int32 BodyIndex = 0; BodyIndex < CelestialBodyRequests.Num(); ++BodyIndex)
	{
		float BodyRadius = 0.0f;
		if (!TryComputeScaledBodyRadiusFromCelestialBodyRequest(CelestialBodyRequests[BodyIndex], BodyRadius))
		{
			return false;
		}
		const float GravityRadius = ComputeGravityRadiusFromCelestialBodyRequest(CelestialBodyRequests[BodyIndex]);
		FSROrbitInfo& OrbitInfo = OrbitInfos.AddDefaulted_GetRef();
		OrbitInfo.OrbitingBodyExtent = FMath::Max(BodyRadius, GravityRadius);
		OrbitInfo.DesiredOrbitRadius = InitialOrbit + (OrbitIncrease * static_cast<float>(BodyIndex));
	}

	OutOrbitRadii.SetNumUninitialized(OrbitInfos.Num());
	float NextMinimumInnerEdge = ParentBodyRadius;
	float RequiredParentGravityRadius = NextMinimumInnerEdge;
	for (int32 BodyIndex = 0; BodyIndex < OrbitInfos.Num(); ++BodyIndex)
	{
		const FSROrbitInfo& OrbitInfo = OrbitInfos[BodyIndex];
		const float MinimumCenterRadius = NextMinimumInnerEdge + OrbitInfo.OrbitingBodyExtent;
		const float OrbitRadius = FMath::Max(OrbitInfo.DesiredOrbitRadius, MinimumCenterRadius);
		OutOrbitRadii[BodyIndex] = OrbitRadius;

		const float OuterEdge = OrbitRadius + OrbitInfo.OrbitingBodyExtent;
		RequiredParentGravityRadius = OuterEdge;
		NextMinimumInnerEdge = OuterEdge;
	}

	if (ParentGravityRadius + KINDA_SMALL_NUMBER < RequiredParentGravityRadius)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Solar system generation requires '%s' gravity radius %.2f to be at least %.2f for %d orbiting bodies."),
			*ParentBody->GetName(),
			ParentGravityRadius,
			RequiredParentGravityRadius,
			CelestialBodyRequests.Num());
		return false;
	}

	return true;
}

void ASRSolarSystemGenerator::EnsureParentGravityContainsOrbitingBody(ASRCelestialBody* ParentBody, const ASRCelestialBody* OrbitingBody) const
{
	if (!IsValid(ParentBody) || !IsValid(OrbitingBody))
	{
		return;
	}

	const USRGravityParent* ParentGravityParent = ParentBody->GetGravityParent();
	if (!IsValid(ParentGravityParent))
	{
		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires GravityParent on '%s'."), *ParentBody->GetName());
		return;
	}
	const float ParentGravityRadius = ParentGravityParent->GetGravityRadius();

	const float OrbitRadius = FVector::Dist(ParentBody->GetActorLocation(), OrbitingBody->GetActorLocation());
	const float OrbitingBodyRadius = ComputeScaledBodyRadius(OrbitingBody);
	const USRGravityParent* OrbitingBodyGravityParent = OrbitingBody->GetGravityParent();
	if (!IsValid(OrbitingBodyGravityParent))
	{
		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires GravityParent on '%s'."), *OrbitingBody->GetName());
		return;
	}
	const float OrbitingBodyGravityRadius = OrbitingBodyGravityParent->GetGravityRadius();
	const float OrbitingBodyExtent = FMath::Max(OrbitingBodyRadius, OrbitingBodyGravityRadius);
	const float RequiredParentGravityRadius = OrbitRadius + OrbitingBodyExtent;
	if (ParentGravityRadius + KINDA_SMALL_NUMBER >= RequiredParentGravityRadius)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("Solar system generation requires '%s' gravity radius %.2f to be at least %.2f to contain orbiting body '%s'."),
		*ParentBody->GetName(),
		ParentGravityRadius,
		RequiredParentGravityRadius,
		*OrbitingBody->GetName());
}

FVector ASRSolarSystemGenerator::ComputeOrbitWorldLocation(const AActor* ParentBody, float OrbitRadius, float InitialAngleDegrees) const
{
	const FVector ParentLocation = IsValid(ParentBody) ? ParentBody->GetActorLocation() : GetActorLocation();
	const float PhaseRadians = FMath::DegreesToRadians(InitialAngleDegrees);

	return FVector(
		ParentLocation.X,
		ParentLocation.Y + (FMath::Cos(PhaseRadians) * OrbitRadius),
		ParentLocation.Z + (FMath::Sin(PhaseRadians) * OrbitRadius));
}

void ASRSolarSystemGenerator::SpawnPlanets(ASRCelestialBody* ParentStar, const USRStarDataAsset* SourceStarDataAsset, FRandomStream& RandomStream, TArray<TObjectPtr<ASRCelestialBody>>& OutGeneratedPlanets)
{
	OutGeneratedPlanets.Reset();
	if (!IsValid(ParentStar) || !IsValid(SourceStarDataAsset))
	{
		return;
	}

	const int32 ResolvedMinPlanetCount = FMath::Max(0, MinPlanet);
	const int32 ResolvedMaxPlanetCount = FMath::Max(ResolvedMinPlanetCount, MaxPlanet);
	const int32 RequestedPlanetCount = RandomStream.RandRange(ResolvedMinPlanetCount, ResolvedMaxPlanetCount);
	if (RequestedPlanetCount <= 0)
	{
		return;
	}

	TArray<FSRCelestialBodyGenerateRequest> CandidatePlanetCelestialBodyRequests;
	CandidatePlanetCelestialBodyRequests.Reserve(RequestedPlanetCount);
	const TSubclassOf<ASRCelestialBody> ResolvedPlanetClass = ValidateRuntimeCelestialClass(PlanetClass, TEXT("PlanetClass"));
	if (!ResolvedPlanetClass)
	{
		return;
	}

	for (int32 PlanetIndex = 0; PlanetIndex < RequestedPlanetCount; ++PlanetIndex)
	{
		const USRPlanetDataAsset* SelectedPlanetData = ResolveRandomDataAssetStrict(PlanetDataAssets, RandomStream, TEXT("planet"));
		if (!IsValid(SelectedPlanetData))
		{
			return;
		}

		FSRCelestialBodyGenerateRequest PlanetCelestialBodyRequest;
		if (!TryBuildRequestFromDataAsset(ResolvedPlanetClass, SelectedPlanetData, PlanetCelestialBodyRequest))
		{
			return;
		}

		PlanetCelestialBodyRequest.BodyData.ParentBody = ParentStar;
		CandidatePlanetCelestialBodyRequests.Add(PlanetCelestialBodyRequest);
	}

	if (CandidatePlanetCelestialBodyRequests.IsEmpty())
	{
		return;
	}

	{
		TArray<FSRCelestialBodyGenerateRequest> ResolvedPlanetCelestialBodyRequests;
		BuildOrbitingBodyRequests(ParentStar, CandidatePlanetCelestialBodyRequests.Num(), CandidatePlanetCelestialBodyRequests, RandomStream, ResolvedPlanetCelestialBodyRequests);
		for (int32 PlanetIndex = 0; PlanetIndex < ResolvedPlanetCelestialBodyRequests.Num(); ++PlanetIndex)
		{
			if (ASRCelestialBody* GeneratedPlanet = SpawnOrbitingBody(ResolvedPlanetCelestialBodyRequests[PlanetIndex].BodyClass, ResolvedPlanetCelestialBodyRequests[PlanetIndex], ParentStar))
			{
				OutGeneratedPlanets.Add(GeneratedPlanet);
				SpawnMoons(GeneratedPlanet, RandomStream, RuntimeMoonBodies);
				EnsureParentGravityContainsOrbitingBody(ParentStar, GeneratedPlanet);
			}
		}
	}
}

void ASRSolarSystemGenerator::SpawnMoons(ASRCelestialBody* ParentPlanet, FRandomStream& RandomStream, TArray<TObjectPtr<ASRCelestialBody>>& OutGeneratedMoons)
{
	if (!IsValid(ParentPlanet))
	{
		return;
	}

	const int32 ResolvedMinMoonCount = FMath::Max(0, MinMoon);
	const int32 ResolvedMaxMoonCount = FMath::Max(ResolvedMinMoonCount, MaxMoon);
	const int32 RequestedMoonCount = RandomStream.RandRange(ResolvedMinMoonCount, ResolvedMaxMoonCount);
	if (RequestedMoonCount <= 0)
	{
		return;
	}

	TArray<FSRCelestialBodyGenerateRequest> CandidateMoonCelestialBodyRequests;
	CandidateMoonCelestialBodyRequests.Reserve(RequestedMoonCount);
	const TSubclassOf<ASRCelestialBody> ResolvedMoonClass = ValidateRuntimeCelestialClass(PlanetClass, TEXT("PlanetClass for moons"));
	if (!ResolvedMoonClass)
	{
		return;
	}

	for (int32 MoonIndex = 0; MoonIndex < RequestedMoonCount; ++MoonIndex)
	{
		const USRMoonDataAsset* SelectedMoonData = ResolveRandomDataAssetStrict(MoonDataAssets, RandomStream, TEXT("moon"));
		if (!IsValid(SelectedMoonData))
		{
			return;
		}

		FSRCelestialBodyGenerateRequest MoonCelestialBodyRequest;
		if (!TryBuildRequestFromDataAsset(ResolvedMoonClass, SelectedMoonData, MoonCelestialBodyRequest))
		{
			return;
		}

		MoonCelestialBodyRequest.BodyData.ParentBody = ParentPlanet;
		CandidateMoonCelestialBodyRequests.Add(MoonCelestialBodyRequest);
	}

	if (CandidateMoonCelestialBodyRequests.IsEmpty())
	{
		return;
	}

	{
		TArray<FSRCelestialBodyGenerateRequest> ResolvedMoonCelestialBodyRequests;
		BuildOrbitingBodyRequests(ParentPlanet, CandidateMoonCelestialBodyRequests.Num(), CandidateMoonCelestialBodyRequests, RandomStream, ResolvedMoonCelestialBodyRequests);
		for (int32 MoonIndex = 0; MoonIndex < ResolvedMoonCelestialBodyRequests.Num(); ++MoonIndex)
		{
			if (ASRCelestialBody* GeneratedMoon = SpawnOrbitingBody(ResolvedMoonCelestialBodyRequests[MoonIndex].BodyClass, ResolvedMoonCelestialBodyRequests[MoonIndex], ParentPlanet))
			{
				OutGeneratedMoons.Add(GeneratedMoon);
			}
		}
	}
}

void ASRSolarSystemGenerator::PrepareRuntimeGeneratedDynamicMeshes()
{
	const double TotalStart = SRSolarNowSeconds();
	int32 PlanetCount = 0;
	int32 MoonCount = 0;
	double PlanetTotalMs = 0.0;
	double MoonTotalMs = 0.0;
	double SlowestBodyMs = 0.0;
	FString SlowestBodyName(TEXT("None"));
	TArray<FString> SlowestBodyDetailLines;

	auto PrepareBody = [&SlowestBodyMs, &SlowestBodyName, &SlowestBodyDetailLines](ASRCelestialBody* Body)
	{
		TArray<FString> BodyDetailLines;
		const double BodyStart = SRSolarNowSeconds();
		{
			FSRTimingLogScopedCapture CaptureBodyDetailLogs(BodyDetailLines);
			Body->PrepareCelestialBodyDynamicMesh();
		}
		const double BodyMs = SRSolarElapsedMilliseconds(BodyStart);
		if (BodyMs > SlowestBodyMs)
		{
			SlowestBodyMs = BodyMs;
			SlowestBodyName = GetNameSafe(Body);
			SlowestBodyDetailLines = MoveTemp(BodyDetailLines);
		}
		return BodyMs;
	};

	for (TObjectPtr<ASRCelestialBody>& PlanetBody : RuntimePlanetBodies)
	{
		if (IsValid(PlanetBody))
		{
			PlanetTotalMs += PrepareBody(PlanetBody.Get());
			++PlanetCount;
		}
	}

	for (TObjectPtr<ASRCelestialBody>& MoonBody : RuntimeMoonBodies)
	{
		if (IsValid(MoonBody))
		{
			MoonTotalMs += PrepareBody(MoonBody.Get());
			++MoonCount;
		}
	}
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("PrepareRuntimeGeneratedDynamicMeshes.Total %.2f ms Bodies=%d Planets=%d PlanetTotal=%.2f ms Moons=%d MoonTotal=%.2f ms Slowest=%s SlowestMs=%.2f"),
		SRSolarElapsedMilliseconds(TotalStart),
		PlanetCount + MoonCount,
		PlanetCount,
		PlanetTotalMs,
		MoonCount,
		MoonTotalMs,
		*SlowestBodyName,
		SlowestBodyMs));
	if (!SlowestBodyDetailLines.IsEmpty())
	{
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("PrepareRuntimeGeneratedDynamicMeshes.SlowestDetail Body=%s Lines=%d"),
			*SlowestBodyName,
			SlowestBodyDetailLines.Num()));
		for (const FString& DetailLine : SlowestBodyDetailLines)
		{
			FSRTimingLog::AddLine(FString::Printf(TEXT("PrepareRuntimeGeneratedDynamicMeshes.SlowestDetail.%s"), *DetailLine));
		}
	}
}

void ASRSolarSystemGenerator::GenerateRuntimeNaturalStructures(int32 RuntimeGenerationSeed)
{
	const double TotalStart = SRSolarNowSeconds();
	double StageStart = SRSolarNowSeconds();
	DestroyRuntimeNaturalStructures();
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeNaturalStructures.DestroyExisting %.2f ms"), SRSolarElapsedMilliseconds(StageStart)));
	if (!bGenerateNaturalStructures)
	{
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeNaturalStructures.Total %.2f ms Disabled"), SRSolarElapsedMilliseconds(TotalStart)));
		return;
	}

	FRandomStream NaturalStructureRandomStream(RuntimeGenerationSeed + 7919);
	int32 PlanetCount = 0;
	double PlanetTotalMs = 0.0;
	double SlowestBodyMs = 0.0;
	FString SlowestBodyName(TEXT("None"));
	for (TObjectPtr<ASRCelestialBody>& PlanetBody : RuntimePlanetBodies)
	{
		if (IsValid(PlanetBody))
		{
			const double BodyStart = SRSolarNowSeconds();
			{
				FSRTimingLogScopedSuppress SuppressBodyDetailLogs;
				GenerateNaturalStructuresForBody(PlanetBody, NaturalStructureRandomStream);
			}
			const double BodyMs = SRSolarElapsedMilliseconds(BodyStart);
			PlanetTotalMs += BodyMs;
			++PlanetCount;
			if (BodyMs > SlowestBodyMs)
			{
				SlowestBodyMs = BodyMs;
				SlowestBodyName = GetNameSafe(PlanetBody.Get());
			}
		}
	}
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("GenerateRuntimeNaturalStructures.Total %.2f ms Planets=%d PlanetTotal=%.2f ms Slowest=%s SlowestMs=%.2f"),
		SRSolarElapsedMilliseconds(TotalStart),
		PlanetCount,
		PlanetTotalMs,
		*SlowestBodyName,
		SlowestBodyMs));
}

void ASRSolarSystemGenerator::GenerateNaturalStructuresForBody(ASRCelestialBody* Body, FRandomStream& RandomStream)
{
	const double TotalStart = SRSolarNowSeconds();
	if (!IsValid(Body) || Body->GetBodyCategory() != ESRCelestialBodyCategory::Planet)
	{
		return;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = Body->GetSurfaceGrid();
	if (!IsValid(SurfaceGrid) || SurfaceGrid->GetCellCount() <= 0)
	{
		return;
	}

	double StageStart = SRSolarNowSeconds();
	const TArray<FSRPlanetSurfaceGridCell>& Cells = SurfaceGrid->GetCellsRef();
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.GetCellsRef '%s' %.2f ms Cells=%d"), *GetNameSafe(Body), SRSolarElapsedMilliseconds(StageStart), Cells.Num()));

	TMap<FName, TArray<int32>> CandidateCellIndicesByBiomeId;
	auto GetBiomeCandidateCellIndices = [&Cells, &CandidateCellIndicesByBiomeId](FName BiomeId) -> const TArray<int32>&
	{
		if (const TArray<int32>* ExistingCandidateCellIndices = CandidateCellIndicesByBiomeId.Find(BiomeId))
		{
			return *ExistingCandidateCellIndices;
		}

		const double BuildStart = SRSolarNowSeconds();
		TArray<int32>& CandidateCellIndices = CandidateCellIndicesByBiomeId.Add(BiomeId);
		CandidateCellIndices.Reserve(Cells.Num());
		for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
		{
			const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
			if (Cell.BiomeId == BiomeId && !Cell.bOccupied)
			{
				CandidateCellIndices.Add(CellIndex);
			}
		}
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.BuildBiomeIdCandidateIndices '%s' %.2f ms Candidates=%d"), *BiomeId.ToString(), SRSolarElapsedMilliseconds(BuildStart), CandidateCellIndices.Num()));
		return CandidateCellIndices;
	};

	bool bLoggedMissingStructureDataAsset = false;
	auto GenerateRuleForCandidateCells = [this, Body, SurfaceGrid, &Cells, &RandomStream, &bLoggedMissingStructureDataAsset](
		const TArray<int32>& CandidateCellIndices,
		USRStructureDataAsset* StructureDataAsset,
		float SpawnChancePerCell,
		int32 MaxCount,
		int32 MinCellSpacing)
	{
		const double RuleStart = SRSolarNowSeconds();
		const int32 InitialCandidateCount = CandidateCellIndices.Num();
		if (!IsValid(StructureDataAsset))
		{
			if (!bLoggedMissingStructureDataAsset)
			{
				bLoggedMissingStructureDataAsset = true;
				UE_LOG(LogTemp, Error, TEXT("Natural structure generation for '%s' has one or more rules without StructureDataAsset."), *GetNameSafe(Body));
			}
			return;
		}

		if (CandidateCellIndices.IsEmpty())
		{
			FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.Rule '%s' %.2f ms Candidates=0 Placed=0"), *GetNameSafe(StructureDataAsset), SRSolarElapsedMilliseconds(RuleStart)));
			return;
		}

		double StageStart = SRSolarNowSeconds();
		TArray<int32> CandidateIterationIndices = CandidateCellIndices;
		const int32 SafeMaxCount = FMath::Max(0, MaxCount);
		const float SafeSpawnChancePerCell = FMath::Clamp(SpawnChancePerCell, 0.0f, 1.0f);
		const int32 MinimumCandidateAttempts = SafeMaxCount > 0
			? SafeMaxCount * FMath::Max(4, FMath::CeilToInt(1.0f / FMath::Max(SafeSpawnChancePerCell, 0.05f)))
			: CandidateIterationIndices.Num();
		const int32 PartialShuffleCount = FMath::Clamp(
			FMath::Max(MinimumCandidateAttempts, 1024),
			0,
			CandidateIterationIndices.Num());
		for (int32 CandidateIndex = 0; CandidateIndex < PartialShuffleCount; ++CandidateIndex)
		{
			const int32 SwapIndex = RandomStream.RandRange(CandidateIndex, CandidateIterationIndices.Num() - 1);
			if (SwapIndex != CandidateIndex)
			{
				CandidateIterationIndices.Swap(CandidateIndex, SwapIndex);
			}
		}
		CandidateIterationIndices.SetNum(PartialShuffleCount, EAllowShrinking::No);
		const double ShuffleMs = SRSolarElapsedMilliseconds(StageStart);

		TArray<FSRPlanetSurfaceGridCellId> PlacedOriginCellIds;
		const int32 SafeMinCellSpacing = FMath::Max(0, MinCellSpacing);
		int32 PlacedCount = 0;
		for (const int32 CandidateCellIndex : CandidateIterationIndices)
		{
			if (SafeMaxCount > 0 && PlacedCount >= SafeMaxCount)
			{
				break;
			}

			if (!Cells.IsValidIndex(CandidateCellIndex))
			{
				continue;
			}
			const FSRPlanetSurfaceGridCell& CandidateCell = Cells[CandidateCellIndex];

			if (SafeSpawnChancePerCell < 1.0f && RandomStream.FRand() > SafeSpawnChancePerCell)
			{
				continue;
			}

			bool bTooCloseToPlacedStructure = false;
			for (const FSRPlanetSurfaceGridCellId& PlacedOriginCellId : PlacedOriginCellIds)
			{
				if (PlacedOriginCellId.Face == CandidateCell.CellId.Face
					&& FMath::Abs(PlacedOriginCellId.CellX - CandidateCell.CellId.CellX) <= SafeMinCellSpacing
					&& FMath::Abs(PlacedOriginCellId.CellY - CandidateCell.CellId.CellY) <= SafeMinCellSpacing)
				{
					bTooCloseToPlacedStructure = true;
					break;
				}
			}
			if (bTooCloseToPlacedStructure)
			{
				continue;
			}

			bool bPlacedStructure = false;
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = Body->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				FName OccupantId = NAME_None;
				bPlacedStructure = StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(SurfaceGrid, CandidateCell.CellId, StructureDataAsset, OccupantId, true);
			}

			if (!bPlacedStructure)
			{
				AActor* PlacedStructureActor = nullptr;
				bPlacedStructure = USRStructurePlacementLibrary::TryPlaceStructureOnSurfaceGrid(SurfaceGrid, CandidateCell.CellId, StructureDataAsset, PlacedStructureActor);
				if (bPlacedStructure)
				{
					RuntimeNaturalStructureActors.Add(PlacedStructureActor);
				}
			}

			if (bPlacedStructure)
			{
				PlacedOriginCellIds.Add(CandidateCell.CellId);
				++PlacedCount;
			}
		}
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("GenerateNaturalStructuresForBody.Rule '%s' %.2f ms Candidates=%d Iteration=%d Placed=%d Shuffle=%.2f ms"),
			*GetNameSafe(StructureDataAsset),
			SRSolarElapsedMilliseconds(RuleStart),
			InitialCandidateCount,
			CandidateIterationIndices.Num(),
			PlacedCount,
			ShuffleMs));
	};

	auto BuildProfileCandidateCellIndices = [&Cells]()
	{
		const double BuildStart = SRSolarNowSeconds();
		TArray<int32> CandidateCellIndices;
		CandidateCellIndices.Reserve(Cells.Num());
		for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
		{
			const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
			if (!Cell.bOccupied)
			{
				CandidateCellIndices.Add(CellIndex);
			}
		}
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.BuildProfileCandidateIndices %.2f ms Candidates=%d"), SRSolarElapsedMilliseconds(BuildStart), CandidateCellIndices.Num()));
		return CandidateCellIndices;
	};

	const FSRCelestialBodyData BodyData = Body->GetData();
	auto FindRuleOverride = [](const TArray<FSRNaturalStructureSpawnRuleOverride>& Overrides, FName RuleId)
	{
		if (RuleId.IsNone())
		{
			return static_cast<const FSRNaturalStructureSpawnRuleOverride*>(nullptr);
		}

		return Overrides.FindByPredicate([RuleId](const FSRNaturalStructureSpawnRuleOverride& Override)
		{
			return Override.RuleId == RuleId;
		});
	};

	auto GenerateProfileRule = [&GenerateRuleForCandidateCells, &BuildProfileCandidateCellIndices, &FindRuleOverride](
		const FSRProfileNaturalStructureSpawnRule& Rule,
		const TArray<FSRNaturalStructureSpawnRuleOverride>& RuleOverrides)
	{
		const FSRNaturalStructureSpawnRuleOverride* RuleOverride = FindRuleOverride(RuleOverrides, Rule.RuleId);
		const bool bRuleEnabled = RuleOverride ? RuleOverride->bEnabled : Rule.bEnabled;
		if (!bRuleEnabled)
		{
			return;
		}

		GenerateRuleForCandidateCells(
			BuildProfileCandidateCellIndices(),
			Rule.StructureDataAsset.Get(),
			RuleOverride ? RuleOverride->SpawnChancePerCell : Rule.SpawnChancePerCell,
			RuleOverride ? RuleOverride->MaxCount : Rule.MaxCount,
			RuleOverride ? RuleOverride->MinCellSpacing : Rule.MinCellSpacing);
	};

	auto GenerateBiomeRule = [&GenerateRuleForCandidateCells, &GetBiomeCandidateCellIndices, &FindRuleOverride](
		FName BiomeId,
		const FSRProfileNaturalStructureSpawnRule& Rule,
		const TArray<FSRNaturalStructureSpawnRuleOverride>& RuleOverrides)
	{
		const FSRNaturalStructureSpawnRuleOverride* RuleOverride = FindRuleOverride(RuleOverrides, Rule.RuleId);
		const bool bRuleEnabled = RuleOverride ? RuleOverride->bEnabled : Rule.bEnabled;
		if (!bRuleEnabled)
		{
			return;
		}

		GenerateRuleForCandidateCells(
			GetBiomeCandidateCellIndices(BiomeId),
			Rule.StructureDataAsset.Get(),
			RuleOverride ? RuleOverride->SpawnChancePerCell : Rule.SpawnChancePerCell,
			RuleOverride ? RuleOverride->MaxCount : Rule.MaxCount,
			RuleOverride ? RuleOverride->MinCellSpacing : Rule.MinCellSpacing);
	};

	if (USRPlanetTerrainProfileDataAsset* TerrainProfileDataAsset = BodyData.TerrainProfileDataAsset.Get())
	{
		for (const FSRProfileNaturalStructureSpawnRule& Rule : TerrainProfileDataAsset->ProfileNaturalStructureSpawnRules)
		{
			GenerateProfileRule(Rule, BodyData.ProfileNaturalStructureSpawnRuleOverrides);
		}

		for (const FSRPlanetProfileBiomeEntry& BiomeEntry : TerrainProfileDataAsset->Biomes)
		{
			const USRPlanetBiomeDataAsset* BiomeDataAsset = BiomeEntry.BiomeDataAsset.Get();
			if (!IsValid(BiomeDataAsset))
			{
				continue;
			}

			for (const FSRProfileNaturalStructureSpawnRule& Rule : BiomeDataAsset->NaturalStructureSpawnRules)
			{
				GenerateBiomeRule(BiomeDataAsset->BiomeId, Rule, BiomeEntry.NaturalStructureSpawnRuleOverrides);
			}
		}
	}

	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.Total '%s' %.2f ms"), *GetNameSafe(Body), SRSolarElapsedMilliseconds(TotalStart)));
}

void ASRSolarSystemGenerator::DestroyRuntimeNaturalStructures()
{
	for (TObjectPtr<ASRCelestialBody>& PlanetBody : RuntimePlanetBodies)
	{
		if (!IsValid(PlanetBody))
		{
			continue;
		}

		ASRCelestialBody* PlanetBodyActor = PlanetBody.Get();
		USRPlanetSurfaceGrid* SurfaceGrid = PlanetBodyActor->GetSurfaceGrid();
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = PlanetBodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			StructureInstanceManager->ClearNaturalStructures(SurfaceGrid);
		}
	}

	if (UWorld* World = GetWorld())
	{
		for (TObjectPtr<AActor>& NaturalStructureActor : RuntimeNaturalStructureActors)
		{
			if (IsValid(NaturalStructureActor))
			{
				World->DestroyActor(NaturalStructureActor);
			}
		}
	}

	RuntimeNaturalStructureActors.Reset();
}

void ASRSolarSystemGenerator::DestroyTrackedActor(TObjectPtr<ASRCelestialBody>& ActorToDestroy)
{
	if (IsValid(ActorToDestroy) && GetWorld())
	{
		GetWorld()->DestroyActor(ActorToDestroy);
	}

	ActorToDestroy = nullptr;
}

void ASRSolarSystemGenerator::DestroyTrackedActors(TArray<TObjectPtr<ASRCelestialBody>>& ActorsToDestroy)
{
	for (TObjectPtr<ASRCelestialBody>& ActorToDestroy : ActorsToDestroy)
	{
		DestroyTrackedActor(ActorToDestroy);
	}

	ActorsToDestroy.Reset();
}
