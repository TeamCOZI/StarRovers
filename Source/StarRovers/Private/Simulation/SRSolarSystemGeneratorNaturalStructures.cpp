#include "Simulation/SRSolarSystemGenerator.h"

#include "Utility/SRLog.h"
#include "Simulation/SRSolarSystemGeneratorPipeline.h"

#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetBiomeDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"
#include "Utility/SRTimingLog.h"

using namespace StarRovers::Simulation::SolarSystemGeneration;
void ASRSolarSystemGenerator::ContinueRuntimeNaturalStructureGeneration()
{
	if (!bGenerateNaturalStructures)
	{
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeNaturalStructures.Total %.2f ms Disabled"), GetSolarSystemGenerationElapsedMilliseconds(AsyncNaturalStructuresTotalStart)));
		LogAsyncGenerationStageTiming(TEXT("GenerateRuntimeNaturalStructures"), GetSolarSystemGenerationElapsedMilliseconds(AsyncNaturalStructuresTotalStart));
		FinishRuntimeSystemGeneration();
		return;
	}

	const int32 TotalBodyCount = RuntimePlanetBodies.Num() + RuntimeMoonBodies.Num();
	if (AsyncNaturalBodyIndex < TotalBodyCount)
	{
		ASRCelestialBody* Body = AsyncNaturalBodyIndex < RuntimePlanetBodies.Num()
			? RuntimePlanetBodies[AsyncNaturalBodyIndex].Get()
			: RuntimeMoonBodies[AsyncNaturalBodyIndex - RuntimePlanetBodies.Num()].Get();
		if (IsValid(Body))
		{
			const double BodyStart = GetSolarSystemGenerationTimingSeconds();
			{
				FSRTimingLogScopedSuppress SuppressBodyDetailLogs;
				GenerateNaturalStructuresForBody(Body, AsyncNaturalStructureRandomStream);
			}
			const double BodyMs = GetSolarSystemGenerationElapsedMilliseconds(BodyStart);
			AsyncNaturalBodyTotalMs += BodyMs;
			++AsyncNaturalBodyCount;
			if (BodyMs > AsyncNaturalSlowestBodyMs)
			{
				AsyncNaturalSlowestBodyMs = BodyMs;
				AsyncNaturalSlowestBodyName = GetNameSafe(Body);
			}
		}

		++AsyncNaturalBodyIndex;
		const float NaturalProgress = TotalBodyCount <= 0
			? 1.0f
			: static_cast<float>(AsyncNaturalBodyIndex) / static_cast<float>(TotalBodyCount);
		UpdateLoadingProgress(
			FMath::Lerp(0.84f, 0.96f, NaturalProgress),
			FText::FromString(FString::Printf(TEXT("Placing natural structures... %d / %d"), FMath::Min(AsyncNaturalBodyIndex, TotalBodyCount), TotalBodyCount)));
		ScheduleLoadingGenerationStep(&ASRSolarSystemGenerator::ContinueRuntimeNaturalStructureGeneration);
		return;
	}

	FSRTimingLog::AddLine(FString::Printf(
		TEXT("GenerateRuntimeNaturalStructures.Total %.2f ms Bodies=%d BodyTotal=%.2f ms Slowest=%s SlowestMs=%.2f"),
		GetSolarSystemGenerationElapsedMilliseconds(AsyncNaturalStructuresTotalStart),
		AsyncNaturalBodyCount,
		AsyncNaturalBodyTotalMs,
		*AsyncNaturalSlowestBodyName,
		AsyncNaturalSlowestBodyMs));
	LogAsyncGenerationStageTiming(TEXT("GenerateRuntimeNaturalStructures"), GetSolarSystemGenerationElapsedMilliseconds(AsyncNaturalStructuresTotalStart));
	FinishRuntimeSystemGeneration();
}

void ASRSolarSystemGenerator::GenerateRuntimeNaturalStructures(int32 RuntimeGenerationSeed)
{
	const double TotalStart = GetSolarSystemGenerationTimingSeconds();
	double StageStart = GetSolarSystemGenerationTimingSeconds();
	DestroyRuntimeNaturalStructures();
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeNaturalStructures.DestroyExisting %.2f ms"), GetSolarSystemGenerationElapsedMilliseconds(StageStart)));
	if (!bGenerateNaturalStructures)
	{
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateRuntimeNaturalStructures.Total %.2f ms Disabled"), GetSolarSystemGenerationElapsedMilliseconds(TotalStart)));
		return;
	}

	FRandomStream NaturalStructureRandomStream(RuntimeGenerationSeed + 7919);
	int32 BodyCount = 0;
	double BodyTotalMs = 0.0;
	double SlowestBodyMs = 0.0;
	FString SlowestBodyName(TEXT("None"));
	auto GenerateForBodies = [this, &NaturalStructureRandomStream, &BodyCount, &BodyTotalMs, &SlowestBodyMs, &SlowestBodyName](
		TArray<TObjectPtr<ASRCelestialBody>>& Bodies)
	{
		for (TObjectPtr<ASRCelestialBody>& Body : Bodies)
		{
			if (IsValid(Body))
			{
				const double BodyStart = GetSolarSystemGenerationTimingSeconds();
				{
					FSRTimingLogScopedSuppress SuppressBodyDetailLogs;
					GenerateNaturalStructuresForBody(Body, NaturalStructureRandomStream);
				}
				const double BodyMs = GetSolarSystemGenerationElapsedMilliseconds(BodyStart);
				BodyTotalMs += BodyMs;
				++BodyCount;
				if (BodyMs > SlowestBodyMs)
				{
					SlowestBodyMs = BodyMs;
					SlowestBodyName = GetNameSafe(Body.Get());
				}
			}
		}
	};
	GenerateForBodies(RuntimePlanetBodies);
	GenerateForBodies(RuntimeMoonBodies);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("GenerateRuntimeNaturalStructures.Total %.2f ms Bodies=%d BodyTotal=%.2f ms Slowest=%s SlowestMs=%.2f"),
		GetSolarSystemGenerationElapsedMilliseconds(TotalStart),
		BodyCount,
		BodyTotalMs,
		*SlowestBodyName,
		SlowestBodyMs));
}

void ASRSolarSystemGenerator::GenerateNaturalStructuresForBody(ASRCelestialBody* Body, FRandomStream& RandomStream)
{
	const double TotalStart = GetSolarSystemGenerationTimingSeconds();
	if (!IsValid(Body)
		|| (Body->GetBodyCategory() != ESRCelestialBodyCategory::Planet
			&& Body->GetBodyCategory() != ESRCelestialBodyCategory::Moon))
	{
		return;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = Body->GetSurfaceGrid();
	if (!IsValid(SurfaceGrid) || SurfaceGrid->GetCellCount() <= 0)
	{
		return;
	}

	double StageStart = GetSolarSystemGenerationTimingSeconds();
	const TArray<FSRPlanetSurfaceGridCell>& Cells = SurfaceGrid->GetCellsRef();
	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.GetCellsRef '%s' %.2f ms Cells=%d"), *GetNameSafe(Body), GetSolarSystemGenerationElapsedMilliseconds(StageStart), Cells.Num()));

	TMap<FName, TArray<int32>> CandidateCellIndicesByBiomeId;
	TMap<FName, TArray<int32>> DryCandidateCellIndicesByBiomeId;
	auto GetBiomeCandidateCellIndices = [&Cells, &CandidateCellIndicesByBiomeId, &DryCandidateCellIndicesByBiomeId](
		FName BiomeId,
		bool bRequireDryLand) -> const TArray<int32>&
	{
		TMap<FName, TArray<int32>>& CandidateCache = bRequireDryLand
			? DryCandidateCellIndicesByBiomeId
			: CandidateCellIndicesByBiomeId;
		if (const TArray<int32>* ExistingCandidateCellIndices = CandidateCache.Find(BiomeId))
		{
			return *ExistingCandidateCellIndices;
		}

		const double BuildStart = GetSolarSystemGenerationTimingSeconds();
		TArray<int32>& CandidateCellIndices = CandidateCache.Add(BiomeId);
		CandidateCellIndices.Reserve(Cells.Num());
		for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
		{
			const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
			if (Cell.BiomeId == BiomeId
				&& !Cell.bOccupied
				&& (!bRequireDryLand || Cell.WaterRole == ESRBiomeWaterRole::None))
			{
				CandidateCellIndices.Add(CellIndex);
			}
		}
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("GenerateNaturalStructuresForBody.BuildBiomeIdCandidateIndices '%s' %.2f ms Candidates=%d DryOnly=%s"),
			*BiomeId.ToString(),
			GetSolarSystemGenerationElapsedMilliseconds(BuildStart),
			CandidateCellIndices.Num(),
			bRequireDryLand ? TEXT("true") : TEXT("false")));
		return CandidateCellIndices;
	};

	bool bLoggedMissingStructureDataAsset = false;
	USRStructureInstanceManagerComponent* StructureInstanceManager = Body->FindComponentByClass<USRStructureInstanceManagerComponent>();
	auto GenerateRuleForCandidateCells = [this, Body, SurfaceGrid, StructureInstanceManager, &Cells, &RandomStream, &bLoggedMissingStructureDataAsset](
		const TArray<int32>& CandidateCellIndices,
		USRStructureDataAsset* StructureDataAsset,
		float SpawnChancePerCell,
		int32 MaxCount,
		int32 MinCellSpacing)
	{
		const double RuleStart = GetSolarSystemGenerationTimingSeconds();
		const int32 InitialCandidateCount = CandidateCellIndices.Num();
		if (!IsValid(StructureDataAsset))
		{
			if (!bLoggedMissingStructureDataAsset)
			{
				bLoggedMissingStructureDataAsset = true;
				SR_LOG(SolarSystem, LogTemp, Error, TEXT("Natural structure generation for '%s' has one or more rules without StructureDataAsset."), *GetNameSafe(Body));
			}
			return;
		}

		if (CandidateCellIndices.IsEmpty())
		{
			FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.Rule '%s' %.2f ms Candidates=0 Placed=0"), *GetNameSafe(StructureDataAsset), GetSolarSystemGenerationElapsedMilliseconds(RuleStart)));
			return;
		}

		double StageStart = GetSolarSystemGenerationTimingSeconds();
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
		const double ShuffleMs = GetSolarSystemGenerationElapsedMilliseconds(StageStart);

		TArray<FSRPlanetSurfaceGridCellId> PlacedOriginCellIds;
		const int32 SafeMinCellSpacing = FMath::Max(0, MinCellSpacing);
		if (SafeMinCellSpacing > 0)
		{
			const int32 ExpectedPlacedCount = SafeMaxCount > 0 ? FMath::Min(SafeMaxCount, CandidateIterationIndices.Num()) : CandidateIterationIndices.Num();
			PlacedOriginCellIds.Reserve(ExpectedPlacedCount);
		}
		int32 PlacedCount = 0;
		auto TryPlaceCandidate = [this, SurfaceGrid, StructureInstanceManager, StructureDataAsset, &Cells, &PlacedOriginCellIds, SafeMinCellSpacing, &PlacedCount](
			int32 CandidateCellIndex)
		{
			if (!Cells.IsValidIndex(CandidateCellIndex))
			{
				return false;
			}
			const FSRPlanetSurfaceGridCell& CandidateCell = Cells[CandidateCellIndex];
			if (CandidateCell.bOccupied)
			{
				return false;
			}

			if (SafeMinCellSpacing > 0)
			{
				for (const FSRPlanetSurfaceGridCellId& PlacedOriginCellId : PlacedOriginCellIds)
				{
					if (PlacedOriginCellId.Face == CandidateCell.CellId.Face
						&& FMath::Abs(PlacedOriginCellId.CellX - CandidateCell.CellId.CellX) <= SafeMinCellSpacing
						&& FMath::Abs(PlacedOriginCellId.CellY - CandidateCell.CellId.CellY) <= SafeMinCellSpacing)
					{
						return false;
					}
				}
			}

			bool bPlacedStructure = false;
			if (StructureInstanceManager)
			{
				FName OccupantId = NAME_None;
				bPlacedStructure = StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(
					SurfaceGrid,
					CandidateCell.CellId,
					StructureDataAsset,
					OccupantId,
					true,
					true);
			}

			if (!bPlacedStructure)
			{
				AActor* PlacedStructureActor = nullptr;
				bPlacedStructure = USRStructurePlacementLibrary::TryPlaceStructureOnSurfaceGrid(
					SurfaceGrid,
					CandidateCell.CellId,
					StructureDataAsset,
					PlacedStructureActor,
					true);
				if (bPlacedStructure)
				{
					RuntimeNaturalStructureActors.Add(PlacedStructureActor);
				}
			}

			if (bPlacedStructure)
			{
				if (SafeMinCellSpacing > 0)
				{
					PlacedOriginCellIds.Add(CandidateCell.CellId);
				}
				++PlacedCount;
			}
			return bPlacedStructure;
		};

		for (const int32 CandidateCellIndex : CandidateIterationIndices)
		{
			if (SafeMaxCount > 0 && PlacedCount >= SafeMaxCount)
			{
				break;
			}

			if (SafeSpawnChancePerCell < 1.0f && RandomStream.FRand() > SafeSpawnChancePerCell)
			{
				continue;
			}
			TryPlaceCandidate(CandidateCellIndex);
		}

		bool bUsedMandatoryDepositFallback = false;
		if (PlacedCount == 0 && StructureDataAsset->bIsResourceDeposit)
		{
			// Resource availability is gameplay-critical. Random chance may vary the
			// count, but an enabled deposit rule must place at least one visible seam.
			for (const int32 CandidateCellIndex : CandidateCellIndices)
			{
				if (TryPlaceCandidate(CandidateCellIndex))
				{
					bUsedMandatoryDepositFallback = true;
					break;
				}
			}
		}
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("GenerateNaturalStructuresForBody.Rule '%s' %.2f ms Candidates=%d Iteration=%d Placed=%d Shuffle=%.2f ms MandatoryFallback=%s"),
			*GetNameSafe(StructureDataAsset),
			GetSolarSystemGenerationElapsedMilliseconds(RuleStart),
			InitialCandidateCount,
			CandidateIterationIndices.Num(),
			PlacedCount,
			ShuffleMs,
			bUsedMandatoryDepositFallback ? TEXT("true") : TEXT("false")));
	};

	auto BuildProfileCandidateCellIndices = [&Cells](bool bRequireDryLand)
	{
		const double BuildStart = GetSolarSystemGenerationTimingSeconds();
		TArray<int32> CandidateCellIndices;
		CandidateCellIndices.Reserve(Cells.Num());
		for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
		{
			const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
			if (!Cell.bOccupied
				&& (!bRequireDryLand || Cell.WaterRole == ESRBiomeWaterRole::None))
			{
				CandidateCellIndices.Add(CellIndex);
			}
		}
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("GenerateNaturalStructuresForBody.BuildProfileCandidateIndices %.2f ms Candidates=%d DryOnly=%s"),
			GetSolarSystemGenerationElapsedMilliseconds(BuildStart),
			CandidateCellIndices.Num(),
			bRequireDryLand ? TEXT("true") : TEXT("false")));
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
			BuildProfileCandidateCellIndices(
				IsValid(Rule.StructureDataAsset.Get()) && Rule.StructureDataAsset->bIsResourceDeposit),
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
			GetBiomeCandidateCellIndices(
				BiomeId,
				IsValid(Rule.StructureDataAsset.Get()) && Rule.StructureDataAsset->bIsResourceDeposit),
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

	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.Total '%s' %.2f ms"), *GetNameSafe(Body), GetSolarSystemGenerationElapsedMilliseconds(TotalStart)));
}

void ASRSolarSystemGenerator::DestroyRuntimeNaturalStructures()
{
	auto ClearForBodies = [](TArray<TObjectPtr<ASRCelestialBody>>& Bodies)
	{
		for (TObjectPtr<ASRCelestialBody>& Body : Bodies)
		{
			if (!IsValid(Body))
			{
				continue;
			}

			ASRCelestialBody* BodyActor = Body.Get();
			USRPlanetSurfaceGrid* SurfaceGrid = BodyActor->GetSurfaceGrid();
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearNaturalStructures(SurfaceGrid);
			}
		}
	};
	ClearForBodies(RuntimePlanetBodies);
	ClearForBodies(RuntimeMoonBodies);

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
