#include "Simulation/SRSolarSystemGenerator.h"

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
	const int32 ResolvedMaxPlanetCount = FMath::Max(FMath::Max(0, MinPlanet), MaxPlanet);
	const int32 ResolvedMaxMoonCount = FMath::Max(FMath::Max(0, MinMoon), MaxMoon);
	ResizeOrbitPeriodsToCount(PlanetOrbitPeriods, ResolvedMaxPlanetCount);
	ResizeOrbitPeriodsToCount(MoonOrbitPeriods, ResolvedMaxMoonCount);
}

float ASRSolarSystemGenerator::ResolvePlanetOrbitPeriod(int32 PlanetIndex) const
{
	return ResolveIndexedOrbitPeriod(PlanetOrbitPeriods, PlanetIndex);
}

float ASRSolarSystemGenerator::ResolveMoonOrbitPeriod(int32 MoonIndex) const
{
	return ResolveIndexedOrbitPeriod(MoonOrbitPeriods, MoonIndex);
}
