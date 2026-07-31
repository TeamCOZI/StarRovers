#include "UI/SRStellarContractHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Celestial/SRStar.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Styling/SlateColor.h"
#include "UI/SRPatternGridWidget.h"

namespace
{
	UTextBlock* MakeContractHUDText(
		UWidgetTree* WidgetTree,
		const FName& Name,
		int32 FontSize,
		const FLinearColor& Color)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetJustification(ETextJustify::Center);
		return TextBlock;
	}
}

TSharedRef<SWidget> USRStellarContractHUDWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void USRStellarContractHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
	TimeUntilRefresh = 0.0f;
	RefreshFromPrimaryStar();
}

void USRStellarContractHUDWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	BuildWidgetTree();
	RefreshFromPrimaryStar();
}

void USRStellarContractHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeUntilRefresh -= InDeltaTime;
	if (TimeUntilRefresh <= 0.0f)
	{
		RefreshFromPrimaryStar();
		TimeUntilRefresh = FMath::Max(0.0f, RefreshInterval);
	}
}

void USRStellarContractHUDWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StellarContractHUDCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StellarContractHUDSizeBox"));
	PanelSizeBox->SetWidthOverride(190.0f);
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelSizeBox))
	{
		PanelSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		PanelSlot->SetPosition(FVector2D(-18.0f, 72.0f));
		PanelSlot->SetAutoSize(true);
	}

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StellarContractHUDPanel"));
	PanelBorder->SetBrushColor(FLinearColor(0.018f, 0.026f, 0.038f, 0.92f));
	PanelBorder->SetPadding(FMargin(12.0f));
	PanelBorder->SetHorizontalAlignment(HAlign_Center);
	PanelSizeBox->SetContent(PanelBorder);

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StellarContractHUDContent"));
	PanelBorder->SetContent(ContentBox);

	ContractTitleTextBlock = MakeContractHUDText(
		WidgetTree,
		TEXT("StellarContractHUDTitle"),
		14,
		FLinearColor(0.84f, 0.91f, 1.0f, 1.0f));
	if (ContractTitleTextBlock)
	{
		if (UVerticalBoxSlot* TitleSlot = ContentBox->AddChildToVerticalBox(ContractTitleTextBlock))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	TargetPatternGridWidget = WidgetTree->ConstructWidget<USRPatternGridWidget>(
		USRPatternGridWidget::StaticClass(),
		TEXT("StellarTargetPatternGrid"));
	TargetPatternGridWidget->SetCellSize(24.0f);
	if (UVerticalBoxSlot* PatternSlot = ContentBox->AddChildToVerticalBox(TargetPatternGridWidget))
	{
		PatternSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		PatternSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ScoreTextBlock = MakeContractHUDText(
		WidgetTree,
		TEXT("StellarContractHUDScore"),
		13,
		FLinearColor(0.96f, 0.96f, 0.90f, 1.0f));
	if (ScoreTextBlock)
	{
		if (UVerticalBoxSlot* ScoreSlot = ContentBox->AddChildToVerticalBox(ScoreTextBlock))
		{
			ScoreSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
			ScoreSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	StellarHealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(),
		TEXT("StellarHealthProgressBar"));
	StellarHealthProgressBar->SetPercent(1.0f);
	StellarHealthProgressBar->SetFillColorAndOpacity(FLinearColor(0.20f, 0.78f, 0.42f, 1.0f));
	if (UVerticalBoxSlot* HealthBarSlot = ContentBox->AddChildToVerticalBox(StellarHealthProgressBar))
	{
		HealthBarSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		HealthBarSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	StellarHealthTextBlock = MakeContractHUDText(
		WidgetTree,
		TEXT("StellarHealthText"),
		12,
		FLinearColor(0.80f, 0.86f, 0.90f, 1.0f));
	if (StellarHealthTextBlock)
	{
		if (UVerticalBoxSlot* HealthTextSlot = ContentBox->AddChildToVerticalBox(StellarHealthTextBlock))
		{
			HealthTextSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}
}

void USRStellarContractHUDWidget::RefreshFromPrimaryStar()
{
	if (!PanelBorder || !TargetPatternGridWidget || !ScoreTextBlock || !StellarHealthProgressBar || !StellarHealthTextBlock)
	{
		return;
	}

	const ASRStar* PrimaryStar = ResolvePrimaryStar();
	if (!IsValid(PrimaryStar))
	{
		PanelBorder->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	PanelBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const FSRStellarPatternContract Contract = PrimaryStar->GetStellarPatternContract();
	const FSRStellarContractState State = PrimaryStar->GetStellarContractState();
	TargetPatternGridWidget->SetPatternAndMask(Contract.RequiredPattern, Contract.RequiredMask);

	if (ContractTitleTextBlock)
	{
		ContractTitleTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("TARGET PATTERN · %s"),
			*Contract.ContractId.ToString())));
	}
	ScoreTextBlock->SetText(FText::FromString(FString::Printf(
		TEXT("Cycle %d   Score %lld / %lld"),
		State.ActiveCycleIndex,
		static_cast<long long>(State.CurrentCycleScore),
		static_cast<long long>(State.RequiredScoreThisCycle))));

	const double SafeMaximumHealth = FMath::Max(State.MaximumStellarHealth, UE_DOUBLE_SMALL_NUMBER);
	const float HealthRatio = static_cast<float>(FMath::Clamp(State.CurrentStellarHealth / SafeMaximumHealth, 0.0, 1.0));
	StellarHealthProgressBar->SetPercent(HealthRatio);
	StellarHealthProgressBar->SetFillColorAndOpacity(
		HealthRatio > 0.50f
			? FLinearColor(0.20f, 0.78f, 0.42f, 1.0f)
			: (HealthRatio > 0.25f
				? FLinearColor(0.95f, 0.62f, 0.16f, 1.0f)
				: FLinearColor(0.94f, 0.18f, 0.13f, 1.0f)));
	StellarHealthTextBlock->SetText(FText::FromString(FString::Printf(
		TEXT("STELLAR HEALTH  %.0f / %.0f   (-%.3f/s)"),
		State.CurrentStellarHealth,
		State.MaximumStellarHealth,
		State.CurrentStellarHealthDecreasePerSecond)));
}

ASRStar* USRStellarContractHUDWidget::ResolvePrimaryStar() const
{
	const UWorld* World = GetWorld();
	const USRCelestialBodyRegistrySubsystem* Registry = IsValid(World)
		? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>()
		: nullptr;
	return IsValid(Registry) ? Cast<ASRStar>(Registry->GetPrimaryStarActor()) : nullptr;
}
