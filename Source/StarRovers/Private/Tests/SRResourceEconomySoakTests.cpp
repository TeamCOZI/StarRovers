#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceSystemContent.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRPlanetEnvironmentSelection.h"
#include "Simulation/SRResourceEconomySoak.h"
#include "Simulation/SRSolarSystemGenerator.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

#if WITH_EDITOR
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#endif

namespace StarRovers::ResourceEconomySoakTests
{
	constexpr TCHAR GeneratorBlueprintPath[] =
		TEXT("/Game/StarRovers/Generation/Blueprints/BP_SolarSystemGenerator.BP_SolarSystemGenerator");

	ASRSolarSystemGenerator* LoadGeneratorCDO()
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, GeneratorBlueprintPath);
		return IsValid(Blueprint) && IsValid(Blueprint->GeneratedClass)
			? Cast<ASRSolarSystemGenerator>(Blueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	}

	TArray<const USRPlanetDataAsset*> BuildPlanetCandidates(
		const ASRSolarSystemGenerator& Generator)
	{
		TArray<const USRPlanetDataAsset*> Result;
		Result.Reserve(Generator.GetPlanetEnvironmentCatalog().Num());
		for (const TObjectPtr<USRPlanetDataAsset>& Planet : Generator.GetPlanetEnvironmentCatalog())
		{
			Result.Add(Planet.Get());
		}
		return Result;
	}

#if WITH_EDITOR
	UWorld* FindPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	TMap<FName, int32> BuildFiniteDepositAmounts()
	{
		TMap<FName, int32> Result;
		TArray<FSRReferenceResourceDefinitionV2> Cards;
		FSRResourceSystemContent::GetAllReferenceResourceDefinitions(Cards);
		for (const FSRReferenceResourceDefinitionV2& Card : Cards)
		{
			Result.Add(Card.ResourceId, Card.DepositTotalAmount);
		}
		TArray<FSRUtilityResourceDefinitionV2> Utilities;
		FSRResourceSystemContent::GetAllUtilityResourceDefinitions(Utilities);
		for (const FSRUtilityResourceDefinitionV2& Utility : Utilities)
		{
			if (Utility.DepositTotalAmount > 0)
			{
				Result.Add(Utility.ResourceId, Utility.DepositTotalAmount);
			}
		}
		return Result;
	}

	bool HasMinerApproachCell(
		const USRPlanetSurfaceGrid& SurfaceGrid,
		const USRStructureInstanceManagerComponent& StructureManager,
		const FSRPlanetSurfaceGridCellId& OriginCellId)
	{
		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid.GetCellNeighbors(OriginCellId, Neighbors))
		{
			return false;
		}
		const FSRPlanetSurfaceGridCellId NeighborIds[] = {
			Neighbors.NegativeU,
			Neighbors.PositiveU,
			Neighbors.NegativeV,
			Neighbors.PositiveV,
		};
		for (const FSRPlanetSurfaceGridCellId& NeighborId : NeighborIds)
		{
			FSRPlanetSurfaceGridCellInfo NeighborInfo;
			if (!SurfaceGrid.GetCellInfoById(NeighborId, NeighborInfo))
			{
				continue;
			}
			if ((!NeighborInfo.bOccupied && NeighborInfo.bCanConstruct)
				|| (!NeighborInfo.OccupantId.IsNone()
					&& StructureManager.CanDestroyNaturalStructureForConstruction(
						NeighborInfo.OccupantId)))
			{
				return true;
			}
		}
		return false;
	}

	FString BuildAndValidateRuntimeSignature(
		FAutomationTestBase& Test,
		UWorld& World,
		const ASRSolarSystemGenerator& Generator,
		int32 ExpectedRootSeed)
	{
		const TMap<FName, int32> ExpectedDepositAmounts = BuildFiniteDepositAmounts();
		TMap<FString, const USRPlanetDataAsset*> PlanetAssetByEnvironmentName;
		for (const TObjectPtr<USRPlanetDataAsset>& Planet : Generator.GetPlanetEnvironmentCatalog())
		{
			if (IsValid(Planet))
			{
				PlanetAssetByEnvironmentName.Add(
					Planet->VariableName.IsEmpty() ? Planet->GetName() : Planet->VariableName.ToString(),
					Planet.Get());
			}
		}

		int32 PlanetCount = 0;
		int32 DepositCount = 0;
		int32 InaccessibleDepositCount = 0;
		int32 InvalidAmountCount = 0;
		int32 SpawnEnvelopeFailureCount = 0;
		TSet<FString> UniqueEnvironments;
		TSet<FName> SystemResourceIds;
		TArray<FString> BodySignatures;
		for (TActorIterator<ASRCelestialBody> BodyIt(&World); BodyIt; ++BodyIt)
		{
			ASRCelestialBody* Body = *BodyIt;
			if (!IsValid(Body)
				|| Body->GetBodyCategory() != ESRCelestialBodyCategory::Planet)
			{
				continue;
			}
			++PlanetCount;
			const FSRCelestialBodyData& BodyData = Body->GetData();
			const FString EnvironmentName = BodyData.VariableName.IsEmpty()
				? Body->GetName()
				: BodyData.VariableName.ToString();
			UniqueEnvironments.Add(EnvironmentName);

			USRStructureInstanceManagerComponent* StructureManager =
				Body->FindComponentByClass<USRStructureInstanceManagerComponent>();
			USRPlanetSurfaceGrid* SurfaceGrid = Body->GetSurfaceGrid();
			if (!IsValid(StructureManager) || !IsValid(SurfaceGrid))
			{
				++SpawnEnvelopeFailureCount;
				continue;
			}

			TArray<FSRPlacedStructureInstance> PlacedStructures;
			StructureManager->GetPlacedStructures(PlacedStructures);
			TMap<FName, FSRPlacedStructureInstance> PlacedByOccupantId;
			for (const FSRPlacedStructureInstance& Placed : PlacedStructures)
			{
				PlacedByOccupantId.Add(Placed.OccupantId, Placed);
			}
			TArray<FSRResourceDepositInstance> Deposits;
			StructureManager->GetResourceDepositInstances(Deposits);
			TMap<FName, int32> DepositCountByResourceId;
			TArray<FString> DepositSignatures;
			for (const FSRResourceDepositInstance& Deposit : Deposits)
			{
				++DepositCount;
				++DepositCountByResourceId.FindOrAdd(Deposit.ResourceId);
				SystemResourceIds.Add(Deposit.ResourceId);
				const int32* ExpectedAmount = ExpectedDepositAmounts.Find(Deposit.ResourceId);
				if (!ExpectedAmount
					|| Deposit.TotalAmount != *ExpectedAmount
					|| Deposit.RemainingAmount != *ExpectedAmount)
				{
					++InvalidAmountCount;
				}
				const FSRPlacedStructureInstance* Placed =
					PlacedByOccupantId.Find(Deposit.OccupantId);
				if (!Placed
					|| !HasMinerApproachCell(*SurfaceGrid, *StructureManager, Placed->OriginCellId))
				{
					++InaccessibleDepositCount;
				}
				if (Placed)
				{
					DepositSignatures.Add(FString::Printf(
						TEXT("%s:%d:%d:%d:%d"),
						*Deposit.ResourceId.ToString(),
						static_cast<int32>(Placed->OriginCellId.Face),
						Placed->OriginCellId.CellX,
						Placed->OriginCellId.CellY,
						Deposit.TotalAmount));
				}
			}
			DepositSignatures.Sort();

			const USRPlanetDataAsset* const* PlanetAsset =
				PlanetAssetByEnvironmentName.Find(EnvironmentName);
			TArray<FSRPlanetResourceRuleAvailability> Availability;
			FSRPlanetEnvironmentSelector::GetEnabledResourceRuleAvailability(
				PlanetAsset ? *PlanetAsset : nullptr,
				Availability);
			if (Availability.Num() != 3 || DepositCountByResourceId.Num() != 3)
			{
				++SpawnEnvelopeFailureCount;
			}
			for (const FSRPlanetResourceRuleAvailability& Rule : Availability)
			{
				FString ResourceIdString = Rule.RuleId.ToString();
				ResourceIdString.RemoveFromStart(TEXT("ResourceV2."));
				const int32 Count = DepositCountByResourceId.FindRef(FName(*ResourceIdString));
				if (Count < Rule.MinimumGuaranteedCount || Count > Rule.MaximumCount)
				{
					++SpawnEnvelopeFailureCount;
				}
			}
			BodySignatures.Add(FString::Printf(
				TEXT("%s#%d[%s]"),
				*EnvironmentName,
				BodyData.GenerationSeed,
				*FString::Join(DepositSignatures, TEXT(";"))));
		}
		BodySignatures.Sort();

		Test.TestEqual(TEXT("Forced generation records the requested root seed"),
			Generator.GetLastRuntimeGenerationSeed(), ExpectedRootSeed);
		Test.TestTrue(TEXT("Every forced seed creates five to seven planets"),
			PlanetCount >= Generator.GetMinimumPlanetCount()
				&& PlanetCount <= Generator.GetMaximumPlanetCount());
		Test.TestTrue(TEXT("Every forced seed contains at least four environments"),
			UniqueEnvironments.Num() >= Generator.GetMinimumUniquePlanetTypes());
		Test.TestEqual(TEXT("Every runtime deposit uses its exact finite catalog amount"),
			InvalidAmountCount, 0);
		Test.TestEqual(TEXT("Every runtime deposit retains a legal Miner approach cell"),
			InaccessibleDepositCount, 0);
		Test.TestEqual(TEXT("Runtime deposit counts remain inside each authored spawn envelope"),
			SpawnEnvelopeFailureCount, 0);
		for (const FName RequiredRuleId : Generator.GetRequiredSystemResourceRuleIds())
		{
			FString ResourceIdString = RequiredRuleId.ToString();
			ResourceIdString.RemoveFromStart(TEXT("ResourceV2."));
			Test.TestTrue(
				*FString::Printf(TEXT("Forced seed contains %s"), *ResourceIdString),
				SystemResourceIds.Contains(FName(*ResourceIdString)));
		}
		Test.AddInfo(FString::Printf(
			TEXT("Runtime seed %d | planets=%d environments=%d deposits=%d signatureEntries=%d"),
			ExpectedRootSeed,
			PlanetCount,
			UniqueEnvironments.Num(),
			DepositCount,
			BodySignatures.Num()));
		return FString::Join(BodySignatures, TEXT("||"));
	}

	class FRunDeterministicRuntimeSeedsPIECommand final : public IAutomationLatentCommand
	{
	public:
		explicit FRunDeterministicRuntimeSeedsPIECommand(FAutomationTestBase& InTest)
			: Test(InTest)
			, StartTimeSeconds(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			UWorld* World = FindPIEWorld();
			ASRSolarSystemGenerator* Generator = nullptr;
			if (IsValid(World))
			{
				for (TActorIterator<ASRSolarSystemGenerator> It(World); It; ++It)
				{
					Generator = *It;
					break;
				}
			}
			if (IsValid(World)
				&& IsValid(Generator)
				&& !Generator->IsRuntimeSystemGenerationInProgress())
			{
				const int32 Seeds[] = { 104729, 130363, 104729 };
				if (SeedIndex < UE_ARRAY_COUNT(Seeds))
				{
					ASRCelestialBody* Star = Generator->GenerateRuntimeSystemForSeed(Seeds[SeedIndex]);
					if (!IsValid(Star))
					{
						Test.AddError(FString::Printf(
							TEXT("Could not force runtime generation for seed %d."),
							Seeds[SeedIndex]));
						return true;
					}
					Signatures.Add(BuildAndValidateRuntimeSignature(
						Test, *World, *Generator, Seeds[SeedIndex]));
					++SeedIndex;
					return false;
				}
				Test.TestTrue(TEXT("Different root seeds create different runtime layouts"),
					Signatures.Num() == 3 && Signatures[0] != Signatures[1]);
				Test.TestTrue(TEXT("Replaying one root seed reproduces every planet and deposit cell"),
					Signatures.Num() == 3 && Signatures[0] == Signatures[2]);
				return true;
			}

			if (FPlatformTime::Seconds() - StartTimeSeconds < 180.0)
			{
				return false;
			}
			Test.AddError(TEXT("PIE multi-seed generation did not finish within 180 seconds."));
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
		int32 SeedIndex = 0;
		TArray<FString> Signatures;
	};
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceEconomyAuthoredSeedSoakTest,
	"StarRovers.ResourceSystem.Phase22.SeedSoak.AuthoredPortfolio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceEconomyAuthoredSeedSoakTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::ResourceEconomySoakTests;
	ASRSolarSystemGenerator* GeneratorCDO = LoadGeneratorCDO();
	TestNotNull(TEXT("The authored Solar System generator loads"), GeneratorCDO);
	if (!IsValid(GeneratorCDO))
	{
		return false;
	}

	FSRResourceEconomySoakRules Rules;
	Rules.FirstSeed = 1;
	Rules.SeedCount = 512;
	Rules.MinimumPlanetCount = GeneratorCDO->GetMinimumPlanetCount();
	Rules.MaximumPlanetCount = GeneratorCDO->GetMaximumPlanetCount();
	Rules.MinimumUniquePlanetTypes = GeneratorCDO->GetMinimumUniquePlanetTypes();
	const TArray<const USRPlanetDataAsset*> Candidates = BuildPlanetCandidates(*GeneratorCDO);
	const FSRResourceEconomySoakReport Report = FSRResourceEconomySoakModel::Run(
		Candidates,
		GeneratorCDO->GetRequiredSystemResourceRuleIds(),
		Rules);
	if (!Report.bPassed)
	{
		AddError(Report.FailureReason);
	}
	AddInfo(Report.BuildSummaryString());
	TestTrue(TEXT("All authored multi-seed balance gates pass"), Report.bPassed);
	TestEqual(TEXT("All 512 deterministic seeds pass"), Report.PassedSeedCount, 512);
	TestEqual(TEXT("No deterministic seed violates a resource contract"), Report.FailedSeedCount, 0);
	TestEqual(TEXT("All six environments appear across sampled systems"),
		Report.EnvironmentAppearanceCount.Num(), 6);
	for (const TPair<FString, int32>& Pair : Report.EnvironmentAppearanceCount)
	{
		TestTrue(
			*FString::Printf(TEXT("%s appears in at least one quarter of systems"), *Pair.Key),
			Pair.Value >= 128 && Pair.Value <= 512);
	}
	TestTrue(TEXT("Every system guarantees at least four relocatable complete fronts"),
		Report.MinimumObservedGuaranteedCompleteFronts >= 4);
	TestTrue(TEXT("Potential fronts never fall below guaranteed fronts"),
		Report.MinimumObservedPotentialCompleteFronts
			>= Report.MinimumObservedGuaranteedCompleteFronts);
	for (const FName RequiredRuleId : GeneratorCDO->GetRequiredSystemResourceRuleIds())
	{
		const FSRResourceEconomyRuleSoakRange* Range = Report.RuleRanges.Find(RequiredRuleId);
		TestTrue(
			*FString::Printf(TEXT("Every seed contains a finite source for %s"), *RequiredRuleId.ToString()),
			Range
				&& Range->MinimumSourcePlanetCount >= 1
				&& Range->MinimumGuaranteedDepositCount > 0
				&& Range->MinimumPotentialDepositCount >= Range->MinimumGuaranteedDepositCount);
	}
	TestTrue(TEXT("One deposit mines raw in eight minutes but feeds a ten-second line for twenty minutes"),
		FMath::IsNearlyEqual(Report.RawSingleDepositMiningSeconds, 480.0)
			&& FMath::IsNearlyEqual(Report.LineFedSingleFrontSeconds, 1200.0));
	TestTrue(TEXT("One basic front buys recovery time but cannot win alone"),
		Report.BasicLineResult.Outcome != ESRStellarRunOutcome::Victory
			&& Report.BasicLineResult.SupplyDeliveryCount == 120
			&& FMath::IsNearlyEqual(Report.BasicLineResult.FirstSupplyExhaustionSeconds, 1520.0));
	TestTrue(TEXT("A pre-dispatched remote optimized front wins inside 25-35 minutes"),
		Report.DistributedExpansionResult.Outcome == ESRStellarRunOutcome::Victory
			&& Report.DistributedExpansionResult.CompletionSeconds >= 1500.0
			&& Report.DistributedExpansionResult.CompletionSeconds <= 2100.0);

	const FSRResourceEconomySoakReport ReplayReport = FSRResourceEconomySoakModel::Run(
		Candidates,
		GeneratorCDO->GetRequiredSystemResourceRuleIds(),
		Rules);
	TestTrue(TEXT("The complete 512-seed CSV is byte-for-byte deterministic"),
		Report.BuildCsv() == ReplayReport.BuildCsv());
	return !HasAnyErrors();
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceEconomyRuntimeSeedReplayPIETest,
	"StarRovers.ResourceSystem.Phase22.SeedSoak.PIE.RuntimeSeedReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceEconomyRuntimeSeedReplayPIETest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::ResourceEconomySoakTests::FRunDeterministicRuntimeSeedsPIECommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

#endif
