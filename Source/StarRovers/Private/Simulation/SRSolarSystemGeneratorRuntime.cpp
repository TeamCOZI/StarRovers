#include "Simulation/SRSolarSystemGenerator.h"

#include "Simulation/SRSolarSystemGeneratorInternal.h"

#include "Engine/Engine.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "TimerManager.h"
#include "Utility/SRMemoryDiagnostics.h"
#include "Utility/SRTimingLog.h"

using namespace StarRoversSolarSystemGeneratorInternal;
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
