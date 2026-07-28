#include "Simulation/SRSolarSystemGenerator.h"

#include "Celestial/SRPlanetDataAsset.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "UI/SRLoadingScreenWidget.h"

namespace
{
	void ResizeOrbitPeriodsToCount(TArray<float>& OrbitPeriods, int32 RequestedCount)
	{
		const int32 ResolvedCount = FMath::Max(0, RequestedCount);
		const int32 PreviousCount = OrbitPeriods.Num();
		OrbitPeriods.SetNum(ResolvedCount);

		for (int32 Index = PreviousCount; Index < ResolvedCount; ++Index)
		{
			OrbitPeriods[Index] = static_cast<float>(Index + 1);
		}

		for (float& OrbitPeriod : OrbitPeriods)
		{
			OrbitPeriod = FMath::Max(0.0f, OrbitPeriod);
		}
	}

	float ResolveIndexedOrbitPeriod(const TArray<float>& OrbitPeriods, int32 BodyIndex)
	{
		if (BodyIndex < 0)
		{
			return 1.0f;
		}

		return OrbitPeriods.IsValidIndex(BodyIndex)
			? FMath::Max(0.0f, OrbitPeriods[BodyIndex])
			: static_cast<float>(BodyIndex + 1);
	}
}

ASRSolarSystemGenerator::ASRSolarSystemGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GenerationSeed = 1000;
	bRandomizeGenerationSeedEachRun = false;
	MinimumUniquePlanetTypes = 4;
	RequiredSystemResourceRuleIds = {
		TEXT("ResourceV2.HeliosIron"),
		TEXT("ResourceV2.EchoQuartz"),
		TEXT("ResourceV2.VerdantSpore"),
		TEXT("ResourceV2.AuroraPlasma"),
		TEXT("ResourceV2.NullPearl"),
		TEXT("ResourceV2.CommonOre"),
		TEXT("ResourceV2.BiomassFeedstock"),
	};
	MinPlanet = 3;
	MaxPlanet = 7;
	MinMoon = 0;
	MaxMoon = 1;
	PlanetInitialOrbit = 30000.0f;
	PlanetOrbitIncrease = 20000.0f;
	MoonInitialOrbit = 6000.0f;
	MoonOrbitIncrease = 4000.0f;
	NormalizeOrbitPeriodSettings();
	bGenerateNaturalStructures = true;
	LoadingScreenWidgetClass = USRLoadingScreenWidget::StaticClass();
	LoadingScreenZOrder = 10000;
	bEnableMemoryDiagnostics = true;
	bParallelDynamicMeshPreparation = true;
	DynamicMeshPreparationMaxConcurrency = 4;
}

#if WITH_EDITOR
void ASRSolarSystemGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NormalizeOrbitPeriodSettings();
}
#endif

void ASRSolarSystemGenerator::PostLoad()
{
	Super::PostLoad();
	NormalizeOrbitPeriodSettings();
}

void ASRSolarSystemGenerator::BeginPlay()
{
	Super::BeginPlay();
	NormalizeOrbitPeriodSettings();
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

void ASRSolarSystemGenerator::NormalizeOrbitPeriodSettings()
{
	MinimumUniquePlanetTypes = FMath::Max(0, MinimumUniquePlanetTypes);
	TSet<FName> SeenRequiredResourceRuleIds;
	RequiredSystemResourceRuleIds.RemoveAll([&SeenRequiredResourceRuleIds](const FName RuleId)
	{
		if (RuleId.IsNone() || SeenRequiredResourceRuleIds.Contains(RuleId))
		{
			return true;
		}
		SeenRequiredResourceRuleIds.Add(RuleId);
		return false;
	});
	const int32 ResolvedMaxPlanetCount = FMath::Max(FMath::Max(0, MinPlanet), MaxPlanet);
	const int32 ResolvedMaxMoonCount = FMath::Max(FMath::Max(0, MinMoon), MaxMoon);
	ResizeOrbitPeriodsToCount(PlanetOrbitPeriods, ResolvedMaxPlanetCount);
	ResizeOrbitPeriodsToCount(MoonOrbitPeriods, ResolvedMaxMoonCount);
}

#if WITH_EDITOR
void ASRSolarSystemGenerator::ConfigurePlanetEnvironmentCatalogForEditor(
	const TArray<USRPlanetDataAsset*>& InPlanetDataAssets,
	int32 InMinimumUniquePlanetTypes,
	int32 InMinPlanet,
	int32 InMaxPlanet,
	int32 InMinMoon,
	int32 InMaxMoon,
	const TArray<FName>& InRequiredSystemResourceRuleIds)
{
	Modify();
	PlanetDataAssets.Reset(InPlanetDataAssets.Num());
	for (USRPlanetDataAsset* PlanetDataAsset : InPlanetDataAssets)
	{
		if (IsValid(PlanetDataAsset))
		{
			PlanetDataAssets.AddUnique(PlanetDataAsset);
		}
	}
	MinimumUniquePlanetTypes = FMath::Max(0, InMinimumUniquePlanetTypes);
	MinPlanet = FMath::Max(0, InMinPlanet);
	MaxPlanet = FMath::Max(MinPlanet, InMaxPlanet);
	MinMoon = FMath::Max(0, InMinMoon);
	MaxMoon = FMath::Max(MinMoon, InMaxMoon);
	RequiredSystemResourceRuleIds = InRequiredSystemResourceRuleIds;
	NormalizeOrbitPeriodSettings();
	MarkPackageDirty();
}
#endif

float ASRSolarSystemGenerator::ResolvePlanetOrbitPeriod(int32 PlanetIndex) const
{
	return ResolveIndexedOrbitPeriod(PlanetOrbitPeriods, PlanetIndex);
}

float ASRSolarSystemGenerator::ResolveMoonOrbitPeriod(int32 MoonIndex) const
{
	return ResolveIndexedOrbitPeriod(MoonOrbitPeriods, MoonIndex);
}
