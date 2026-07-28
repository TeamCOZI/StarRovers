#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRStellarRunResultPresentation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarRunVictoryPresentationTest,
	"StarRovers.UI.RunCommand.RunResult.Victory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarRunVictoryPresentationTest::RunTest(const FString& Parameters)
{
	FSRStellarRunResultSnapshot Snapshot;
	Snapshot.bHasStar = true;
	Snapshot.RunProgress.Outcome = ESRStellarRunOutcome::Victory;
	Snapshot.RunProgress.TotalDeliveredFuel = 100000.0;
	Snapshot.RunProgress.VictoryDeliveryTarget = 100000.0;
	Snapshot.RunProgress.RecentIncomePerSecond = 125.0;
	Snapshot.RunProgress.RequiredIncomePerSecond = 100.0;
	Snapshot.RunProgress.CompletionSimulationSeconds = 1800.0;

	const FSRStellarRunResultPresentation Presentation =
		FSRStellarRunResultPresentationBuilder::Build(Snapshot);
	TestTrue(TEXT("Victory uses the positive terminal presentation"), Presentation.bVictory);
	TestEqual(TEXT("Victory title is unambiguous"), Presentation.TitleText.ToString(), FString(TEXT("VICTORY")));
	TestTrue(TEXT("Victory result exposes cumulative delivery"),
		Presentation.DetailText.ToString().Contains(TEXT("100.0K")));
	TestTrue(TEXT("Victory result exposes completion time"),
		Presentation.DetailText.ToString().Contains(TEXT("30:00")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarRunDefeatPresentationTest,
	"StarRovers.UI.RunCommand.RunResult.Defeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarRunDefeatPresentationTest::RunTest(const FString& Parameters)
{
	FSRStellarRunResultSnapshot Snapshot;
	Snapshot.bHasStar = true;
	Snapshot.RunProgress.Outcome = ESRStellarRunOutcome::Defeat;
	Snapshot.StoredFuel = 0.0;
	Snapshot.ReferenceFuel = 1000.0;

	const FSRStellarRunResultPresentation Presentation =
		FSRStellarRunResultPresentationBuilder::Build(Snapshot);
	TestFalse(TEXT("Defeat never uses victory styling"), Presentation.bVictory);
	TestEqual(TEXT("Defeat title remains unambiguous"), Presentation.TitleText.ToString(), FString(TEXT("DEFEAT")));
	TestTrue(TEXT("Defeat result explains the supernova state"),
		Presentation.DetailText.ToString().Contains(TEXT("초신성")));
	return true;
}

#endif
