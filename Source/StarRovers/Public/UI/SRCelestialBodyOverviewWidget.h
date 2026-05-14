#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SRCelestialBodyOverviewWidget.generated.h"

class AActor;
class SWidget;
class UBorder;
class UButton;
class UCanvasPanel;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class USRCelestialBodyOverviewWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FSRStarRoversCelestialBodyRequestedSignature, AActor*);

struct FSRNameplateButtonLayout
{
	TWeakObjectPtr<AActor> CelestialBodyActor;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D LabelPosition = FVector2D::ZeroVector;
	FVector2D LabelAlignment = FVector2D(0.5f, 1.0f);
	FVector2D LeaderStart = FVector2D::ZeroVector;
	FVector2D LeaderEnd = FVector2D::ZeroVector;
	float OutlineRadius = 0.0f;
	bool bIsVisible = false;
};

UCLASS()
class STARROVERS_API USRCelestialBodyOverviewEntryAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRCelestialBodyOverviewWidget* InOwnerWidget, AActor* InCelestialBodyActor);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<USRCelestialBodyOverviewWidget> OwnerWidget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CelestialBodyActor;
};

UCLASS(Blueprintable)
class STARROVERS_API USRCelestialBodyOverviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	void SetCelestialBodies(const TArray<AActor*>& NewCelestialBodies);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	void SetSelectedActor(AActor* NewSelectedActor);

	void DispatchEntryClicked(AActor* CelestialBodyActor);
	FSRStarRoversCelestialBodyRequestedSignature& OnCelestialBodyRequested();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial")
	FText StarSystemText = NSLOCTEXT("StarRoversOverview", "StarSystemText", "System Objects");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial", meta = (ClampMin = "0.0"))
	float StarSystemNameplateIndentPixels = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial")
	FLinearColor OverviewBorderColor = FLinearColor(0.015f, 0.025f, 0.04f, 0.88f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial")
	FLinearColor StarSystemScrollBoxButtonColor = FLinearColor(0.08f, 0.11f, 0.15f, 0.82f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial")
	FLinearColor SelectedStarSystemScrollBoxButtonColor = FLinearColor(0.18f, 0.36f, 0.42f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial")
	FLinearColor StarSystemNameplateTextColor = FLinearColor(0.86f, 0.92f, 0.97f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates")
	bool bShowNameplateButtons = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates")
	FLinearColor NameplateButtonColor = FLinearColor(0.02f, 0.04f, 0.06f, 0.78f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates")
	FLinearColor SelectedNameplateButtonColor = FLinearColor(0.20f, 0.48f, 0.56f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (ClampMin = "1.0"))
	float NameplateOutlineMinRadiusPixels = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (ClampMin = "1.0"))
	float NameplateOutlineMaxRadiusPixels = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (ClampMin = "0.0"))
	float NameplateOutlinePaddingPixels = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (ClampMin = "0.0"))
	float NameplateLeaderLengthPixels = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (ClampMin = "3"))
	int32 NameplateOutlineSegments = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (ClampMin = "0.0"))
	float NameplateOutlineLineThickness = 2.0f;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> OverviewCanvasPanel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OverviewBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> OverviewVerticalBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StarSystemTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> StarSystemScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NameplateToggleButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameplateToggleButtonTextBlock;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CelestialBodies;

	UPROPERTY(Transient)
	TObjectPtr<AActor> SelectedActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRCelestialBodyOverviewEntryAction>> EntryActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> NameplateActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> NameplateButtons;

	TArray<FSRNameplateButtonLayout> NameplateButtonLayouts;

private:
	UFUNCTION()
	void HandleNameplateToggleClicked();

	void BuildOverviewWidgetTree();
	void RebuildStarSystemScrollBox();
	void RebuildNameplateButtons();
	void RefreshNameplateButtonLayout();
	bool BuildNameplateButtonLayoutForActor(AActor* CelestialBodyActor, int32 NameplateButtonIndex, FSRNameplateButtonLayout& OutLayout) const;
	void AddStarSystemScrollBoxButton(AActor* CelestialBodyActor, int32 Depth, const TMap<AActor*, TArray<AActor*>>& ChildrenByParent);
	FText GetStarSystemNameplateText(AActor* CelestialBodyActor) const;
	FText GetStarSystemNameplatePrefixText(AActor* CelestialBodyActor) const;
	void SortStarSystemBodies(TArray<TObjectPtr<AActor>>& StarSystemBodiesToSort) const;

	FSRStarRoversCelestialBodyRequestedSignature CelestialBodyRequestedEvent;
};
