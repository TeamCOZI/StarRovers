#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SRFamilyLineBalanceV2.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Misc/AutomationTest.h"

namespace StarRovers::FamilyFacilityBalanceV2Tests
{
	struct FExpectedFacility
	{
		ESRFacilityContentPresetV2 Preset;
		ESRFacilityLineRoleV2 LineRole;
		double EnergyDelta;
		float CycleSeconds;
		int32 OperationalLoad;
	};

	FSRFacilityResourceV2Evaluation ApplyFacility(
		FSRResourceInstance& Resource,
		ESRFacilityContentPresetV2 Preset,
		ESRFacilityTemperatureState Temperature = ESRFacilityTemperatureState::Normal)
	{
		USRFacilityDataAsset* Facility = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		FSRResourceSystemContent::ApplyFacilityPreset(*Facility, Preset);
		FSRFacilityInstance Instance;
		Instance.FacilityDataAsset = Facility;
		Instance.TemperatureState = Temperature;
		Instance.bProcessEnabled = true;
		const FSRFacilityResourceV2Evaluation Evaluation =
			FSRFacilityResourceV2Processor::Evaluate(Instance, Resource, TEXT("BalanceBody"));
		if (Evaluation.IsSuccess())
		{
			Resource = Evaluation.ResourceProcessResult.OutputResource;
		}
		return Evaluation;
	}

	bool HasState(const FSRResourceInstance& Resource, ESRResourceFamilyState State)
	{
		return (Resource.ActiveFamilyStateFlags
			& StarRovers::Resources::GetFamilyStateBit(State)) != 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFamilyFacilityBalanceCatalogTest,
	"StarRovers.ResourceSystem.Balance.FamilyFacilities.RoleAndCostMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFamilyFacilityBalanceCatalogTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FamilyFacilityBalanceV2Tests;
	const TArray<FExpectedFacility> Expectations = {
		{ ESRFacilityContentPresetV2::PulseProcessor, ESRFacilityLineRoleV2::UniversalBridge, 1.0, 2.0f, 1 },
		{ ESRFacilityContentPresetV2::CompressionMill, ESRFacilityLineRoleV2::UniversalBridge, 3.0, 5.0f, 3 },
		{ ESRFacilityContentPresetV2::InductionForge, ESRFacilityLineRoleV2::Primer, 4.0, 4.0f, 3 },
		{ ESRFacilityContentPresetV2::CryoPress, ESRFacilityLineRoleV2::Payoff, 3.0, 4.0f, 3 },
		{ ESRFacilityContentPresetV2::AnnealingChamber, ESRFacilityLineRoleV2::Recovery, 0.0, 6.0f, 2 },
		{ ESRFacilityContentPresetV2::ResonanceMill, ESRFacilityLineRoleV2::Repeater, 3.0, 3.0f, 2 },
		{ ESRFacilityContentPresetV2::FacetShifter, ESRFacilityLineRoleV2::Recovery, 2.0, 3.0f, 2 },
		{ ESRFacilityContentPresetV2::GrowthVat, ESRFacilityLineRoleV2::Primer, 0.0, 5.0f, 1 },
		{ ESRFacilityContentPresetV2::EnzymeLoom, ESRFacilityLineRoleV2::Payoff, 2.0, 2.0f, 2 },
		{ ESRFacilityContentPresetV2::SporePress, ESRFacilityLineRoleV2::Payoff, 5.0, 5.0f, 1 },
		{ ESRFacilityContentPresetV2::ArcAmplifier, ESRFacilityLineRoleV2::Burst, 4.0, 2.0f, 5 },
		{ ESRFacilityContentPresetV2::GroundingCoil, ESRFacilityLineRoleV2::Stabilizer, 1.0, 3.0f, 1 },
		{ ESRFacilityContentPresetV2::NullSink, ESRFacilityLineRoleV2::Sacrifice, -3.0, 2.0f, 1 },
		{ ESRFacilityContentPresetV2::EchoChamber, ESRFacilityLineRoleV2::Payoff, 5.0, 5.0f, 3 },
	};

	for (const FExpectedFacility& Expected : Expectations)
	{
		FSRFacilityContentDefinitionV2 Definition;
		const bool bFound = FSRResourceSystemContent::TryGetFacilityDefinition(
			Expected.Preset,
			Definition);
		TestTrue(TEXT("Every balanced processing facility remains in the content catalog"), bFound);
		if (!bFound)
		{
			continue;
		}
		TestEqual(TEXT("The facility exposes its intended Line role"), Definition.LineRole, Expected.LineRole);
		TestTrue(TEXT("The additive Energy value matches the balance contract"),
			FMath::IsNearlyEqual(Definition.FacilityEnergyDelta, Expected.EnergyDelta));
		TestTrue(TEXT("The base cycle matches the balance contract"),
			FMath::IsNearlyEqual(Definition.CycleSeconds, Expected.CycleSeconds));
		TestEqual(TEXT("The Operational Load matches the balance contract"),
			Definition.OperationalLoad,
			Expected.OperationalLoad);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFamilyFacilityReferenceCycleTest,
	"StarRovers.ResourceSystem.Balance.FamilyFacilities.ReferenceCycles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFamilyFacilityReferenceCycleTest::RunTest(const FString& Parameters)
{
	struct FExpectedCycle
	{
		ESRResourceFamily Family;
		double EnergyGain;
		double EffectiveSeconds;
		double LoadSeconds;
		int32 CommittedLoad;
		int32 StepCount;
	};
	const TArray<FExpectedCycle> Expectations = {
		{ ESRResourceFamily::Metal, 12.0, 14.4, 37.2, 8, 3 },
		{ ESRResourceFamily::Crystal, 19.0, 14.25, 28.5, 8, 4 },
		{ ESRResourceFamily::Organic, 8.0, 7.0, 9.0, 3, 2 },
		{ ESRResourceFamily::Plasma, 14.0, 8.175, 24.975, 11, 3 },
		{ ESRResourceFamily::Void, 7.0, 7.0, 17.0, 4, 2 },
	};

	TMap<ESRResourceFamily, FSRFamilyLineBalanceResultV2> Results;
	for (const FExpectedCycle& Expected : Expectations)
	{
		ESRResourceContentPresetV2 ResourcePreset;
		TArray<FSRFamilyLineBalanceStepV2> Steps;
		TestTrue(TEXT("Every Family owns a canonical one-cycle sequence"),
			FSRFamilyLineBalanceV2::BuildReferenceCycle(Expected.Family, ResourcePreset, Steps));
		const FSRFamilyLineBalanceResultV2 Result = FSRFamilyLineBalanceV2::Evaluate(
			ResourcePreset,
			Steps);
		Results.Add(Expected.Family, Result);
		TestTrue(TEXT("The canonical cycle evaluates without World state"), Result.bValid);
		TestEqual(TEXT("The cycle keeps the intended Family"), Result.Family, Expected.Family);
		TestEqual(TEXT("The cycle contains the intended number of physical steps"),
			Result.StepCount,
			Expected.StepCount);
		TestTrue(TEXT("The cycle Energy gain matches the balance contract"),
			FMath::IsNearlyEqual(Result.EnergyGain, Expected.EnergyGain));
		TestTrue(TEXT("Refinement-adjusted time matches the balance contract"),
			FMath::IsNearlyEqual(Result.TotalEffectiveSeconds, Expected.EffectiveSeconds, 0.001));
		TestTrue(TEXT("Load-seconds match the balance contract"),
			FMath::IsNearlyEqual(Result.TotalLoadSeconds, Expected.LoadSeconds, 0.001));
		TestEqual(TEXT("Committed Facility Load matches the balance contract"),
			Result.CommittedOperationalLoad,
			Expected.CommittedLoad);
	}

	const FSRFamilyLineBalanceResultV2& Organic = Results.FindChecked(ESRResourceFamily::Organic);
	const FSRFamilyLineBalanceResultV2& Plasma = Results.FindChecked(ESRResourceFamily::Plasma);
	const FSRFamilyLineBalanceResultV2& Crystal = Results.FindChecked(ESRResourceFamily::Crystal);
	TestTrue(TEXT("Organic is the capacity-efficient Family reference cycle"),
		Organic.GetEnergyPerLoadSecond() > Plasma.GetEnergyPerLoadSecond()
			&& Organic.GetEnergyPerLoadSecond() > Crystal.GetEnergyPerLoadSecond());
	TestTrue(TEXT("Plasma is the high-throughput, high-Load Family reference cycle"),
		Plasma.GetEnergyPerEffectiveSecond() > Crystal.GetEnergyPerEffectiveSecond()
			&& Plasma.CommittedOperationalLoad > Crystal.CommittedOperationalLoad);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFamilyFacilityUniversalBridgeTest,
	"StarRovers.ResourceSystem.Balance.FamilyFacilities.UniversalBridgeCannotCashMerit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFamilyFacilityUniversalBridgeTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FamilyFacilityBalanceV2Tests;

	FSRResourceInstance Metal;
	FSRResourceSystemContent::MakeReferenceResourceInstance(
		ESRResourceContentPresetV2::HeliosIron,
		TEXT("BalanceBody"),
		Metal);
	ApplyFacility(Metal, ESRFacilityContentPresetV2::InductionForge, ESRFacilityTemperatureState::Hot);
	const FSRFacilityResourceV2Evaluation MetalBridge = ApplyFacility(
		Metal,
		ESRFacilityContentPresetV2::PulseProcessor,
		ESRFacilityTemperatureState::Cold);
	TestTrue(TEXT("A Universal Bridge remains a valid fallback process"), MetalBridge.IsSuccess());
	TestFalse(TEXT("A Universal Bridge never claims specialist affinity"),
		MetalBridge.ProcessSpec.bIsFamilySpecialist);
	TestFalse(TEXT("A Cold Universal Bridge cannot activate Metal Tempered"),
		HasState(Metal, ESRResourceFamilyState::Tempered));
	TestTrue(TEXT("The Bridge still advances Metal Work Strain"),
		Metal.ProcessingMemory.GeneralProcessesSinceReset == 2);

	FSRResourceInstance Crystal;
	FSRResourceSystemContent::MakeReferenceResourceInstance(
		ESRResourceContentPresetV2::EchoQuartz,
		TEXT("BalanceBody"),
		Crystal);
	ApplyFacility(Crystal, ESRFacilityContentPresetV2::PulseProcessor);
	const FSRFacilityResourceV2Evaluation CrystalBridge = ApplyFacility(
		Crystal,
		ESRFacilityContentPresetV2::PulseProcessor);
	TestFalse(TEXT("Repeated Universal Bridges cannot activate Crystal Resonant"),
		HasState(Crystal, ESRResourceFamilyState::Resonant));
	TestTrue(TEXT("Universal Crystal repetition still records fracture pressure"),
		Crystal.ProcessingMemory.ConsecutiveSameArchetypeCount == 2
			&& FMath::IsNearlyZero(CrystalBridge.ResourceProcessResult.FamilyEnergyDelta));

	FSRResourceInstance Organic;
	FSRResourceSystemContent::MakeReferenceResourceInstance(
		ESRResourceContentPresetV2::VerdantSpore,
		TEXT("BalanceBody"),
		Organic);
	ApplyFacility(Organic, ESRFacilityContentPresetV2::GrowthVat);
	const FSRFacilityResourceV2Evaluation OrganicBridge = ApplyFacility(
		Organic,
		ESRFacilityContentPresetV2::PulseProcessor);
	TestTrue(TEXT("A Bridge carries Matured forward instead of consuming its payoff"),
		HasState(Organic, ESRResourceFamilyState::Matured));
	TestTrue(TEXT("A Bridge receives no Organic Matured bonus"),
		FMath::IsNearlyZero(OrganicBridge.ResourceProcessResult.FamilyEnergyDelta));

	FSRResourceInstance Void;
	FSRResourceSystemContent::MakeReferenceResourceInstance(
		ESRResourceContentPresetV2::NullPearl,
		TEXT("BalanceBody"),
		Void);
	ApplyFacility(Void, ESRFacilityContentPresetV2::NullSink);
	const FSRFacilityResourceV2Evaluation VoidBridge = ApplyFacility(
		Void,
		ESRFacilityContentPresetV2::PulseProcessor);
	TestTrue(TEXT("A Bridge cannot consume a prepared Void Echo"),
		HasState(Void, ESRResourceFamilyState::Echoing)
			&& Void.ProcessingMemory.StoredFamilyMagnitude > 0.0);
	TestTrue(TEXT("A Bridge still advances Void collapse pressure without an Echo bonus"),
		Void.ProcessingMemory.GeneralProcessesSinceReset == 1
			&& FMath::IsNearlyZero(VoidBridge.ResourceProcessResult.FamilyEnergyDelta));
	return true;
}

#endif
