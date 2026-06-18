#include "Simulation/SRSolarSystemGenerator.h"

#include "Simulation/SRSolarSystemGeneratorInternal.h"

#include "Async/ParallelFor.h"
#include "HAL/CriticalSection.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"
#include "Utility/SRTimingLog.h"

using namespace StarRoversSolarSystemGeneratorInternal;

namespace
{
	constexpr int32 StableDynamicMeshPreparationMaxConcurrency = 8;

	TAutoConsoleVariable<int32> CVarSRDynamicMeshPrepareDetailBodyLimit(
		TEXT("sr.DynamicMesh.PrepareDetailBodyLimit"),
		0,
		TEXT("Maximum number of generated body dynamic mesh timing detail blocks to log. Set 0 to log only the slowest body detail."));

	TAutoConsoleVariable<int32> CVarSRDynamicMeshParallelBodyPrepare(
		TEXT("sr.DynamicMesh.ParallelBodyPrepare"),
		-1,
		TEXT("Override generated celestial body dynamic mesh parallel preparation. -1 uses the SolarSystemGenerator setting, 0 disables, 1 enables."));

	TAutoConsoleVariable<int32> CVarSRDynamicMeshParallelBodyPrepareMaxConcurrency(
		TEXT("sr.DynamicMesh.ParallelBodyPrepareMaxConcurrency"),
		-1,
		TEXT("Override dynamic mesh preparation max concurrency. -1 uses the SolarSystemGenerator setting."));

	bool ResolveParallelDynamicMeshPreparationEnabled(bool bConfiguredEnabled)
	{
		const int32 OverrideValue = CVarSRDynamicMeshParallelBodyPrepare.GetValueOnGameThread();
		if (OverrideValue >= 0)
		{
			return OverrideValue != 0;
		}
		return bConfiguredEnabled;
	}

	int32 ResolveParallelDynamicMeshPreparationMaxConcurrency(int32 ConfiguredMaxConcurrency)
	{
		const int32 OverrideValue = CVarSRDynamicMeshParallelBodyPrepareMaxConcurrency.GetValueOnGameThread();
		const int32 RequestedMaxConcurrency = OverrideValue > 0 ? OverrideValue : ConfiguredMaxConcurrency;
		return FMath::Clamp(RequestedMaxConcurrency, 1, StableDynamicMeshPreparationMaxConcurrency);
	}

	void LogPreparedBodyTimingDetails(
		const TCHAR* Prefix,
		const TArray<FSRPreparedBodyTimingDetail>& BodyTimingDetails,
		const FString& SlowestBodyName,
		const TArray<FString>& SlowestBodyDetailLines)
	{
		const int32 DetailBodyLimit = FMath::Max(0, CVarSRDynamicMeshPrepareDetailBodyLimit.GetValueOnGameThread());
		if (DetailBodyLimit > 0 && !BodyTimingDetails.IsEmpty())
		{
			const int32 DetailBodyCount = FMath::Min(DetailBodyLimit, BodyTimingDetails.Num());
			FSRTimingLog::AddLine(FString::Printf(
				TEXT("%s.BodyDetails Bodies=%d Logged=%d Limit=%d"),
				Prefix,
				BodyTimingDetails.Num(),
				DetailBodyCount,
				DetailBodyLimit));
			for (int32 DetailBodyIndex = 0; DetailBodyIndex < DetailBodyCount; ++DetailBodyIndex)
			{
				const FSRPreparedBodyTimingDetail& BodyDetail = BodyTimingDetails[DetailBodyIndex];
				FSRTimingLog::AddLine(FString::Printf(
					TEXT("%s.BodyDetail Body=%s Ms=%.2f Lines=%d"),
					Prefix,
					*BodyDetail.BodyName,
					BodyDetail.Milliseconds,
					BodyDetail.DetailLines.Num()));
				for (const FString& DetailLine : BodyDetail.DetailLines)
				{
					FSRTimingLog::AddLine(FString::Printf(TEXT("%s.BodyDetail.%s.%s"), Prefix, *BodyDetail.BodyName, *DetailLine));
				}
			}
			return;
		}

		if (!SlowestBodyDetailLines.IsEmpty())
		{
			FSRTimingLog::AddLine(FString::Printf(
				TEXT("%s.SlowestDetail Body=%s Lines=%d"),
				Prefix,
				*SlowestBodyName,
				SlowestBodyDetailLines.Num()));
			for (const FString& DetailLine : SlowestBodyDetailLines)
			{
				FSRTimingLog::AddLine(FString::Printf(TEXT("%s.SlowestDetail.%s"), Prefix, *DetailLine));
			}
		}
	}
}
void ASRSolarSystemGenerator::ContinueRuntimeDynamicMeshPreparation()
{
	if (AsyncPrepareBodyIndex == 0 && ResolveParallelDynamicMeshPreparationEnabled(bParallelDynamicMeshPreparation))
	{
		UpdateLoadingProgress(0.20f, NSLOCTEXT("StarRoversLoadingScreen", "PreparingSurfacesParallel", "Preparing planet surfaces..."));
		PrepareRuntimeGeneratedDynamicMeshes();
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
		return;
	}

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
				AsyncPrepareSlowestBodyDetailLines = BodyDetailLines;
			}
			FSRPreparedBodyTimingDetail& BodyTimingDetail = AsyncPrepareBodyTimingDetails.AddDefaulted_GetRef();
			BodyTimingDetail.BodyName = GetNameSafe(Body);
			BodyTimingDetail.Milliseconds = BodyMs;
			BodyTimingDetail.DetailLines = MoveTemp(BodyDetailLines);
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
				AsyncPrepareSlowestBodyDetailLines = BodyDetailLines;
			}
			FSRPreparedBodyTimingDetail& BodyTimingDetail = AsyncPrepareBodyTimingDetails.AddDefaulted_GetRef();
			BodyTimingDetail.BodyName = GetNameSafe(Body);
			BodyTimingDetail.Milliseconds = BodyMs;
			BodyTimingDetail.DetailLines = MoveTemp(BodyDetailLines);
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
		TEXT("PrepareRuntimeGeneratedDynamicMeshes.Total %.2f ms Bodies=%d Planets=%d PlanetTotal=%.2f ms Moons=%d MoonTotal=%.2f ms Slowest=%s SlowestMs=%.2f Parallel=false Path=DeferredSequential"),
		SRSolarElapsedMilliseconds(AsyncDynamicMeshTotalStart),
		AsyncPreparePlanetCount + AsyncPrepareMoonCount,
		AsyncPreparePlanetCount,
		AsyncPreparePlanetTotalMs,
		AsyncPrepareMoonCount,
		AsyncPrepareMoonTotalMs,
		*AsyncPrepareSlowestBodyName,
		AsyncPrepareSlowestBodyMs));
	LogPreparedBodyTimingDetails(
		TEXT("PrepareRuntimeGeneratedDynamicMeshes"),
		AsyncPrepareBodyTimingDetails,
		AsyncPrepareSlowestBodyName,
		AsyncPrepareSlowestBodyDetailLines);
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
	TArray<FSRPreparedBodyTimingDetail> BodyTimingDetails;
	FCriticalSection TimingDetailsCriticalSection;

	auto RecordPreparedBody = [
		&SlowestBodyMs,
		&SlowestBodyName,
		&SlowestBodyDetailLines,
		&BodyTimingDetails,
		&TimingDetailsCriticalSection](
		ASRCelestialBody* Body,
		double BodyMs,
		TArray<FString>&& BodyDetailLines)
	{
		FScopeLock Lock(&TimingDetailsCriticalSection);
		if (BodyMs > SlowestBodyMs)
		{
			SlowestBodyMs = BodyMs;
			SlowestBodyName = GetNameSafe(Body);
			SlowestBodyDetailLines = BodyDetailLines;
		}
		FSRPreparedBodyTimingDetail& BodyTimingDetail = BodyTimingDetails.AddDefaulted_GetRef();
		BodyTimingDetail.BodyName = GetNameSafe(Body);
		BodyTimingDetail.Milliseconds = BodyMs;
		BodyTimingDetail.DetailLines = MoveTemp(BodyDetailLines);
	};

	auto PrepareBody = [&RecordPreparedBody](ASRCelestialBody* Body)
	{
		TArray<FString> BodyDetailLines;
		const double BodyStart = SRSolarNowSeconds();
		{
			FSRTimingLogScopedCapture CaptureBodyDetailLogs(BodyDetailLines);
			Body->PrepareCelestialBodyDynamicMesh();
		}
		const double BodyMs = SRSolarElapsedMilliseconds(BodyStart);
		RecordPreparedBody(Body, BodyMs, MoveTemp(BodyDetailLines));
		return BodyMs;
	};

	const bool bParallelBodyPrepare = ResolveParallelDynamicMeshPreparationEnabled(bParallelDynamicMeshPreparation);
	if (bParallelBodyPrepare)
	{
		const int32 MaxParallelBodyPrepare = ResolveParallelDynamicMeshPreparationMaxConcurrency(DynamicMeshPreparationMaxConcurrency);

		struct FSRBodyPrepareBatchItem
		{
			ASRCelestialBody* Body = nullptr;
			bool bIsMoon = false;
		};

		TArray<FSRBodyPrepareBatchItem> BodiesToPrepare;
		BodiesToPrepare.Reserve(RuntimePlanetBodies.Num() + RuntimeMoonBodies.Num());
		for (TObjectPtr<ASRCelestialBody>& PlanetBody : RuntimePlanetBodies)
		{
			if (IsValid(PlanetBody))
			{
				FSRBodyPrepareBatchItem& Item = BodiesToPrepare.AddDefaulted_GetRef();
				Item.Body = PlanetBody.Get();
				Item.bIsMoon = false;
				++PlanetCount;
			}
		}

		for (TObjectPtr<ASRCelestialBody>& MoonBody : RuntimeMoonBodies)
		{
			if (IsValid(MoonBody))
			{
				FSRBodyPrepareBatchItem& Item = BodiesToPrepare.AddDefaulted_GetRef();
				Item.Body = MoonBody.Get();
				Item.bIsMoon = true;
				++MoonCount;
			}
		}

		auto AddBodyTotalMs = [&PlanetTotalMs, &MoonTotalMs](const FSRBodyPrepareBatchItem& Item, double BodyMs)
		{
			if (Item.bIsMoon)
			{
				MoonTotalMs += BodyMs;
			}
			else
			{
				PlanetTotalMs += BodyMs;
			}
		};

		auto PrepareBodiesInBatches = [&RecordPreparedBody, &AddBodyTotalMs, MaxParallelBodyPrepare](const TArray<FSRBodyPrepareBatchItem>& BodiesToPrepare)
		{
			for (int32 BatchStart = 0; BatchStart < BodiesToPrepare.Num(); BatchStart += MaxParallelBodyPrepare)
			{
				const double BatchStartSeconds = SRSolarNowSeconds();
				const int32 BatchCount = FMath::Min(MaxParallelBodyPrepare, BodiesToPrepare.Num() - BatchStart);
				int32 BatchPlanetCount = 0;
				int32 BatchMoonCount = 0;
				for (int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
				{
					if (BodiesToPrepare[BatchStart + BatchIndex].bIsMoon)
					{
						++BatchMoonCount;
					}
					else
					{
						++BatchPlanetCount;
					}
				}
				TArray<FSRCelestialBodyPreparedDynamicMesh> PreparedMeshes;
				PreparedMeshes.SetNum(BatchCount);
				TArray<TArray<FString>> BodyDetailLines;
				BodyDetailLines.SetNum(BatchCount);
				TArray<double> BuildTimes;
				BuildTimes.SetNumZeroed(BatchCount);
				TArray<bool> bBuildSucceeded;
				bBuildSucceeded.Init(false, BatchCount);

				const double BuildPhaseStart = SRSolarNowSeconds();
				ParallelFor(
					BatchCount,
					[&BodiesToPrepare, &PreparedMeshes, &BodyDetailLines, &BuildTimes, &bBuildSucceeded, BatchStart](int32 BatchIndex)
					{
						ASRCelestialBody* Body = BodiesToPrepare[BatchStart + BatchIndex].Body;
						if (!IsValid(Body))
						{
							return;
						}

						const double BuildStart = SRSolarNowSeconds();
						{
							FSRTimingLogScopedCapture CaptureBodyDetailLogs(BodyDetailLines[BatchIndex]);
							bBuildSucceeded[BatchIndex] = Body->BuildPreparedCelestialBodyDynamicMesh(PreparedMeshes[BatchIndex]);
						}
						BuildTimes[BatchIndex] = SRSolarElapsedMilliseconds(BuildStart);
					});
				const double BuildWallMs = SRSolarElapsedMilliseconds(BuildPhaseStart);

				double BuildSumMs = 0.0;
				double BuildMaxMs = 0.0;
				for (double BuildMs : BuildTimes)
				{
					BuildSumMs += BuildMs;
					BuildMaxMs = FMath::Max(BuildMaxMs, BuildMs);
				}

				const double ApplyPhaseStart = SRSolarNowSeconds();
				double ApplySumMs = 0.0;
				double ApplyMaxMs = 0.0;
				for (int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
				{
					const FSRBodyPrepareBatchItem& Item = BodiesToPrepare[BatchStart + BatchIndex];
					ASRCelestialBody* Body = Item.Body;
					if (!IsValid(Body))
					{
						continue;
					}

					double ApplyMs = 0.0;
					if (bBuildSucceeded[BatchIndex])
					{
						TArray<FString> ApplyDetailLines;
						const double ApplyStart = SRSolarNowSeconds();
						{
							FSRTimingLogScopedCapture CaptureApplyDetailLogs(ApplyDetailLines);
							Body->ApplyPreparedCelestialBodyDynamicMesh(MoveTemp(PreparedMeshes[BatchIndex]), ApplyStart);
						}
						ApplyMs = SRSolarElapsedMilliseconds(ApplyStart);
						BodyDetailLines[BatchIndex].Append(MoveTemp(ApplyDetailLines));
					}
					else
					{
						const double FallbackStart = SRSolarNowSeconds();
						{
							FSRTimingLogScopedCapture CaptureFallbackDetailLogs(BodyDetailLines[BatchIndex]);
							Body->PrepareCelestialBodyDynamicMesh();
						}
						BuildTimes[BatchIndex] = SRSolarElapsedMilliseconds(FallbackStart);
						ApplyMs = 0.0;
					}
					ApplySumMs += ApplyMs;
					ApplyMaxMs = FMath::Max(ApplyMaxMs, ApplyMs);

					const double BodyMs = BuildTimes[BatchIndex] + ApplyMs;
					AddBodyTotalMs(Item, BodyMs);
					RecordPreparedBody(Body, BodyMs, MoveTemp(BodyDetailLines[BatchIndex]));
				}
				const double ApplyWallMs = SRSolarElapsedMilliseconds(ApplyPhaseStart);
				FSRTimingLog::AddLine(FString::Printf(
					TEXT("PrepareRuntimeGeneratedDynamicMeshes.Batch Start=%d Count=%d Planets=%d Moons=%d MaxConcurrency=%d BuildWall=%.2f ms BuildSum=%.2f ms BuildMax=%.2f ms ApplyWall=%.2f ms ApplySum=%.2f ms ApplyMax=%.2f ms TotalWall=%.2f ms"),
					BatchStart,
					BatchCount,
					BatchPlanetCount,
					BatchMoonCount,
					MaxParallelBodyPrepare,
					BuildWallMs,
					BuildSumMs,
					BuildMaxMs,
					ApplyWallMs,
					ApplySumMs,
					ApplyMaxMs,
					SRSolarElapsedMilliseconds(BatchStartSeconds)));
			}
		};

		PrepareBodiesInBatches(BodiesToPrepare);
	}
	else
	{
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
	}
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("PrepareRuntimeGeneratedDynamicMeshes.Total %.2f ms Bodies=%d Planets=%d PlanetTotal=%.2f ms Moons=%d MoonTotal=%.2f ms Slowest=%s SlowestMs=%.2f Parallel=%s MaxConcurrency=%d"),
		SRSolarElapsedMilliseconds(TotalStart),
		PlanetCount + MoonCount,
		PlanetCount,
		PlanetTotalMs,
		MoonCount,
		MoonTotalMs,
		*SlowestBodyName,
		SlowestBodyMs,
		bParallelBodyPrepare ? TEXT("true") : TEXT("false"),
		bParallelBodyPrepare ? ResolveParallelDynamicMeshPreparationMaxConcurrency(DynamicMeshPreparationMaxConcurrency) : 1));
	LogPreparedBodyTimingDetails(
		TEXT("PrepareRuntimeGeneratedDynamicMeshes"),
		BodyTimingDetails,
		SlowestBodyName,
		SlowestBodyDetailLines);
}
