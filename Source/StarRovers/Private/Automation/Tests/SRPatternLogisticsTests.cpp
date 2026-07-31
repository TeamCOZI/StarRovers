#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceDataAsset.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "../SRFacilityResourceOperations.h"

#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace StarRovers::PatternLogisticsTests
{
	FSRResourceInstance MakeResource(
		FName ResourceId,
		const FSRPattern& Pattern,
		int32 StackCount = 1,
		USRResourceDataAsset* ResourceDataAsset = nullptr)
	{
		FSRResourceInstance Resource;
		Resource.ResourceInstanceId = FName(TEXT("CargoInstance"));
		Resource.ResourceDataAsset = ResourceDataAsset;
		Resource.ResourceId = ResourceId;
		Resource.Pattern = Pattern;
		Resource.SourcePatternId = FName(TEXT("DepositSource"));
		Resource.SourcePatternSeed = 1729;
		Resource.StackCount = StackCount;
		return Resource;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternPayloadAndStackingTest,
	"StarRovers.Logistics.Pattern.PayloadAndStacking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternPayloadAndStackingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern Pattern;
	Pattern.SetGlyph(1, 1, ESRGlyphType::Metal);
	Pattern.SetGlyph(3, 3, ESRGlyphType::Crystal);
	USRResourceDataAsset* FirstDataAsset = NewObject<USRResourceDataAsset>();
	USRResourceDataAsset* SecondDataAsset = NewObject<USRResourceDataAsset>();
	FirstDataAsset->ResourceId = FName(TEXT("StarIron"));
	SecondDataAsset->ResourceId = FName(TEXT("StarIron"));
	FSRResourceInstance First = StarRovers::PatternLogisticsTests::MakeResource(
		FName(TEXT("StarIron")),
		Pattern,
		2,
		FirstDataAsset);
	FSRResourceInstance Equivalent = StarRovers::PatternLogisticsTests::MakeResource(
		FName(TEXT("StarIron")),
		Pattern,
		1,
		SecondDataAsset);
	Equivalent.ResourceInstanceId = FName(TEXT("AnotherCargoInstance"));
	Equivalent.SourcePatternId = FName(TEXT("AnotherDeposit"));
	Equivalent.SourcePatternSeed = 99;

	TestFalse(
		TEXT("A reset resource is empty cargo, not a transportable Pattern payload."),
		StarRovers::PatternRouting::IsValidPatternPayload(FSRResourceInstance()));
	TestTrue(
		TEXT("A reset resource is the canonical empty-cargo sentinel."),
		StarRovers::PatternRouting::IsEmptyPatternPayload(FSRResourceInstance()));
	TestTrue(
		TEXT("A non-empty canonical Pattern resource is transportable."),
		StarRovers::PatternRouting::IsValidPatternPayload(First));
	TestTrue(
		TEXT("Stack identity ignores asset pointer, provenance, and instance ID."),
		StarRovers::ResourcePatterns::ArePatternPayloadsEquivalent(First, Equivalent));

	FSRFacilityPortInventory PortInventory;
	PortInventory.Capacity = 4;
	PortInventory.PortSpec.RoutingFilter.MatchMode = ESRPatternRoutingMatchMode::ExactPattern;
	PortInventory.PortSpec.RoutingFilter.RequiredPattern = Pattern;
	TestEqual(
		TEXT("The first exact Pattern stack fills two units."),
		StarRovers::FacilityResources::TryAddResourceToInventorySlot(PortInventory, First),
		2);
	TestEqual(
		TEXT("Equivalent Pattern cargo merges into the existing stack."),
		StarRovers::FacilityResources::TryAddResourceToInventorySlot(PortInventory, Equivalent),
		1);
	TestEqual(
		TEXT("The homogeneous slot contains three units."),
		StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory),
		3);

	FSRResourceInstance Different = Equivalent;
	Different.Pattern.SetGlyph(0, 0, ESRGlyphType::Organic);
	TestFalse(
		TEXT("One different Pattern cell prevents stack equivalence."),
		StarRovers::ResourcePatterns::ArePatternPayloadsEquivalent(First, Different));
	TestEqual(
		TEXT("A different Pattern cannot enter the exact-filtered homogeneous slot."),
		StarRovers::FacilityResources::TryAddResourceToInventorySlot(PortInventory, Different),
		0);

	FSRResourceInstance Taken;
	TestEqual(
		TEXT("Stack extraction takes the requested amount."),
		StarRovers::FacilityResources::TryTakeResourceStackFromInventorySlot(PortInventory, 2, Taken),
		2);
	TestTrue(TEXT("Stack extraction preserves all Pattern cells."), Taken.Pattern == Pattern);
	TestEqual(TEXT("Stack extraction preserves source provenance."), Taken.SourcePatternId, First.SourcePatternId);
	TestEqual(
		TEXT("One exact Pattern unit remains in the slot."),
		StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternRoutingFilterTest,
	"StarRovers.Logistics.Pattern.RoutingFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternRoutingFilterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern Pattern;
	Pattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	Pattern.SetGlyph(4, 4, ESRGlyphType::Plasma);
	const FSRResourceInstance Resource = StarRovers::PatternLogisticsTests::MakeResource(
		FName(TEXT("RoutedOre")),
		Pattern);

	FSRPatternRoutingFilter AnyFilter;
	TestTrue(
		TEXT("The default filter accepts every valid Pattern payload."),
		StarRovers::PatternRouting::MatchesRoutingFilter(Resource, AnyFilter));
	AnyFilter.ResourceId = FName(TEXT("OtherOre"));
	TestFalse(
		TEXT("An optional resource identity criterion is enforced."),
		StarRovers::PatternRouting::MatchesRoutingFilter(Resource, AnyFilter));

	FSRPatternRoutingFilter ExactFilter;
	ExactFilter.ResourceId = Resource.ResourceId;
	ExactFilter.MatchMode = ESRPatternRoutingMatchMode::ExactPattern;
	ExactFilter.RequiredPattern = Pattern;
	TestTrue(
		TEXT("An exact filter accepts all 25 matching cells."),
		StarRovers::PatternRouting::MatchesRoutingFilter(Resource, ExactFilter));
	FSRResourceInstance Different = Resource;
	Different.Pattern.SetGlyph(0, 0, ESRGlyphType::Organic);
	TestFalse(
		TEXT("An exact filter rejects a difference outside any special target area."),
		StarRovers::PatternRouting::MatchesRoutingFilter(Different, ExactFilter));

	FSRPatternRoutingFilter MaskedFilter;
	MaskedFilter.MatchMode = ESRPatternRoutingMatchMode::MaskedPattern;
	MaskedFilter.RequiredPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	MaskedFilter.RequiredMask.SetCellActive(2, 2, true);
	MaskedFilter.RequiredMask.SetCellActive(0, 0, true);
	TestTrue(
		TEXT("A masked filter can require both a glyph and an explicitly empty cell."),
		StarRovers::PatternRouting::MatchesRoutingFilter(Resource, MaskedFilter));
	TestFalse(
		TEXT("The same masked filter rejects a glyph in its required empty cell."),
		StarRovers::PatternRouting::MatchesRoutingFilter(Different, MaskedFilter));

	FSRPatternRoutingFilter InvalidMaskedFilter;
	InvalidMaskedFilter.MatchMode = ESRPatternRoutingMatchMode::MaskedPattern;
	TestFalse(TEXT("A masked filter with no active cells is invalid."), InvalidMaskedFilter.IsCanonical());
	TestFalse(
		TEXT("Invalid filters reject cargo rather than broadening silently."),
		StarRovers::PatternRouting::MatchesRoutingFilter(Resource, InvalidMaskedFilter));
	return true;
}

#endif
