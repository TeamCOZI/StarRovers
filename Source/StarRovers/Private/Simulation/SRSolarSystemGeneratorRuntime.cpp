#include "Simulation/SRSolarSystemGenerator.h"

#include "Simulation/SRSolarSystemGeneratorPipeline.h"

#include "Engine/Engine.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "TimerManager.h"
#include "Utility/SRMemoryDiagnostics.h"
#include "Utility/SRLog.h"
#include "Utility/SRTimingLog.h"

using namespace StarRovers::Simulation::SolarSystemGeneration;
ASRCelestialBody* ASRSolarSystemGenerator::GenerateRuntimeSystem()
{
	if (bRuntimeGenerationInProgress)
	{
		return nullptr;
	}
	bRuntimeGenerationInProgress = true;
	ASRCelestialBody* Result = GenerateRuntimeSystemInternal(ResolveRuntimeGenerationSeed());
	bRuntimeGenerationInProgress = false;
	return Result;
}

ASRCelestialBody* ASRSolarSystemGenerator::GenerateRuntimeSystemWithSeed(int32 RuntimeGenerationSeed)
{
	if (RuntimeGenerationSeed < 0 || bRuntimeGenerationInProgress)
	{
		return nullptr;
	}
	bRuntimeGenerationInProgress = true;
	ASRCelestialBody* Result = GenerateRuntimeSystemInternal(RuntimeGenerationSeed);
	bRuntimeGenerationInProgress = false;
	return Result;
}

ASRCelestialBody* ASRSolarSystemGenerator::GenerateRuntimeSystemInternal(int32 RuntimeGenerationSeed)
{
	FSRTimingLogSession TimingLogSession(TEXT("GenerateRuntimeSystem"));
	const double TotalStart = GetSolarSystemGenerationTimingSeconds();
	NormalizeOrbitPeriodSettings();
	if (!GetWorld())
	{
		return nullptr;
	}
	if (!EnsurePatternContentLoadedFromClassDefaults())
	{
		return nullptr;
	}

	TArray<FSRSolarSystemGenerationStageTiming> StageTimings;
	auto LogStageTiming = [&StageTimings](const TCHAR* StageName, double Milliseconds, const FString& Suffix = FString())
	{
		StageTimings.Add({ FString(StageName), Milliseconds });
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.%s %.2f ms%s"), StageName, Milliseconds, *Suffix));
	};

	LogMemoryDiagnosticsSnapshot(TEXT("GenerateRuntimeSystem.BeforeClear"));
	double StageStart = GetSolarSystemGenerationTimingSeconds();
	ClearRuntimeGeneratedBodies();
	LogStageTiming(TEXT("ClearRuntimeGeneratedBodies"), GetSolarSystemGenerationElapsedMilliseconds(StageStart));

	FSRTimingLog::AddLine(FString::Printf(
		TEXT("GenerateRuntimeSystem.Seed Configured=%d Runtime=%d Randomized=%s"),
		GenerationSeed,
		RuntimeGenerationSeed,
		bRandomizeGenerationSeedEachRun ? TEXT("true") : TEXT("false")));

	FRandomStream RandomStream(RuntimeGenerationSeed);
	const USRStarDataAsset* SelectedStarDataAsset = nullptr;
	StageStart = GetSolarSystemGenerationTimingSeconds();
	RuntimeStarBody = SpawnPrimaryStar(RandomStream, SelectedStarDataAsset);
	LogStageTiming(TEXT("SpawnPrimaryStar"), GetSolarSystemGenerationElapsedMilliseconds(StageStart));
	if (!IsValid(RuntimeStarBody))
	{
		return nullptr;
	}

	StageStart = GetSolarSystemGenerationTimingSeconds();
	SpawnPlanets(RuntimeStarBody, SelectedStarDataAsset, RandomStream, RuntimePlanetBodies);
	LogStageTiming(TEXT("SpawnPlanets"), GetSolarSystemGenerationElapsedMilliseconds(StageStart), FString::Printf(TEXT(" Planets=%d Moons=%d"), RuntimePlanetBodies.Num(), RuntimeMoonBodies.Num()));
	StageStart = GetSolarSystemGenerationTimingSeconds();
	PrepareRuntimeGeneratedDynamicMeshes();
	LogStageTiming(TEXT("PrepareRuntimeGeneratedDynamicMeshes"), GetSolarSystemGenerationElapsedMilliseconds(StageStart));
	StageStart = GetSolarSystemGenerationTimingSeconds();
	GenerateRuntimeNaturalStructures(RuntimeGenerationSeed);
	LogStageTiming(TEXT("GenerateRuntimeNaturalStructures"), GetSolarSystemGenerationElapsedMilliseconds(StageStart));
	StageStart = GetSolarSystemGenerationTimingSeconds();
	ValidateRuntimePatternGeneration(RuntimeGenerationSeed);
	LogStageTiming(TEXT("ValidateRuntimePatternGeneration"), GetSolarSystemGenerationElapsedMilliseconds(StageStart));
	StageStart = GetSolarSystemGenerationTimingSeconds();
	if (USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = GetWorld()->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
	{
		CelestialBodyRegistry->SetPrimaryStarActor(RuntimeStarBody);
	}
	LogStageTiming(TEXT("Registry"), GetSolarSystemGenerationElapsedMilliseconds(StageStart));
	const FSRSolarSystemGenerationStageTiming* SlowestStageTiming = nullptr;
	for (const FSRSolarSystemGenerationStageTiming& StageTiming : StageTimings)
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
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeSystem.Total %.2f ms"), GetSolarSystemGenerationElapsedMilliseconds(TotalStart)));
	LogMemoryDiagnosticsSnapshot(TEXT("GenerateRuntimeSystem.AfterComplete"));
	MarkRuntimeGenerationCompleted(RuntimeGenerationSeed);

	return RuntimeStarBody;
}

int32 ASRSolarSystemGenerator::ResolveRuntimeGenerationSeed() const
{
	return bRandomizeGenerationSeedEachRun
		? CreateRuntimeRandomGenerationSeed()
		: FMath::Max(0, GenerationSeed);
}

void ASRSolarSystemGenerator::MarkRuntimeGenerationCompleted(int32 RuntimeGenerationSeed)
{
	LastCompletedGenerationSeed = FMath::Max(0, RuntimeGenerationSeed);
	bHasCompletedGenerationSeed = IsValid(RuntimeStarBody);
}

bool ASRSolarSystemGenerator::GetLastCompletedGenerationSeed(int32& OutRuntimeGenerationSeed) const
{
	OutRuntimeGenerationSeed = bHasCompletedGenerationSeed ? LastCompletedGenerationSeed : 0;
	return bHasCompletedGenerationSeed && IsValid(RuntimeStarBody);
}

bool ASRSolarSystemGenerator::IsRuntimeGenerationInProgress() const
{
	return bRuntimeGenerationInProgress;
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
	bHasCompletedGenerationSeed = false;
	LastCompletedGenerationSeed = 0;
	bHasPatternGenerationValidationResult = false;
	LastPatternGenerationValidationResult = FSRPatternGenerationValidationResult();
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
				SR_LOG(SolarSystem, LogTemp, Display, TEXT("Requested garbage collection after clearing runtime generated celestial bodies."));
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
