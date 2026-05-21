#include "UI/SRCelestialBodyCircleWidget.h"

#include "Rendering/DrawElements.h"

int32 USRCelestialBodyCircleWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 CircleLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D Center = LocalSize * 0.5f;
	const float Radius = (FMath::Min(LocalSize.X, LocalSize.Y) * 0.5f)
		- FMath::Max(0.0f, CirclePaddingPixels)
		- (FMath::Max(0.0f, CircleLineThickness) * 0.5f);
	if (Radius <= KINDA_SMALL_NUMBER || CircleLineThickness <= KINDA_SMALL_NUMBER)
	{
		return CircleLayerId;
	}

	TArray<FVector2D> CirclePoints;
	const int32 SafeSegmentCount = FMath::Max(3, CircleSegments);
	CirclePoints.Reserve(SafeSegmentCount + 1);
	for (int32 SegmentIndex = 0; SegmentIndex <= SafeSegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SafeSegmentCount);
		const float AngleRadians = Alpha * UE_TWO_PI;
		CirclePoints.Add(Center + FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians)) * Radius);
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		CircleLayerId,
		AllottedGeometry.ToPaintGeometry(),
		CirclePoints,
		ESlateDrawEffect::None,
		CircleColor,
		true,
		CircleLineThickness);

	return CircleLayerId + 1;
}
