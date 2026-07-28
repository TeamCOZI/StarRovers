#include "UI/SRUILayoutPolicy.h"

FSRUIConstrainedPanelLayout FSRUILayoutPolicy::ResolveConstrainedPanel(
	const FVector2D& DesiredSize,
	const FVector2D& LogicalViewportSize,
	float SafeMargin,
	float MinimumReadableScale)
{
	FSRUIConstrainedPanelLayout Layout;
	const FVector2D SafeDesiredSize(
		FMath::Max(0.0f, DesiredSize.X),
		FMath::Max(0.0f, DesiredSize.Y));
	const FVector2D SafeViewportSize(
		FMath::Max(0.0f, LogicalViewportSize.X),
		FMath::Max(0.0f, LogicalViewportSize.Y));
	const float ClampedSafeMargin = FMath::Max(0.0f, SafeMargin);
	Layout.AvailableSize = FVector2D(
		FMath::Max(0.0f, SafeViewportSize.X - ClampedSafeMargin * 2.0f),
		FMath::Max(0.0f, SafeViewportSize.Y - ClampedSafeMargin * 2.0f));

	float ResolvedScale = 1.0f;
	if (SafeDesiredSize.X > UE_SMALL_NUMBER)
	{
		ResolvedScale = FMath::Min(ResolvedScale, Layout.AvailableSize.X / SafeDesiredSize.X);
	}
	if (SafeDesiredSize.Y > UE_SMALL_NUMBER)
	{
		ResolvedScale = FMath::Min(ResolvedScale, Layout.AvailableSize.Y / SafeDesiredSize.Y);
	}
	Layout.Scale = FMath::Clamp(ResolvedScale, 0.0f, 1.0f);
	Layout.ResolvedSize = SafeDesiredSize * Layout.Scale;
	Layout.bConstrained = Layout.Scale < 1.0f - UE_KINDA_SMALL_NUMBER;
	Layout.bBelowReadableScale = Layout.Scale < FMath::Clamp(MinimumReadableScale, 0.0f, 1.0f);
	return Layout;
}

FSRUITopCenterLaneLayout FSRUILayoutPolicy::ResolveTopCenterLane(
	const FVector2D& DesiredSize,
	const FVector2D& LogicalViewportSize,
	float LeftInset,
	float RightInset,
	float MinimumLaneWidth,
	float SafeMargin,
	float MinimumReadableScale)
{
	FSRUITopCenterLaneLayout Layout;
	const FVector2D ViewportSize(
		FMath::Max(0.0f, LogicalViewportSize.X),
		FMath::Max(0.0f, LogicalViewportSize.Y));
	const FVector2D SafeDesiredSize(
		FMath::Max(0.0f, DesiredSize.X),
		FMath::Max(0.0f, DesiredSize.Y));
	const float ClampedSafeMargin = FMath::Max(0.0f, SafeMargin);
	const float ReservedLeft = FMath::Max(ClampedSafeMargin, LeftInset);
	const float ReservedRight = FMath::Max(ClampedSafeMargin, RightInset);
	const float SidePanelLaneWidth = FMath::Max(
		0.0f,
		ViewportSize.X - ReservedLeft - ReservedRight);
	Layout.bPreservesSidePanels = SidePanelLaneWidth
		>= FMath::Max(0.0f, MinimumLaneWidth);

	const float ResolvedLeft = Layout.bPreservesSidePanels
		? ReservedLeft
		: ClampedSafeMargin;
	const float ResolvedRight = Layout.bPreservesSidePanels
		? ReservedRight
		: ClampedSafeMargin;
	const float TopHUDHeight = ViewportSize.Y * DefaultTopHUDHeightRatio;
	const float ResolvedTop = FMath::Max(
		ClampedSafeMargin,
		TopHUDHeight + DefaultCommandLaneGap);
	Layout.Insets = FMargin(
		ResolvedLeft,
		ResolvedTop,
		ResolvedRight,
		ClampedSafeMargin);
	Layout.AvailableSize = FVector2D(
		FMath::Max(0.0f, ViewportSize.X - ResolvedLeft - ResolvedRight),
		FMath::Max(0.0f, ViewportSize.Y - ResolvedTop - ClampedSafeMargin));

	Layout.DesignSize = FVector2D(
		FMath::Min(SafeDesiredSize.X, Layout.AvailableSize.X),
		SafeDesiredSize.Y);
	float ResolvedScale = 1.0f;
	if (Layout.DesignSize.X > UE_SMALL_NUMBER)
	{
		ResolvedScale = FMath::Min(
			ResolvedScale,
			Layout.AvailableSize.X / Layout.DesignSize.X);
	}
	if (Layout.DesignSize.Y > UE_SMALL_NUMBER)
	{
		ResolvedScale = FMath::Min(
			ResolvedScale,
			Layout.AvailableSize.Y / Layout.DesignSize.Y);
	}
	Layout.Scale = FMath::Clamp(ResolvedScale, 0.0f, 1.0f);
	Layout.bCompact = Layout.DesignSize.X
		< SafeDesiredSize.X - UE_KINDA_SMALL_NUMBER;
	Layout.bBelowReadableScale = !Layout.bPreservesSidePanels
		|| Layout.Scale < FMath::Clamp(MinimumReadableScale, 0.0f, 1.0f);
	return Layout;
}

FSRUIPageWindow FSRUILayoutPolicy::ResolvePageWindow(
	int32 PageCount,
	int32 SelectedPageIndex,
	int32 MaximumIndicatorCount)
{
	FSRUIPageWindow Window;
	if (PageCount <= 0 || MaximumIndicatorCount <= 0)
	{
		return Window;
	}

	const int32 ClampedSelectedPage = FMath::Clamp(SelectedPageIndex, 0, PageCount - 1);
	Window.IndicatorCount = FMath::Min(PageCount, MaximumIndicatorCount);
	const int32 MaximumFirstPageIndex = PageCount - Window.IndicatorCount;
	Window.FirstPageIndex = FMath::Clamp(
		ClampedSelectedPage - Window.IndicatorCount / 2,
		0,
		MaximumFirstPageIndex);
	Window.SelectedLocalIndex = ClampedSelectedPage - Window.FirstPageIndex;
	Window.bHasLeadingPages = Window.FirstPageIndex > 0;
	Window.bHasTrailingPages = Window.FirstPageIndex + Window.IndicatorCount < PageCount;
	Window.PageLabel = FString::Printf(TEXT("PAGE %d / %d"), ClampedSelectedPage + 1, PageCount);
	return Window;
}
