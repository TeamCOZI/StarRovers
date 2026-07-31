#if WITH_DEV_AUTOMATION_TESTS

#include "Celestial/SRStar.h"

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStarTwoStageHealthDepletionTest,
	"StarRovers.Celestial.StarHealth.TwoStageDepletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStarTwoStageHealthDepletionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("SRStarHealthTestWorld")));
	TestNotNull(TEXT("The test world is created."), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	ASRStar* Star = TestWorld->SpawnActor<ASRStar>();
	TestNotNull(TEXT("A runtime star can be spawned."), Star);
	if (!Star)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	const FSRStellarPatternContract Contract = Star->GetStellarPatternContract();
	Star->SetCurrentStellarHealth(0.0);
	FSRStellarContractState State = Star->GetStellarContractState();
	TestEqual(
		TEXT("The first health depletion advances Main Sequence to Red Giant."),
		State.EvolutionStage,
		ESRStellarEvolutionStage::RedGiant);
	TestTrue(
		TEXT("Red Giant begins with a refilled stage health reserve."),
		FMath::IsNearlyEqual(State.CurrentStellarHealth, Contract.StartingStellarHealth));
	TestFalse(TEXT("The first depletion is not Game Over."), State.bSupernovaGameOver);

	Star->SetCurrentStellarHealth(0.0);
	State = Star->GetStellarContractState();
	TestEqual(
		TEXT("The second health depletion advances Red Giant to Supernova."),
		State.EvolutionStage,
		ESRStellarEvolutionStage::Supernova);
	TestTrue(TEXT("Supernova triggers Game Over."), State.bSupernovaGameOver);
	TestTrue(TEXT("Supernova health remains zero."), FMath::IsNearlyZero(State.CurrentStellarHealth));

	TestWorld->DestroyWorld(false);
	return true;
}

#endif
