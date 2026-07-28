#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Structure/SRStructureInstanceManagerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFiniteResourceDepositAmountTest,
	"StarRovers.ResourceSystem.InitialProgress.FiniteDepositAmount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFiniteResourceDepositAmountTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A positive authored amount remains finite"),
		FSRResourceDepositAmountModel::ResolveInitialAmount(2),
		2);
	TestTrue(TEXT("A zero Legacy amount retains the explicit infinite contract"),
		FSRResourceDepositAmountModel::IsInfinite(
			FSRResourceDepositAmountModel::ResolveInitialAmount(0)));

	int32 FiniteRemaining = 2;
	TestTrue(TEXT("The first finite extraction succeeds"),
		FSRResourceDepositAmountModel::TryConsumeOne(2, FiniteRemaining));
	TestEqual(TEXT("A finite extraction consumes exactly one Card"),
		FiniteRemaining,
		1);
	TestTrue(TEXT("The final finite extraction succeeds"),
		FSRResourceDepositAmountModel::TryConsumeOne(2, FiniteRemaining));
	TestEqual(TEXT("The finite deposit reaches zero"), FiniteRemaining, 0);
	TestFalse(TEXT("A depleted deposit refuses another extraction"),
		FSRResourceDepositAmountModel::TryConsumeOne(2, FiniteRemaining));

	int32 InfiniteRemaining = MAX_int32;
	TestTrue(TEXT("A Legacy infinite deposit remains harvestable"),
		FSRResourceDepositAmountModel::TryConsumeOne(MAX_int32, InfiniteRemaining));
	TestEqual(TEXT("Harvesting does not decrement the Legacy infinite sentinel"),
		InfiniteRemaining,
		MAX_int32);
	return true;
}

#endif
