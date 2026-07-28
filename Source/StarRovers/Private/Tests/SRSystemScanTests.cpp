#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceDataAsset.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRSystemScan.h"

namespace StarRovers::SystemScanTests
{
	FSRSystemScanCandidate MakeCandidate(
		const TCHAR* StableId,
		int32 Score,
		bool bFamilyReady = true,
		bool bBuildAccess = true)
	{
		FSRSystemScanCandidate Candidate;
		Candidate.BodyActor = AActor::StaticClass()->GetDefaultObject<AActor>();
		Candidate.ResourceDataAsset = NewObject<USRResourceDataAsset>(GetTransientPackage());
		Candidate.DepositOccupantId = FName(*FString::Printf(TEXT("Deposit.%s"), StableId));
		Candidate.Family = ESRResourceFamily::Metal;
		Candidate.DepositTotalAmount = 100;
		Candidate.DepositRemainingAmount = 100;
		Candidate.bHasFamilyProcessorAccess = bFamilyReady;
		Candidate.bHasAdjacentBuildAccess = bBuildAccess;
		Candidate.StableCandidateId = FName(StableId);
		Candidate.Score.TotalScore = Score;
		return Candidate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRInitialSystemScanScoringTest,
	"StarRovers.UI.RunCommand.SystemScan.ScoringAndOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRInitialSystemScanScoringTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("The best value in a higher-is-better range normalizes to one"),
		FMath::IsNearlyEqual(
			FSRSystemScanModel::NormalizeHigherIsBetter(6.0f, 2.0f, 6.0f),
			1.0f));
	TestTrue(TEXT("The nearest value in a lower-is-better range normalizes to one"),
		FMath::IsNearlyEqual(
			FSRSystemScanModel::NormalizeLowerIsBetter(100.0f, 100.0f, 500.0f),
			1.0f));
	TestTrue(TEXT("An equal candidate range remains fully competitive instead of losing its category"),
		FMath::IsNearlyEqual(
			FSRSystemScanModel::NormalizeLowerIsBetter(100.0f, 100.0f, 100.0f),
			1.0f));

	const FSRSystemScanScoreBreakdown MaximumScore =
		FSRSystemScanModel::ScoreCandidate(1.0f, 1.0f, 1.0f, true, true);
	TestEqual(TEXT("The five visible score reasons sum to the documented maximum"),
		MaximumScore.TotalScore,
		FSRSystemScanModel::MaximumScore);
	TestEqual(TEXT("Resource quality owns 35 percent of the recommendation"),
		MaximumScore.ResourceQuality,
		FSRSystemScanModel::ResourceQualityWeight);
	TestEqual(TEXT("Capacity headroom owns 20 percent of the recommendation"),
		MaximumScore.CapacityHeadroom,
		FSRSystemScanModel::CapacityHeadroomWeight);

	TArray<FSRSystemScanCandidate> Candidates = {
		StarRovers::SystemScanTests::MakeCandidate(TEXT("UnsafeHighScore"), 100, true, false),
		StarRovers::SystemScanTests::MakeCandidate(TEXT("SafeB"), 80),
		StarRovers::SystemScanTests::MakeCandidate(TEXT("SafeA"), 80),
	};
	FSRSystemScanCandidate Depleted =
		StarRovers::SystemScanTests::MakeCandidate(TEXT("Depleted"), 0);
	Depleted.DepositRemainingAmount = 0;
	Candidates.Add(Depleted);
	FSRSystemScanModel::SortCandidates(Candidates);
	TestEqual(TEXT("A viable start outranks an inaccessible mathematical score"),
		Candidates[0].StableCandidateId,
		FName(TEXT("SafeA")));
	TestEqual(TEXT("Equal candidates use a stable identifier instead of container order"),
		Candidates[1].StableCandidateId,
		FName(TEXT("SafeB")));
	TestEqual(TEXT("An inaccessible deposit remains behind every viable opening"),
		Candidates[2].StableCandidateId,
		FName(TEXT("UnsafeHighScore")));
	TestEqual(TEXT("A depleted deposit can never outrank a mineable opening"),
		Candidates.Last().StableCandidateId,
		FName(TEXT("Depleted")));

	FSRSystemScanSnapshot Portfolio;
	Portfolio.bScanComplete = true;
	Portfolio.RequiredCardResourceCount = 5;
	Portfolio.AvailableRequiredCardResourceCount = 5;
	TestTrue(TEXT("A scan exposes complete five-Card resource coverage"),
		Portfolio.HasCompleteFuelPortfolio());
	Portfolio.MissingRequiredCardResourceIds.Add(TEXT("NullPearl"));
	TestFalse(TEXT("Any named resource gap invalidates the fuel portfolio"),
		Portfolio.HasCompleteFuelPortfolio());
	return true;
}

#endif
