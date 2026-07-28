#pragma once

#include "CoreMinimal.h"

/** Result of fitting a fixed design-size panel inside a logical viewport safe area. */
struct STARROVERS_API FSRUIConstrainedPanelLayout
{
	FVector2D AvailableSize = FVector2D::ZeroVector;
	FVector2D ResolvedSize = FVector2D::ZeroVector;
	float Scale = 1.0f;
	bool bConstrained = false;
	bool bBelowReadableScale = false;
};

/** Responsive top-center lane that avoids the persistent left and right HUD panels. */
struct STARROVERS_API FSRUITopCenterLaneLayout
{
	FMargin Insets = FMargin(0.0f);
	FVector2D AvailableSize = FVector2D::ZeroVector;
	FVector2D DesignSize = FVector2D::ZeroVector;
	float Scale = 1.0f;
	bool bCompact = false;
	bool bPreservesSidePanels = false;
	bool bBelowReadableScale = false;
};

/** Bounded window into a potentially very large paged collection. */
struct STARROVERS_API FSRUIPageWindow
{
	int32 FirstPageIndex = 0;
	int32 IndicatorCount = 0;
	int32 SelectedLocalIndex = INDEX_NONE;
	bool bHasLeadingPages = false;
	bool bHasTrailingPages = false;
	FString PageLabel;

	bool ContainsPage(int32 PageIndex) const
	{
		return PageIndex >= FirstPageIndex
			&& PageIndex < FirstPageIndex + IndicatorCount;
	}
};

/** Shared, deterministic rules for viewport fitting and large-list pagination. */
class STARROVERS_API FSRUILayoutPolicy
{
public:
	static constexpr float DefaultSafeMargin = 24.0f;
	static constexpr float DefaultMinimumReadableScale = 0.65f;
	static constexpr int32 DefaultMaximumPageIndicators = 7;
	static constexpr float DefaultLeftCommandLaneInset = 372.0f;
	static constexpr float DefaultRightCommandLaneInset = 408.0f;
	static constexpr float DefaultTopHUDHeightRatio = 0.082f;
	static constexpr float DefaultCommandLaneGap = 12.0f;
	static constexpr float DefaultMinimumCommandLaneWidth = 440.0f;
	static constexpr float DefaultValidationViewportWidth = 1280.0f;
	static constexpr float DefaultValidationViewportHeight = 720.0f;

	static FSRUIConstrainedPanelLayout ResolveConstrainedPanel(
		const FVector2D& DesiredSize,
		const FVector2D& LogicalViewportSize,
		float SafeMargin = DefaultSafeMargin,
		float MinimumReadableScale = DefaultMinimumReadableScale);

	static FSRUITopCenterLaneLayout ResolveTopCenterLane(
		const FVector2D& DesiredSize,
		const FVector2D& LogicalViewportSize,
		float LeftInset = DefaultLeftCommandLaneInset,
		float RightInset = DefaultRightCommandLaneInset,
		float MinimumLaneWidth = DefaultMinimumCommandLaneWidth,
		float SafeMargin = DefaultSafeMargin,
		float MinimumReadableScale = DefaultMinimumReadableScale);

	static FSRUIPageWindow ResolvePageWindow(
		int32 PageCount,
		int32 SelectedPageIndex,
		int32 MaximumIndicatorCount = DefaultMaximumPageIndicators);
};
