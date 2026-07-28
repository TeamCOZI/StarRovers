#include "Simulation/SRSolarSystemGenerator.h"

#include "Simulation/SRSolarSystemGeneratorPipeline.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "TimerManager.h"
#include "UI/SRLoadingScreenWidget.h"
#include "Utility/SRTimingLog.h"

using namespace StarRovers::Simulation::SolarSystemGeneration;
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
	NormalizeOrbitPeriodSettings();
	if (!GetWorld())
	{
		HideLoadingScreen();
		return;
	}

	bRuntimeGenerationInProgress = true;
	AsyncGenerationStageTimings.Reset();
	AsyncGenerationTotalStart = GetSolarSystemGenerationTimingSeconds();
	FSRTimingLog::BeginSession(TEXT("GenerateRuntimeSystem"));

	UpdateLoadingProgress(0.02f, NSLOCTEXT("StarRoversLoadingScreen", "Clearing", "Clearing previous system..."));
	LogMemoryDiagnosticsSnapshot(TEXT("GenerateRuntimeSystem.BeforeClear"));
	AsyncCurrentStageStart = GetSolarSystemGenerationTimingSeconds();
	ClearRuntimeGeneratedBodies();
	LogAsyncGenerationStageTiming(TEXT("ClearRuntimeGeneratedBodies"), GetSolarSystemGenerationElapsedMilliseconds(AsyncCurrentStageStart));

	UpdateLoadingProgress(0.05f, NSLOCTEXT("StarRoversLoadingScreen", "WaitingForCleanup", "Finalizing cleanup..."));
	AsyncCurrentStageStart = GetSolarSystemGenerationTimingSeconds();
	ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::ContinueRuntimeSystemGenerationAfterClear);
}

void ASRSolarSystemGenerator::ContinueRuntimeSystemGenerationAfterClear()
{
	if (!GetWorld())
	{
		FinishRuntimeSystemGeneration();
		return;
	}

	LogAsyncGenerationStageTiming(TEXT("WaitAfterClearForGC"), GetSolarSystemGenerationElapsedMilliseconds(AsyncCurrentStageStart));

	AsyncRuntimeGenerationSeed = bRandomizeGenerationSeedEachRun
		? CreateRuntimeRandomGenerationSeed()
		: GenerationSeed;
	LastRuntimeGenerationSeed = AsyncRuntimeGenerationSeed;
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("GenerateRuntimeSystem.Seed Configured=%d Runtime=%d Randomized=%s"),
		GenerationSeed,
		AsyncRuntimeGenerationSeed,
		bRandomizeGenerationSeedEachRun ? TEXT("true") : TEXT("false")));

	AsyncGenerationRandomStream = FRandomStream(AsyncRuntimeGenerationSeed);
	AsyncSelectedStarDataAsset = nullptr;

	UpdateLoadingProgress(0.08f, NSLOCTEXT("StarRoversLoadingScreen", "SpawningStar", "Creating primary star..."));
	AsyncCurrentStageStart = GetSolarSystemGenerationTimingSeconds();
	RuntimeStarBody = SpawnPrimaryStar(AsyncGenerationRandomStream, AsyncSelectedStarDataAsset);
	LogAsyncGenerationStageTiming(TEXT("SpawnPrimaryStar"), GetSolarSystemGenerationElapsedMilliseconds(AsyncCurrentStageStart));
	if (!IsValid(RuntimeStarBody))
	{
		FinishRuntimeSystemGeneration();
		return;
	}

	UpdateLoadingProgress(0.14f, NSLOCTEXT("StarRoversLoadingScreen", "SpawningPlanets", "Creating planets..."));
	AsyncCurrentStageStart = GetSolarSystemGenerationTimingSeconds();
	SpawnPlanets(RuntimeStarBody, AsyncSelectedStarDataAsset, AsyncGenerationRandomStream, RuntimePlanetBodies);
	LogAsyncGenerationStageTiming(
		TEXT("SpawnPlanets"),
		GetSolarSystemGenerationElapsedMilliseconds(AsyncCurrentStageStart),
		FString::Printf(TEXT(" Planets=%d Moons=%d"), RuntimePlanetBodies.Num(), RuntimeMoonBodies.Num()));

	AsyncPrepareBodyIndex = 0;
	AsyncPreparePlanetCount = 0;
	AsyncPrepareMoonCount = 0;
	AsyncPreparePlanetTotalMs = 0.0;
	AsyncPrepareMoonTotalMs = 0.0;
	AsyncPrepareSlowestBodyMs = 0.0;
	AsyncPrepareSlowestBodyName = TEXT("None");
	AsyncPrepareSlowestBodyDetailLines.Reset();
	AsyncPrepareBodyTimingDetails.Reset();
	AsyncDynamicMeshTotalStart = GetSolarSystemGenerationTimingSeconds();
	UpdateLoadingProgress(0.20f, NSLOCTEXT("StarRoversLoadingScreen", "PreparingSurfaces", "Preparing planet surfaces..."));
	ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::ContinueRuntimeDynamicMeshPreparation);
}

void ASRSolarSystemGenerator::FinishRuntimeSystemGeneration()
{
	UpdateLoadingProgress(0.98f, NSLOCTEXT("StarRoversLoadingScreen", "Finalizing", "Finalizing star system..."));
	AsyncCurrentStageStart = GetSolarSystemGenerationTimingSeconds();
	if (UWorld* World = GetWorld())
	{
		if (USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
		{
			CelestialBodyRegistry->SetPrimaryStarActor(RuntimeStarBody);
		}
	}
	LogAsyncGenerationStageTiming(TEXT("Registry"), GetSolarSystemGenerationElapsedMilliseconds(AsyncCurrentStageStart));

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
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.Total %.2f ms"), GetSolarSystemGenerationElapsedMilliseconds(AsyncGenerationTotalStart)));
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
