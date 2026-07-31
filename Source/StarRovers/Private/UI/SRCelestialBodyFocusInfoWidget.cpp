#include "UI/SRCelestialBodyFocusInfoWidget.h"

#include "UI/SRCelestialBodyFocusUIInternal.h"
#include "Utility/SRLog.h"
#include "Blueprint/WidgetTree.h"
#include "Celestial/SRStar.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateColor.h"
#include "UI/SRPatternGridWidget.h"

namespace
{
	FIntPoint GetSurfaceGridDisplayCellCoord(const FSRPlanetSurfaceGridCellId& CellId, int32 FaceResolution)
	{
		FIntPoint DisplayCoord(CellId.CellX, CellId.CellY);
		if (CellId.Face == ESRCubeSphereFace::PositiveZ || CellId.Face == ESRCubeSphereFace::NegativeZ)
		{
			const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
			DisplayCoord.X = SafeFaceResolution - 1 - CellId.CellX;
			DisplayCoord.Y = SafeFaceResolution - 1 - CellId.CellY;
		}
		return DisplayCoord;
	}

	const TCHAR* GetStructureBuildKindLabel(ESRStructureBuildKind BuildKind)
	{
		switch (BuildKind)
		{
		case ESRStructureBuildKind::Structure:
			return TEXT("Structure");
		case ESRStructureBuildKind::Conveyor:
			return TEXT("Conveyor");
		default:
			return TEXT("Unknown");
		}
	}

	FString BuildPortDirectionLabel(bool bHasInput, bool bHasOutput)
	{
		if (bHasInput && bHasOutput)
		{
			return TEXT("Input, Output");
		}
		if (bHasInput)
		{
			return TEXT("Input");
		}
		if (bHasOutput)
		{
			return TEXT("Output");
		}
		return TEXT("None");
	}

	FString BuildFacilityPortDirectionSummary(const TArray<FSRFocusedFacilityPortInfo>& FacilityPorts)
	{
		bool bTopInput = false;
		bool bTopOutput = false;
		bool bBottomInput = false;
		bool bBottomOutput = false;
		bool bLeftInput = false;
		bool bLeftOutput = false;
		bool bRightInput = false;
		bool bRightOutput = false;

		auto MarkDirection = [](ESRStructurePortKind PortKind, bool& bHasInput, bool& bHasOutput)
		{
			if (PortKind == ESRStructurePortKind::Output)
			{
				bHasOutput = true;
			}
			else
			{
				bHasInput = true;
			}
		};

		for (const FSRFocusedFacilityPortInfo& PortInfo : FacilityPorts)
		{
			switch (PortInfo.Direction)
			{
			case ESRStructurePortDirection::Left:
				MarkDirection(PortInfo.PortKind, bLeftInput, bLeftOutput);
				break;
			case ESRStructurePortDirection::Right:
				MarkDirection(PortInfo.PortKind, bRightInput, bRightOutput);
				break;
			case ESRStructurePortDirection::Top:
				MarkDirection(PortInfo.PortKind, bTopInput, bTopOutput);
				break;
			case ESRStructurePortDirection::Bottom:
				MarkDirection(PortInfo.PortKind, bBottomInput, bBottomOutput);
				break;
			default:
				break;
			}
		}

		return FString::Printf(
			TEXT("Ports\nTop: %s\nBottom: %s\nLeft: %s\nRight: %s"),
			*BuildPortDirectionLabel(bTopInput, bTopOutput),
			*BuildPortDirectionLabel(bBottomInput, bBottomOutput),
			*BuildPortDirectionLabel(bLeftInput, bLeftOutput),
			*BuildPortDirectionLabel(bRightInput, bRightOutput));
	}

	const TCHAR* GetFocusedFacilityTemperatureLabel(ESRFacilityTemperatureState TemperatureState)
	{
		switch (TemperatureState)
		{
		case ESRFacilityTemperatureState::Frozen:
			return TEXT("Frozen");
		case ESRFacilityTemperatureState::Cold:
			return TEXT("Cold");
		case ESRFacilityTemperatureState::Normal:
			return TEXT("Normal");
		case ESRFacilityTemperatureState::Hot:
			return TEXT("Hot");
		case ESRFacilityTemperatureState::Overheated:
			return TEXT("Overheated");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetStellarEvolutionStageLabel(ESRStellarEvolutionStage EvolutionStage)
	{
		switch (EvolutionStage)
		{
		case ESRStellarEvolutionStage::MainSequence:
			return TEXT("Main Sequence");
		case ESRStellarEvolutionStage::RedGiant:
			return TEXT("Red Giant");
		case ESRStellarEvolutionStage::Supernova:
			return TEXT("Supernova");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetPatternHandRarityLabel(ESRPatternHandRarity Rarity)
	{
		switch (Rarity)
		{
		case ESRPatternHandRarity::Common: return TEXT("Common");
		case ESRPatternHandRarity::Uncommon: return TEXT("Uncommon");
		case ESRPatternHandRarity::Rare: return TEXT("Rare");
		case ESRPatternHandRarity::Epic: return TEXT("Epic");
		case ESRPatternHandRarity::Legendary: return TEXT("Legendary");
		default: return TEXT("Unknown");
		}
	}

	FString BuildFocusedStellarContractSummary(const FSRFocusedStellarContractInfo& ContractInfo)
	{
		const FSRStellarPatternContract& Contract = ContractInfo.Contract;
		const FSRStellarContractState& State = ContractInfo.State;
		FString Summary = FString::Printf(
			TEXT("Stellar Pattern Contract\nStage: %s\nContract: %s\nCycle: %d\nScore: %lld / %lld (Base %lld + Bonus %lld)\nPatterns: %lld accepted, %lld rejected\nStellar Health: %.2f / %.2f\nDemand Mask: %d / 25 cells\nBase per Pattern: %d\nRequirement Growth: +%d / Cycle\nGame Over: %s"),
			GetStellarEvolutionStageLabel(State.EvolutionStage),
			*Contract.ContractId.ToString(),
			State.ActiveCycleIndex,
			State.CurrentCycleScore,
			State.RequiredScoreThisCycle,
			State.CurrentCycleBaseScore,
			State.CurrentCycleBonusScore,
			State.SubmittedPatternCount,
			State.RejectedPatternCount,
			State.CurrentStellarHealth,
			State.MaximumStellarHealth,
			Contract.RequiredMask.GetActiveCellCount(),
			Contract.BaseScorePerPattern,
			Contract.RequiredScoreGrowthPerCycle,
			State.bSupernovaGameOver ? TEXT("Yes") : TEXT("No"));

		if (State.LastSubmission.bPatternValid)
		{
			Summary += FString::Printf(
				TEXT("\n\nLast Submission\nDemand: %s\nScore: %lld (Base %d + Bonus %lld)"),
				State.LastSubmission.bMatchesDemand ? TEXT("Matched") : TEXT("Rejected"),
				State.LastSubmission.TotalScore,
				State.LastSubmission.BaseScorePerPattern,
				State.LastSubmission.BonusScorePerPattern);
			for (const FSRPatternHandMatch& Match : State.LastSubmission.HandMatches)
			{
				Summary += FString::Printf(
					TEXT("\n- %s [%s] +%d"),
					Match.DisplayName.IsEmpty() ? *Match.RuleId.ToString() : *Match.DisplayName.ToString(),
					GetPatternHandRarityLabel(Match.Rarity),
					Match.BonusScore);
			}
		}

		if (!Contract.BonusRules.IsEmpty())
		{
			Summary += TEXT("\n\nAvailable Bonus Hands");
			for (const FSRPatternHandRule& Rule : Contract.BonusRules)
			{
				Summary += FString::Printf(
					TEXT("\n- %s [%s] +%d"),
					Rule.DisplayName.IsEmpty() ? *Rule.RuleId.ToString() : *Rule.DisplayName.ToString(),
					GetPatternHandRarityLabel(Rule.Rarity),
					Rule.BonusScore);
			}
		}
		return Summary;
	}

	bool AreFocusedStellarContractInfosEqual(
		const FSRFocusedStellarContractInfo& Left,
		const FSRFocusedStellarContractInfo& Right)
	{
		return Left.bIsValid == Right.bIsValid
			&& Left.Contract.ContractId == Right.Contract.ContractId
			&& Left.Contract.RequiredPattern == Right.Contract.RequiredPattern
			&& Left.Contract.RequiredMask == Right.Contract.RequiredMask
			&& Left.Contract.BonusRules.Num() == Right.Contract.BonusRules.Num()
			&& Left.State.EvolutionStage == Right.State.EvolutionStage
			&& Left.State.ActiveCycleIndex == Right.State.ActiveCycleIndex
			&& Left.State.RequiredScoreThisCycle == Right.State.RequiredScoreThisCycle
			&& Left.State.CurrentCycleScore == Right.State.CurrentCycleScore
			&& Left.State.CurrentCycleBaseScore == Right.State.CurrentCycleBaseScore
			&& Left.State.CurrentCycleBonusScore == Right.State.CurrentCycleBonusScore
			&& Left.State.SubmittedPatternCount == Right.State.SubmittedPatternCount
			&& Left.State.RejectedPatternCount == Right.State.RejectedPatternCount
			&& FMath::IsNearlyEqual(Left.State.CurrentStellarHealth, Right.State.CurrentStellarHealth)
			&& Left.State.LastSubmission.TotalScore == Right.State.LastSubmission.TotalScore
			&& Left.State.LastSubmission.HandMatches.Num() == Right.State.LastSubmission.HandMatches.Num()
			&& Left.State.LastCycleSettlement.CycleIndex == Right.State.LastCycleSettlement.CycleIndex
			&& Left.State.bSupernovaGameOver == Right.State.bSupernovaGameOver;
	}

	constexpr float FocusDetailsBoxWidth = 360.0f;
	constexpr float FocusDetailsBoxHeight = 260.0f;
}

TSharedRef<SWidget> USRCelestialBodyFocusInfoWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildFocusInfoWidgetTree();
	return Super::RebuildWidget();
}

void USRCelestialBodyFocusInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFocusInfoWidgetTree();
	BindAssemblyModeButtonHandler();
	RefreshFocusInfoText();
}

void USRCelestialBodyFocusInfoWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildFocusInfoWidgetTree();
	BindAssemblyModeButtonHandler();
	RefreshFocusInfoText();
}

void USRCelestialBodyFocusInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible())
	{
		return;
	}

	if (RefreshStellarContractInfoFromFocusedActor())
	{
		RefreshFocusInfoText();
	}
}

FReply USRCelestialBodyFocusInfoWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverFocusInfoUI(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FocusInfo NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRCelestialBodyFocusInfoWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverFocusInfoUI(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FocusInfo NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRCelestialBodyFocusInfoWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverFocusInfoUI(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRCelestialBodyFocusInfoWidget::SetFocusInfo(const FSRCelestialBodyFocusInfo& NewFocusInfo)
{
	FocusInfo = NewFocusInfo;
	bHasFocusInfo = NewFocusInfo.bIsValid;
	RefreshFocusInfoText();
	OnFocusInfoChanged(FocusInfo);
}

void USRCelestialBodyFocusInfoWidget::ClearFocusInfo()
{
	FocusInfo = FSRCelestialBodyFocusInfo();
	bHasFocusInfo = false;
	RefreshFocusInfoText();
	OnFocusCleared();
}

bool USRCelestialBodyFocusInfoWidget::HasFocusInfo() const
{
	return bHasFocusInfo;
}

FSRCelestialBodyFocusInfo USRCelestialBodyFocusInfoWidget::GetFocusInfo() const
{
	return FocusInfo;
}

void USRCelestialBodyFocusInfoWidget::SetAssemblyModeActive(bool bNewAssemblyModeActive)
{
	bAssemblyModeActive = bNewAssemblyModeActive;
	RefreshFocusInfoText();
	RefreshAssemblyModeButton();
}

bool USRCelestialBodyFocusInfoWidget::IsAssemblyModeActive() const
{
	return bAssemblyModeActive;
}

bool USRCelestialBodyFocusInfoWidget::IsPointerOverFocusInfoUI() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverFocusInfoUI(FSlateApplication::Get().GetCursorPos());
}

FSRStarRoversAssemblyModeRequestedSignature& USRCelestialBodyFocusInfoWidget::OnAssemblyModeRequested()
{
	return AssemblyModeRequestedEvent;
}

void USRCelestialBodyFocusInfoWidget::BuildFocusInfoWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		EnsureHoveredCellTextBlock(WidgetTree->RootWidget);
		EnsureAssemblyModeButton(WidgetTree->RootWidget);
		BindAssemblyModeButtonHandler();
		RefreshFocusInfoText();
		RefreshAssemblyModeButton();
		return;
	}

	UCanvasPanel* FocusInfoCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FocusInfoCanvasPanel"));
	WidgetTree->RootWidget = FocusInfoCanvasPanel;

	FocusInfoBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FocusInfoBorder"));
	FocusInfoBorder->SetPadding(FMargin(16.0f));
	FocusInfoBorder->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.92f));

	if (UCanvasPanelSlot* FocusInfoBorderSlot = FocusInfoCanvasPanel->AddChildToCanvas(FocusInfoBorder))
	{
		FocusInfoBorderSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
		FocusInfoBorderSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		FocusInfoBorderSlot->SetPosition(FVector2D(-32.0f, 32.0f));
		FocusInfoBorderSlot->SetAutoSize(true);
	}

	UVerticalBox* FocusInfoVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FocusInfoVerticalBox"));
	FocusInfoBorder->SetContent(FocusInfoVerticalBox);

	VariableNameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VariableNameTextBlock"));

	if (VariableNameTextBlock)
	{
		FSlateFontInfo VariableNameFont = VariableNameTextBlock->GetFont();
		VariableNameFont.Size = 20;
		VariableNameTextBlock->SetFont(VariableNameFont);
		VariableNameTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		VariableNameTextBlock->SetAutoWrapText(true);
		if (UVerticalBoxSlot* VariableNameTextBlockSlot = FocusInfoVerticalBox->AddChildToVerticalBox(VariableNameTextBlock))
		{
			VariableNameTextBlockSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	}

	EnsureHoveredCellTextBlock(FocusInfoVerticalBox);

	EnsureAssemblyModeButton(FocusInfoVerticalBox);
	BindAssemblyModeButtonHandler();
	RefreshFocusInfoText();
	RefreshAssemblyModeButton();
}

void USRCelestialBodyFocusInfoWidget::EnsureHoveredCellTextBlock(UWidget* HoveredCellTextBlockParent)
{
	if (!WidgetTree)
	{
		return;
	}

	if (!HoveredCellTextBlock)
	{
		HoveredCellTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("HoveredCellTextBlock"))));
	}

	if (!HoveredCellContainer)
	{
		HoveredCellContainer = Cast<USizeBox>(WidgetTree->FindWidget(FName(TEXT("HoveredCellContainer"))));
	}

	if (!HoveredCellScrollBox)
	{
		HoveredCellScrollBox = Cast<UScrollBox>(WidgetTree->FindWidget(FName(TEXT("HoveredCellScrollBox"))));
	}

	if (!HoveredCellContainer)
	{
		HoveredCellContainer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HoveredCellContainer"));
	}

	if (!HoveredCellScrollBox)
	{
		HoveredCellScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("HoveredCellScrollBox"));
	}

	if (!HoveredCellTextBlock)
	{
		HoveredCellTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HoveredCellTextBlock"));
	}

	if (!StellarDemandPatternGrid)
	{
		StellarDemandPatternGrid = Cast<USRPatternGridWidget>(WidgetTree->FindWidget(FName(TEXT("StellarDemandPatternGrid"))));
	}
	if (!StellarDemandPatternGrid)
	{
		StellarDemandPatternGrid = WidgetTree->ConstructWidget<USRPatternGridWidget>(
			USRPatternGridWidget::StaticClass(),
			TEXT("StellarDemandPatternGrid"));
		StellarDemandPatternGrid->SetCellSize(28.0f);
	}

	if (!HoveredCellTextBlock || !HoveredCellContainer || !HoveredCellScrollBox)
	{
		return;
	}

	HoveredCellContainer->SetWidthOverride(FocusDetailsBoxWidth);
	HoveredCellContainer->SetHeightOverride(FocusDetailsBoxHeight);
	HoveredCellScrollBox->SetOrientation(Orient_Vertical);

	FSlateFontInfo HoveredCellFont = HoveredCellTextBlock->GetFont();
	HoveredCellFont.Size = 13;
	HoveredCellTextBlock->SetFont(HoveredCellFont);
	HoveredCellTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.9f, 1.0f, 1.0f)));
	HoveredCellTextBlock->SetAutoWrapText(true);
	HoveredCellTextBlock->SetWrapTextAt(FocusDetailsBoxWidth - 28.0f);

	if (HoveredCellTextBlock->GetParent() && HoveredCellTextBlock->GetParent() != HoveredCellScrollBox)
	{
		HoveredCellTextBlock->GetParent()->RemoveChild(HoveredCellTextBlock);
	}

	if (HoveredCellScrollBox->GetParent() && HoveredCellScrollBox->GetParent() != HoveredCellContainer)
	{
		HoveredCellScrollBox->GetParent()->RemoveChild(HoveredCellScrollBox);
	}

	if (HoveredCellContainer->GetContent() != HoveredCellScrollBox)
	{
		HoveredCellContainer->SetContent(HoveredCellScrollBox);
	}

	if (HoveredCellTextBlock->GetParent() != HoveredCellScrollBox)
	{
		HoveredCellScrollBox->AddChild(HoveredCellTextBlock);
	}
	if (StellarDemandPatternGrid && StellarDemandPatternGrid->GetParent() != HoveredCellScrollBox)
	{
		if (StellarDemandPatternGrid->GetParent())
		{
			StellarDemandPatternGrid->GetParent()->RemoveChild(StellarDemandPatternGrid);
		}
		HoveredCellScrollBox->AddChild(StellarDemandPatternGrid);
	}

	if (HoveredCellContainer->GetParent())
	{
		return;
	}

	if (UVerticalBox* ParentVerticalBox = Cast<UVerticalBox>(HoveredCellTextBlockParent))
	{
		if (UVerticalBoxSlot* HoveredCellTextBlockSlot = ParentVerticalBox->AddChildToVerticalBox(HoveredCellContainer))
		{
			HoveredCellTextBlockSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 8.0f));
		}
		return;
	}

	if (UCanvasPanel* ParentCanvasPanel = Cast<UCanvasPanel>(HoveredCellTextBlockParent))
	{
		if (UCanvasPanelSlot* HoveredCellTextBlockSlot = ParentCanvasPanel->AddChildToCanvas(HoveredCellContainer))
		{
			HoveredCellTextBlockSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
			HoveredCellTextBlockSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			HoveredCellTextBlockSlot->SetPosition(FVector2D(-32.0f, 128.0f));
			HoveredCellTextBlockSlot->SetAutoSize(true);
		}
		return;
	}

	if (UPanelWidget* ParentPanelWidget = Cast<UPanelWidget>(HoveredCellTextBlockParent))
	{
		ParentPanelWidget->AddChild(HoveredCellContainer);
	}
}

void USRCelestialBodyFocusInfoWidget::EnsureAssemblyModeButton(UWidget* AssemblyModeButtonParent)
{
	if (!WidgetTree)
	{
		return;
	}

	if (!AssemblyModeButton)
	{
		AssemblyModeButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("AssemblyModeButton"))));
	}

	if (!AssemblyModeButtonTextBlock)
	{
		AssemblyModeButtonTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("AssemblyModeButtonTextBlock"))));
	}

	if (!AssemblyModeButton)
	{
		AssemblyModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AssemblyModeButton"));
	}

	if (!AssemblyModeButtonTextBlock)
	{
		AssemblyModeButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AssemblyModeButtonTextBlock"));
	}

	if (AssemblyModeButton)
	{
		AssemblyModeButton->SetBackgroundColor(FLinearColor(0.16f, 0.22f, 0.28f, 0.95f));
		if (AssemblyModeButtonTextBlock && AssemblyModeButtonTextBlock->GetParent() != AssemblyModeButton)
		{
			AssemblyModeButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			AssemblyModeButtonTextBlock->SetJustification(ETextJustify::Center);
			AssemblyModeButton->AddChild(AssemblyModeButtonTextBlock);
		}

		if (AssemblyModeButton->GetParent())
		{
			return;
		}

		if (UVerticalBox* ParentVerticalBox = Cast<UVerticalBox>(AssemblyModeButtonParent))
		{
			if (UVerticalBoxSlot* AssemblyModeButtonSlot = ParentVerticalBox->AddChildToVerticalBox(AssemblyModeButton))
			{
				AssemblyModeButtonSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
			}
			return;
		}

		if (UCanvasPanel* ParentCanvasPanel = Cast<UCanvasPanel>(AssemblyModeButtonParent))
		{
			if (UCanvasPanelSlot* AssemblyModeButtonSlot = ParentCanvasPanel->AddChildToCanvas(AssemblyModeButton))
			{
				AssemblyModeButtonSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
				AssemblyModeButtonSlot->SetAlignment(FVector2D(1.0f, 0.0f));
				AssemblyModeButtonSlot->SetPosition(FVector2D(-32.0f, 220.0f));
				AssemblyModeButtonSlot->SetAutoSize(true);
			}
			return;
		}

		if (UPanelWidget* ParentPanelWidget = Cast<UPanelWidget>(AssemblyModeButtonParent))
		{
			ParentPanelWidget->AddChild(AssemblyModeButton);
		}
	}
}

void USRCelestialBodyFocusInfoWidget::BindAssemblyModeButtonHandler()
{
	if (!AssemblyModeButton && WidgetTree)
	{
		AssemblyModeButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("AssemblyModeButton"))));
	}

	if (!AssemblyModeButtonTextBlock && WidgetTree)
	{
		AssemblyModeButtonTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("AssemblyModeButtonTextBlock"))));
	}

	if (AssemblyModeButton)
	{
		AssemblyModeButton->OnClicked.RemoveAll(this);
		AssemblyModeButton->OnClicked.AddDynamic(this, &USRCelestialBodyFocusInfoWidget::HandleAssemblyModeButtonClicked);
	}
}

bool USRCelestialBodyFocusInfoWidget::RefreshStellarContractInfoFromFocusedActor()
{
	if (!bHasFocusInfo || !FocusInfo.bHasStellarContractInfo || !IsValid(FocusInfo.Actor))
	{
		return false;
	}

	const ASRStar* Star = Cast<ASRStar>(FocusInfo.Actor.Get());
	if (!IsValid(Star))
	{
		return false;
	}

	FSRFocusedStellarContractInfo NewContractInfo;
	NewContractInfo.bIsValid = true;
	NewContractInfo.Contract = Star->GetStellarPatternContract();
	NewContractInfo.State = Star->GetStellarContractState();
	if (AreFocusedStellarContractInfosEqual(FocusInfo.StellarContractInfo, NewContractInfo))
	{
		return false;
	}

	FocusInfo.StellarContractInfo = MoveTemp(NewContractInfo);
	return true;
}

void USRCelestialBodyFocusInfoWidget::RefreshFocusInfoText()
{
	if (VariableNameTextBlock)
	{
		VariableNameTextBlock->SetText(
			bHasFocusInfo
				? FocusInfo.VariableName
				: NSLOCTEXT("StarRoversFocusInfo", "NoSelectionTitle", "No body selected")
		);
	}

	const bool bHasStellarContractDetails = FocusInfo.bHasStellarContractInfo
		&& FocusInfo.StellarContractInfo.bIsValid;
	if (StellarDemandPatternGrid)
	{
		if (bHasFocusInfo && bHasStellarContractDetails)
		{
			StellarDemandPatternGrid->SetPatternAndMask(
				FocusInfo.StellarContractInfo.Contract.RequiredPattern,
				FocusInfo.StellarContractInfo.Contract.RequiredMask);
			StellarDemandPatternGrid->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			StellarDemandPatternGrid->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (HoveredCellTextBlock)
	{
		const bool bHasSelectedNonFacilityStructure = FocusInfo.bHasSelectedSurfaceStructure
			&& !FocusInfo.SelectedSurfaceStructureInfo.bHasFacilityRuntimeInfo;
		const bool bHasSurfaceFocusDetails = bAssemblyModeActive && (FocusInfo.bHasHoveredSurfaceCell || bHasSelectedNonFacilityStructure);
		if (bHasFocusInfo && (bHasSurfaceFocusDetails || bHasStellarContractDetails))
		{
			FString CellText;
			if (bHasStellarContractDetails)
			{
				CellText += BuildFocusedStellarContractSummary(FocusInfo.StellarContractInfo);
			}

			if (bAssemblyModeActive && FocusInfo.bHasHoveredSurfaceCell)
			{
				const FSRPlanetSurfaceGridCellInfo& CellInfo = FocusInfo.HoveredSurfaceCellInfo;
				if (!CellText.IsEmpty())
				{
					CellText += TEXT("\n\n");
				}
				CellText += FString::Printf(
					TEXT("Face: %d\nCell: %d,%d\nDisplay: %d,%d\nLatitude: %.1f deg\nTemperature: %s"),
					StarRovers::UI::CelestialBodyFocus::GetCubeSphereFaceNumber(CellInfo.CellId.Face),
					CellInfo.CellId.CellX,
					CellInfo.CellId.CellY,
					CellInfo.DisplayCellX,
					CellInfo.DisplayCellY,
					CellInfo.LatitudeDegrees,
					GetFocusedFacilityTemperatureLabel(CellInfo.TemperatureState));
				CellText += FString::Printf(TEXT("\nHoverGridCells: %d"), FocusInfo.HoveredSurfaceGridPatchCellIds.Num());
				for (int32 CellIndex = 0; CellIndex < FocusInfo.HoveredSurfaceGridPatchCellIds.Num(); ++CellIndex)
				{
					const FSRPlanetSurfaceGridCellId& PatchCellId = FocusInfo.HoveredSurfaceGridPatchCellIds[CellIndex];
					const FIntPoint PatchDisplayCoord = GetSurfaceGridDisplayCellCoord(PatchCellId, CellInfo.FaceResolution);
					CellText += (CellIndex % 5 == 0) ? TEXT("\n") : TEXT(" ");
					CellText += FString::Printf(
						TEXT("F%d(%d,%d)"),
						StarRovers::UI::CelestialBodyFocus::GetCubeSphereFaceNumber(PatchCellId.Face),
						PatchDisplayCoord.X,
						PatchDisplayCoord.Y);
				}
			}

			if (bAssemblyModeActive
				&& FocusInfo.bHasSelectedSurfaceStructure
				&& !FocusInfo.SelectedSurfaceStructureInfo.bHasFacilityRuntimeInfo)
			{
				const FSRFocusedSurfaceStructureInfo& StructureInfo = FocusInfo.SelectedSurfaceStructureInfo;
				const FSRPlanetSurfaceGridCellInfo& ClickedCellInfo = StructureInfo.ClickedCellInfo;
				if (!CellText.IsEmpty())
				{
					CellText += TEXT("\n\n");
				}
				CellText += FString::Printf(
					TEXT("Selected Structure\nName: %s\nStructureId: %s\nOccupantId: %s\nKind: %s\nOrigin: F%d(%d,%d)\nClicked: F%d(%d,%d)\nClickedTemperature: %s\nFootprintCells: %d\nFacility: %s\nNatural: %s"),
					*StructureInfo.DisplayName.ToString(),
					*StructureInfo.StructureId.ToString(),
					*StructureInfo.OccupantId.ToString(),
					GetStructureBuildKindLabel(StructureInfo.BuildKind),
					StarRovers::UI::CelestialBodyFocus::GetCubeSphereFaceNumber(StructureInfo.OriginCellId.Face),
					StructureInfo.OriginCellId.CellX,
					StructureInfo.OriginCellId.CellY,
					StarRovers::UI::CelestialBodyFocus::GetCubeSphereFaceNumber(ClickedCellInfo.CellId.Face),
					ClickedCellInfo.CellId.CellX,
					ClickedCellInfo.CellId.CellY,
					GetFocusedFacilityTemperatureLabel(ClickedCellInfo.TemperatureState),
					StructureInfo.FootprintCellIds.Num(),
					StructureInfo.bHasFacilityDataAsset ? TEXT("Yes") : TEXT("No"),
					StructureInfo.bNaturalStructure ? TEXT("Yes") : TEXT("No"));
				if (!StructureInfo.Description.IsEmpty())
				{
					CellText += FString::Printf(TEXT("\n%s"), *StructureInfo.Description.ToString());
				}
				if (!StructureInfo.FacilityPorts.IsEmpty())
				{
					CellText += FString::Printf(TEXT("\n%s"), *BuildFacilityPortDirectionSummary(StructureInfo.FacilityPorts));
				}
			}
			HoveredCellTextBlock->SetText(FText::FromString(CellText));
			HoveredCellTextBlock->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			HoveredCellTextBlock->SetText(FText::GetEmpty());
			HoveredCellTextBlock->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	RefreshAssemblyModeButton();
}

void USRCelestialBodyFocusInfoWidget::RefreshAssemblyModeButton()
{
	if (!AssemblyModeButton)
	{
		return;
	}

	AssemblyModeButton->SetVisibility(ESlateVisibility::Collapsed);
	AssemblyModeButton->SetIsEnabled(false);
	AssemblyModeButton->SetBackgroundColor(
		bAssemblyModeActive
			? FLinearColor(0.25f, 0.48f, 0.34f, 0.98f)
			: FLinearColor(0.16f, 0.22f, 0.28f, 0.95f));

	if (AssemblyModeButtonTextBlock)
	{
		AssemblyModeButtonTextBlock->SetText(
			bAssemblyModeActive
				? NSLOCTEXT("StarRoversFocusInfo", "ExitAssemblyModeButtonLabel", "Exit Assembly")
				: NSLOCTEXT("StarRoversFocusInfo", "EnterAssemblyModeButtonLabel", "Assembly Mode"));
	}
}

void USRCelestialBodyFocusInfoWidget::HandleAssemblyModeButtonClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FocusInfo AssemblyModeButton OnClicked"));
	AssemblyModeRequestedEvent.Broadcast();
}

bool USRCelestialBodyFocusInfoWidget::IsScreenPositionOverFocusInfoUI(const FVector2D& ScreenPosition) const
{
	if (!IsVisible())
	{
		return false;
	}

	if (FocusInfoBorder && FocusInfoBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition))
	{
		return true;
	}

	return AssemblyModeButton
		&& AssemblyModeButton->IsVisible()
		&& AssemblyModeButton->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}
