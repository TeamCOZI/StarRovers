#include "Simulation/SRSolarSystemGenerator.h"

#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "UI/SRLoadingScreenWidget.h"
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
	bParallelDynamicMeshPreparation = true;
	DynamicMeshPreparationMaxConcurrency = 4;
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
