#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceSystemContent.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRFiniteResourceEconomy.h"
#include "Simulation/SRResourceReserveModel.h"

namespace StarRovers::FiniteResourceEconomyTests
{
	FSRResourceDepositInstance MakeDeposit(
		FName ResourceId,
		int32 TotalAmount,
		int32 RemainingAmount,
		int32 Index)
	{
		FSRResourceDepositInstance Deposit;
		Deposit.OccupantId = FName(*FString::Printf(TEXT("Deposit_%d"), Index));
		Deposit.ResourceId = ResourceId;
		Deposit.TotalAmount = TotalAmount;
		Deposit.RemainingAmount = RemainingAmount;
		return Deposit;
	}

	FSRRunBalanceScenario MakePressureScenario()
	{
		FSRRunBalanceScenario Scenario;
		Scenario.ScenarioId = TEXT("FiniteResourceReference");
		Scenario.DurationSeconds = 2100.0;
		Scenario.OutputSampleIntervalSeconds = 10.0;
		Scenario.SecondsPerCycle = 60.0;
		Scenario.StartingStoredFuel = 20000.0;
		Scenario.DemandCurve = ESRRunBalanceDemandCurve::StellarPressureV2;
		Scenario.DemandCurveV2.InitialDemandPerSecond = 50.0;
		Scenario.DemandCurveV2.GraceCycleCount = 2;
		Scenario.DemandCurveV2.DemandIncreasePerCycle = 5.0;
		Scenario.DemandCurveV2.MaximumDemandPerSecond = 100.0;
		Scenario.PressureRulesV2.FuelReserveCapacity = 20000.0;
		Scenario.PressureRulesV2.RedGiantEmergencyReserveRatio = 1.0;
		return Scenario;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFiniteResourceCatalogContractTest,
	"StarRovers.ResourceSystem.Phase20.FiniteEconomy.CatalogContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFiniteResourceCatalogContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FSRFiniteResourceEconomyContract Contract =
		FSRFiniteResourceEconomyModel::BuildReferenceContract();
	if (!Contract.bIsValid)
	{
		AddError(Contract.FailureReason);
	}
	TestTrue(TEXT("The finite economy derives a valid contract from gameplay catalogs"), Contract.bIsValid);
	TestEqual(TEXT("All five Card types are required"), Contract.RequiredCardTypeCount, 5);
	TestEqual(TEXT("One synchronized Card deposit set supports one hundred twenty batches"),
		Contract.BatchesPerCardDepositSet, 120);
	TestEqual(TEXT("Ore and Biomass deposits each contain one hundred eighty units"),
		Contract.RawUnitsPerUtilityDeposit, 180);
	TestTrue(TEXT("Basic Full House throughput remains 824 Energy per ten seconds"),
		FMath::IsNearlyEqual(Contract.BasicFuelEnergyPerBatch, 824.0)
			&& FMath::IsNearlyEqual(Contract.BasicFuelPerSecond, 82.4));
	TestTrue(TEXT("Optimized convergence throughput remains 1180 Energy per ten seconds"),
		FMath::IsNearlyEqual(Contract.OptimizedFuelEnergyPerBatch, 1180.0)
			&& FMath::IsNearlyEqual(Contract.OptimizedFuelPerSecond, 118.0));

	int32 IndustrialSupplyDeposit = -1;
	TestFalse(TEXT("Fabricated Industrial Supply never receives a natural deposit"),
		FSRResourceSystemContent::TryGetDepositTotalAmount(
			ESRResourceContentPresetV2::IndustrialSupply,
			IndustrialSupplyDeposit));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceReserveAggregationTest,
	"StarRovers.ResourceSystem.Phase20.FiniteEconomy.ReserveAggregation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceReserveAggregationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::FiniteResourceEconomyTests;
	TArray<FSRReferenceResourceDefinitionV2> Cards;
	FSRResourceSystemContent::GetAllReferenceResourceDefinitions(Cards);
	TArray<FSRResourceDepositInstance> Deposits;
	const int32 RemainingByCard[] = { 80, 64, 48, 24, 12 };
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		Deposits.Add(MakeDeposit(
			Cards[Index].ResourceId,
			Cards[Index].DepositTotalAmount,
			RemainingByCard[Index],
			Index));
	}

	FSRUtilityResourceDefinitionV2 Ore;
	FSRUtilityResourceDefinitionV2 Biomass;
	FSRResourceSystemContent::TryGetUtilityResourceDefinition(
		ESRResourceContentPresetV2::CommonOre, Ore);
	FSRResourceSystemContent::TryGetUtilityResourceDefinition(
		ESRResourceContentPresetV2::BiomassFeedstock, Biomass);
	Deposits.Add(MakeDeposit(Ore.ResourceId, 180, 90, 5));
	Deposits.Add(MakeDeposit(Biomass.ResourceId, 180, 45, 6));

	const FSRResourceReserveSnapshot Snapshot =
		FSRResourceReserveModel::BuildSnapshot(Deposits);
	TestTrue(TEXT("All seven finite deposits are aggregated without mutation"),
		Snapshot.bHasDeposits
			&& Snapshot.DepositCount == 7
			&& Snapshot.ActiveDepositCount == 7
			&& Snapshot.DepletedDepositCount == 0);
	TestEqual(TEXT("The least abundant Card bounds complete Fuel batches"),
		Snapshot.PotentialFuelBatchCount, static_cast<int64>(12));
	TestEqual(TEXT("The limiting Card id remains actionable"),
		Snapshot.LimitingReferenceCardId, Cards.Last().ResourceId);
	TestEqual(TEXT("Ore and Biomass pairs bound Industrial Supply cycles"),
		Snapshot.PotentialIndustrialSupplyCycleCount, static_cast<int64>(45));
	TestEqual(TEXT("Every reference Card type remains covered"),
		Snapshot.CoveredReferenceCardTypeCount, 5);

	Deposits.RemoveAt(4);
	const FSRResourceReserveSnapshot MissingFamily =
		FSRResourceReserveModel::BuildSnapshot(Deposits);
	TestEqual(TEXT("A missing Card family makes another complete batch impossible"),
		MissingFamily.PotentialFuelBatchCount, static_cast<int64>(0));
	TestEqual(TEXT("The missing family becomes the limiting diagnosis"),
		MissingFamily.LimitingReferenceCardId, Cards.Last().ResourceId);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceReservePressureAndInfiniteTest,
	"StarRovers.ResourceSystem.Phase20.FiniteEconomy.PressureAndLegacyInfinite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceReservePressureAndInfiniteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::FiniteResourceEconomyTests;
	TestEqual(TEXT("Twenty-five percent is the visible low-reserve boundary"),
		FSRResourceReserveModel::ResolvePressure(true, 0.25f),
		ESRResourceReservePressure::Low);
	TestEqual(TEXT("Ten percent is critical"),
		FSRResourceReserveModel::ResolvePressure(true, 0.10f),
		ESRResourceReservePressure::Critical);
	TestEqual(TEXT("Zero remaining is depleted"),
		FSRResourceReserveModel::ResolvePressure(true, 0.0f),
		ESRResourceReservePressure::Depleted);

	TArray<FSRReferenceResourceDefinitionV2> Cards;
	FSRResourceSystemContent::GetAllReferenceResourceDefinitions(Cards);
	TArray<FSRResourceDepositInstance> InfiniteDeposits;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		InfiniteDeposits.Add(MakeDeposit(
			Cards[Index].ResourceId,
			MAX_int32,
			MAX_int32,
			Index));
	}
	const FSRResourceReserveSnapshot Infinite =
		FSRResourceReserveModel::BuildSnapshot(InfiniteDeposits);
	TestTrue(TEXT("Legacy infinite deposits remain explicit rather than overflowing sums"),
		Infinite.InfiniteDepositCount == 5
			&& Infinite.bPotentialFuelBatchesInfinite
			&& Infinite.Pressure == ESRResourceReservePressure::Healthy);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFiniteResourceRunEnvelopeTest,
	"StarRovers.ResourceSystem.Phase20.FiniteEconomy.RunEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFiniteResourceRunEnvelopeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::FiniteResourceEconomyTests;
	FString FailureReason;
	FSRRunBalanceSupplyStage BasicStage;
	TestTrue(TEXT("One basic finite Card set builds a supply stage"),
		FSRFiniteResourceEconomyModel::BuildReferenceSupplyStage(
			false, 1, 300.0, 30.0, BasicStage, FailureReason));
	TestEqual(TEXT("The stage stops after exactly one hundred twenty five-Card batches"),
		BasicStage.MaximumDeliveryCount, 120);

	FSRRunBalanceScenario BasicScenario = MakePressureScenario();
	BasicScenario.SupplyStages.Add(BasicStage);
	const FSRRunBalanceResult Basic = FSRRunBalanceSimulator::Simulate(BasicScenario);
	TestTrue(TEXT("A single deposit set is recovery time, not a complete Run solution"),
		Basic.Outcome != ESRStellarRunOutcome::Victory
			&& Basic.SupplyDeliveryCount == 120
			&& Basic.ExhaustedSupplyStageCount == 1
			&& FMath::IsNearlyEqual(Basic.FirstSupplyExhaustionSeconds, 1520.0));

	FSRRunBalanceSupplyStage OptimizedExpansion;
	TestTrue(TEXT("A second optimized deposit set builds an expansion stage"),
		FSRFiniteResourceEconomyModel::BuildReferenceSupplyStage(
			true, 1, 1500.0, 0.0, OptimizedExpansion, FailureReason));
	FSRRunBalanceScenario ExpandedScenario = MakePressureScenario();
	ExpandedScenario.SupplyStages.Add(BasicStage);
	ExpandedScenario.SupplyStages.Add(OptimizedExpansion);
	const FSRRunBalanceResult Expanded =
		FSRRunBalanceSimulator::Simulate(ExpandedScenario);
	AddInfo(FString::Printf(
		TEXT("Finite economy reference | basic outcome %d at %.0fs, deliveries %d | expanded outcome %d at %.0fs, deliveries %d"),
		static_cast<int32>(Basic.Outcome),
		Basic.SimulatedUntilSeconds,
		Basic.SupplyDeliveryCount,
		static_cast<int32>(Expanded.Outcome),
		Expanded.CompletionSeconds,
		Expanded.SupplyDeliveryCount));
	TestTrue(TEXT("Discovering a second set and optimizing its Line wins in the 25-35 minute target"),
		Expanded.Outcome == ESRStellarRunOutcome::Victory
			&& Expanded.bCompletedInsideTargetWindow
			&& Expanded.CompletionSeconds >= 1500.0
			&& Expanded.CompletionSeconds <= 2100.0);
	return !HasAnyErrors();
}

#endif
