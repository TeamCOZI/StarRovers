#include "Simulation/SRSolarSystemGenerator.h"

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

	if (RuntimePlanetBodies.IsValidIndex(AsyncNaturalPlanetIndex))
	{
		ASRCelestialBody* PlanetBody = RuntimePlanetBodies[AsyncNaturalPlanetIndex].Get();
		if (IsValid(PlanetBody))
		{
			const double BodyStart = GetSolarSystemGenerationTimingSeconds();
			{
				FSRTimingLogScopedSuppress SuppressBodyDetailLogs;
				GenerateNaturalStructuresForBody(PlanetBody, AsyncNaturalStructureRandomStream);
			}
			const double BodyMs = GetSolarSystemGenerationElapsedMilliseconds(BodyStart);
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
		GetSolarSystemGenerationElapsedMilliseconds(AsyncNaturalStructuresTotalStart),
		AsyncNaturalPlanetCount,
		AsyncNaturalPlanetTotalMs,
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
	int32 PlanetCount = 0;
	double PlanetTotalMs = 0.0;
	double SlowestBodyMs = 0.0;
	FString SlowestBodyName(TEXT("None"));
	for (TObjectPtr<ASRCelestialBody>& PlanetBody : RuntimePlanetBodies)
	{
		if (IsValid(PlanetBody))
		{
			const double BodyStart = GetSolarSystemGenerationTimingSeconds();
			{
				FSRTimingLogScopedSuppress SuppressBodyDetailLogs;
				GenerateNaturalStructuresForBody(PlanetBody, NaturalStructureRandomStream);
			}
			const double BodyMs = GetSolarSystemGenerationElapsedMilliseconds(BodyStart);
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
		GetSolarSystemGenerationElapsedMilliseconds(TotalStart),
		PlanetCount,
		PlanetTotalMs,
		*SlowestBodyName,
		SlowestBodyMs));
}

void ASRSolarSystemGenerator::GenerateNaturalStructuresForBody(ASRCelestialBody* Body, FRandomStream& RandomStream)
{
	const double TotalStart = GetSolarSystemGenerationTimingSeconds();
	if (!IsValid(Body) || Body->GetBodyCategory() != ESRCelestialBodyCategory::Planet)
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
	auto GetBiomeCandidateCellIndices = [&Cells, &CandidateCellIndicesByBiomeId](FName BiomeId) -> const TArray<int32>&
	{
		if (const TArray<int32>* ExistingCandidateCellIndices = CandidateCellIndicesByBiomeId.Find(BiomeId))
		{
			return *ExistingCandidateCellIndices;
		}

		const double BuildStart = GetSolarSystemGenerationTimingSeconds();
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
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.BuildBiomeIdCandidateIndices '%s' %.2f ms Candidates=%d"), *BiomeId.ToString(), GetSolarSystemGenerationElapsedMilliseconds(BuildStart), CandidateCellIndices.Num()));
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
		const double RuleStart = GetSolarSystemGenerationTimingSeconds();
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
				bPlacedStructure = StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(SurfaceGrid, CandidateCell.CellId, StructureDataAsset, OccupantId, true, true);
			}

			if (!bPlacedStructure)
			{
				AActor* PlacedStructureActor = nullptr;
				bPlacedStructure = USRStructurePlacementLibrary::TryPlaceStructureOnSurfaceGrid(SurfaceGrid, CandidateCell.CellId, StructureDataAsset, PlacedStructureActor, true);
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
			GetSolarSystemGenerationElapsedMilliseconds(RuleStart),
			InitialCandidateCount,
			CandidateIterationIndices.Num(),
			PlacedCount,
			ShuffleMs));
	};

	auto BuildProfileCandidateCellIndices = [&Cells]()
	{
		const double BuildStart = GetSolarSystemGenerationTimingSeconds();
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
		FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.BuildProfileCandidateIndices %.2f ms Candidates=%d"), GetSolarSystemGenerationElapsedMilliseconds(BuildStart), CandidateCellIndices.Num()));
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

	FSRTimingLog::AddLine(FString::Printf(TEXT("GenerateNaturalStructuresForBody.Total '%s' %.2f ms"), *GetNameSafe(Body), GetSolarSystemGenerationElapsedMilliseconds(TotalStart)));
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
