#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SRCelestialBodyCircleWidget.generated.h"

UCLASS()
class STARROVERS_API USRCelestialBodyCircleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Circle", meta = (DisplayName = "CircleSegments", ClampMin = "3"))
	int32 CircleSegments = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Circle", meta = (DisplayName = "CirclePaddingPixels", ClampMin = "0.0"))
	float CirclePaddingPixels = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Circle", meta = (DisplayName = "CircleLineThickness", ClampMin = "0.0"))
	float CircleLineThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Circle", meta = (DisplayName = "CircleColor"))
	FLinearColor CircleColor = FLinearColor(0.72f, 0.86f, 0.9f, 0.9f);
};
