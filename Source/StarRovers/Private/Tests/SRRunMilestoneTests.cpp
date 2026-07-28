#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Celestial/SRMoonDataAsset.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Celestial/SRStarDataAsset.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRRunMilestoneSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFirstFuelMilestoneOrderingTest,
	"StarRovers.UI.RunCommand.FirstFuelMilestone.Ordering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFirstFuelMilestoneOrderingTest::RunTest(const FString& Parameters)
{
	FSRFirstFuelMilestoneFacts Facts;
	TestEqual(TEXT("A new Run begins by placing an Extractor"),
		FSRFirstFuelMilestoneModel::ResolveCurrentMilestone(Facts),
		ESRFirstFuelMilestone::PlaceExtractor);
	TestEqual(TEXT("No first-fuel milestone is complete at Run start"),
		FSRFirstFuelMilestoneModel::ResolveCompletedMilestoneCount(Facts),
		0);

	Facts.bExtractorPlaced = true;
	TestEqual(TEXT("Placing an Extractor advances to actual Card production"),
		FSRFirstFuelMilestoneModel::ResolveCurrentMilestone(Facts),
		ESRFirstFuelMilestone::ExtractFirstCard);
	Facts.bFirstCardExtracted = true;
	Facts.bFamilyProcessorPlaced = true;
	Facts.bFirstCardProcessed = true;
	TestEqual(TEXT("A processed Card advances to final-fabricator construction"),
		FSRFirstFuelMilestoneModel::ResolveCurrentMilestone(Facts),
		ESRFirstFuelMilestone::PlaceStellarFuelFabricator);
	Facts.bStellarFuelFabricatorPlaced = true;
	Facts.bFirstStellarFuelFabricated = true;
	Facts.bHubPlaced = true;
	Facts.bFirstStellarFuelLaunched = true;
	TestEqual(TEXT("A launched fuel waits for authoritative Star delivery"),
		FSRFirstFuelMilestoneModel::ResolveCurrentMilestone(Facts),
		ESRFirstFuelMilestone::DeliverFirstStellarFuel);
	Facts.bFirstStellarFuelDelivered = true;
	TestEqual(TEXT("Actual Star delivery completes all nine steps"),
		FSRFirstFuelMilestoneModel::ResolveCurrentMilestone(Facts),
		ESRFirstFuelMilestone::Complete);
	TestEqual(TEXT("The completed count matches the authored milestone count"),
		FSRFirstFuelMilestoneModel::ResolveCompletedMilestoneCount(Facts),
		FSRFirstFuelMilestoneModel::TotalMilestoneCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFirstFuelMilestoneLateObservationTest,
	"StarRovers.UI.RunCommand.FirstFuelMilestone.LateObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFirstFuelMilestoneLateObservationTest::RunTest(const FString& Parameters)
{
	FSRFirstFuelMilestoneFacts ExtractedFacts;
	ExtractedFacts.bFirstCardExtracted = true;
	FSRFirstFuelMilestoneModel::ApplyConsistency(ExtractedFacts);
	TestTrue(TEXT("An extracted Card keeps its completed Extractor step after the source was removed"),
		ExtractedFacts.bExtractorPlaced);

	FSRFirstFuelMilestoneFacts Facts;
	Facts.bFirstStellarFuelDelivered = true;
	FSRFirstFuelMilestoneModel::ApplyConsistency(Facts);

	TestTrue(TEXT("A late delivery observation implies the already-consumed upstream Card flow"),
		Facts.bExtractorPlaced
		&& Facts.bFirstCardExtracted
		&& Facts.bFamilyProcessorPlaced
		&& Facts.bFirstCardProcessed
		&& Facts.bStellarFuelFabricatorPlaced
		&& Facts.bFirstStellarFuelFabricated);
	TestTrue(TEXT("A delivered fuel also proves a Hub launch occurred"),
		Facts.bHubPlaced && Facts.bFirstStellarFuelLaunched);
	TestEqual(TEXT("Late UI creation cannot regress a completed first-fuel Run"),
		FSRFirstFuelMilestoneModel::ResolveCurrentMilestone(Facts),
		ESRFirstFuelMilestone::Complete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFirstFuelConstructionWorldContractTest,
	"StarRovers.UI.RunCommand.FirstFuelMilestone.ConstructionWorldContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFirstFuelConstructionWorldContractTest::RunTest(const FString& Parameters)
{
	const USRPlanetDataAsset* Planet = NewObject<USRPlanetDataAsset>(GetTransientPackage());
	const USRMoonDataAsset* Moon = NewObject<USRMoonDataAsset>(GetTransientPackage());
	const USRStarDataAsset* Star = NewObject<USRStarDataAsset>(GetTransientPackage());
	TestTrue(TEXT("Planet definitions explicitly expose a construction workspace"),
		Planet->BuildData().bCanConstruct);
	TestTrue(TEXT("Moon definitions explicitly expose a construction workspace"),
		Moon->BuildData().bCanConstruct);
	TestFalse(TEXT("The primary Star remains a fuel destination, not a construction workspace"),
		Star->BuildData().bCanConstruct);
	return true;
}

#endif
