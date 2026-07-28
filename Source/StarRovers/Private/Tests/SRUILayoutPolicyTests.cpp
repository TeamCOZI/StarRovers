#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRUILayoutPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRUILayoutConstrainedPanelTest,
	"StarRovers.UI.LayoutPolicy.ConstrainedPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRUILayoutConstrainedPanelTest::RunTest(const FString& Parameters)
{
	const FSRUIConstrainedPanelLayout FullHD = FSRUILayoutPolicy::ResolveConstrainedPanel(
		FVector2D(1160.0f, 680.0f),
		FVector2D(1920.0f, 1080.0f));
	TestFalse(TEXT("A 1160x680 decision panel remains at design size in a 1080p viewport"), FullHD.bConstrained);
	TestEqual(TEXT("An unconstrained panel keeps scale 1"), FullHD.Scale, 1.0f);

	const FSRUIConstrainedPanelLayout HD = FSRUILayoutPolicy::ResolveConstrainedPanel(
		FVector2D(1160.0f, 680.0f),
		FVector2D(1280.0f, 720.0f));
	TestTrue(TEXT("A 720p viewport constrains the decision panel to its safe height"), HD.bConstrained);
	TestTrue(TEXT("The constrained panel stays inside the horizontal safe area"), HD.ResolvedSize.X <= HD.AvailableSize.X + UE_KINDA_SMALL_NUMBER);
	TestTrue(TEXT("The constrained panel stays inside the vertical safe area"), HD.ResolvedSize.Y <= HD.AvailableSize.Y + UE_KINDA_SMALL_NUMBER);
	TestFalse(TEXT("The 720p decision panel remains above the readability floor"), HD.bBelowReadableScale);

	const FSRUIConstrainedPanelLayout Compact = FSRUILayoutPolicy::ResolveConstrainedPanel(
		FVector2D(1160.0f, 680.0f),
		FVector2D(640.0f, 360.0f));
	TestTrue(TEXT("A compact viewport is explicitly reported below the readability floor"), Compact.bBelowReadableScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRUILayoutTopCenterLaneTest,
	"StarRovers.UI.LayoutPolicy.TopCenterCommandLane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRUILayoutTopCenterLaneTest::RunTest(const FString& Parameters)
{
	const FVector2D DesiredBanner(820.0f, 148.0f);
	const FSRUITopCenterLaneLayout FullHD = FSRUILayoutPolicy::ResolveTopCenterLane(
		DesiredBanner,
		FVector2D(1920.0f, 1080.0f));
	TestTrue(TEXT("A 1080p command lane preserves both persistent side panels"),
		FullHD.bPreservesSidePanels);
	TestFalse(TEXT("A 1080p Guidance banner keeps its full information hierarchy"),
		FullHD.bCompact);
	TestEqual(TEXT("A 1080p Guidance banner keeps its authored width"),
		FullHD.DesignSize.X,
		820.0);
	TestTrue(TEXT("The command lane starts below the top survival and cycle rails"),
		FullHD.Insets.Top > 1080.0f * FSRUILayoutPolicy::DefaultTopHUDHeightRatio);

	const FSRUITopCenterLaneLayout HD = FSRUILayoutPolicy::ResolveTopCenterLane(
		DesiredBanner,
		FVector2D(1280.0f, 720.0f));
	TestTrue(TEXT("A 720p command lane still preserves Overview and Operations"),
		HD.bPreservesSidePanels);
	TestTrue(TEXT("A 720p Guidance banner switches to its compact hierarchy"),
		HD.bCompact);
	TestEqual(TEXT("The 720p center lane occupies only the unreserved 500 pixels"),
		HD.DesignSize.X,
		500.0);
	TestTrue(TEXT("The compact banner remains between both side-panel reserves"),
		HD.Insets.Left + HD.DesignSize.X <= 1280.0f - HD.Insets.Right
			+ UE_KINDA_SMALL_NUMBER);
	TestFalse(TEXT("The supported 720p layout remains above the readability floor"),
		HD.bBelowReadableScale);

	const FSRUITopCenterLaneLayout Tiny = FSRUILayoutPolicy::ResolveTopCenterLane(
		DesiredBanner,
		FVector2D(640.0f, 360.0f));
	TestFalse(TEXT("An unsupported tiny viewport reports that side panels cannot all be preserved"),
		Tiny.bPreservesSidePanels);
	TestTrue(TEXT("An unsupported tiny viewport is explicitly marked below the readability contract"),
		Tiny.bBelowReadableScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRUILayoutPageWindowTest,
	"StarRovers.UI.LayoutPolicy.LargePageWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRUILayoutPageWindowTest::RunTest(const FString& Parameters)
{
	const FSRUIPageWindow First = FSRUILayoutPolicy::ResolvePageWindow(200, 0);
	TestEqual(TEXT("A 200-page catalog still creates at most seven indicators"), First.IndicatorCount, 7);
	TestEqual(TEXT("The first-page window starts at page zero"), First.FirstPageIndex, 0);
	TestEqual(TEXT("The first page is selected locally"), First.SelectedLocalIndex, 0);
	TestFalse(TEXT("The first window has no hidden pages before it"), First.bHasLeadingPages);
	TestTrue(TEXT("The first window reports hidden pages after it"), First.bHasTrailingPages);

	const FSRUIPageWindow Middle = FSRUILayoutPolicy::ResolvePageWindow(200, 99);
	TestTrue(TEXT("The moving window always contains the selected middle page"), Middle.ContainsPage(99));
	TestEqual(TEXT("A middle selection remains centered in a seven-indicator window"), Middle.SelectedLocalIndex, 3);
	TestTrue(TEXT("A middle window reports omitted pages on both sides"), Middle.bHasLeadingPages && Middle.bHasTrailingPages);
	TestEqual(TEXT("The page label remains exact for large catalogs"), Middle.PageLabel, FString(TEXT("PAGE 100 / 200")));

	const FSRUIPageWindow Last = FSRUILayoutPolicy::ResolvePageWindow(200, 199);
	TestEqual(TEXT("The final window is clamped against the catalog end"), Last.FirstPageIndex, 193);
	TestEqual(TEXT("The final page maps to the final local indicator"), Last.SelectedLocalIndex, 6);
	TestTrue(TEXT("The final window reports hidden pages before it"), Last.bHasLeadingPages);
	TestFalse(TEXT("The final window has no hidden pages after it"), Last.bHasTrailingPages);
	return true;
}

#endif
