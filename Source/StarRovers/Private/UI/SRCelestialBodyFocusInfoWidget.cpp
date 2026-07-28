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
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateColor.h"

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

	FString BuildFocusedStarFuelSummary(const FSRFocusedStarFuelInfo& FuelInfo)
	{
		if (FuelInfo.bUsesStellarPressureCurveV2)
		{
			const TCHAR* DemandPhaseLabel = TEXT("Grace");
			switch (FuelInfo.DemandPhase)
			{
			case ESRStellarDemandPhaseV2::Expansion:
				DemandPhaseLabel = TEXT("Expansion");
				break;
			case ESRStellarDemandPhaseV2::Plateau:
				DemandPhaseLabel = TEXT("Plateau");
				break;
			case ESRStellarDemandPhaseV2::Grace:
			default:
				break;
			}
			return FString::Printf(
				TEXT("Stellar Pressure V2\nStage: %s\nReserve: %.0f / %.0f\nPressure: %.0f%%\nDemand: %.1f/s -> %.1f/s\nPhase: %s | Cycle %d\nLast Delivery: +%.1f\nReserve Gain: +%.1f | Stabilization: +%.1f\nGameOver: %s"),
				GetStellarEvolutionStageLabel(FuelInfo.EvolutionStage),
				FuelInfo.StoredFuel,
				FuelInfo.InitialStageFuel,
				FuelInfo.FuelPressureRatio * 100.0f,
				FuelInfo.RequiredFuelPerCycle,
				FuelInfo.NextCycleDemandPerSecond,
				DemandPhaseLabel,
				FuelInfo.LastFuelDecreaseRateCycleIndex,
				FuelInfo.LastFuelDeliveryAmount,
				FuelInfo.LastFuelReserveGain,
				FuelInfo.LastFuelReserveOverflow,
				FuelInfo.bSupernovaGameOver ? TEXT("Yes") : TEXT("No"));
		}
		return FString::Printf(
			TEXT("Stellar Evolution\nStage: %s\nFuel: %.2f / %.2f\nCurrent Decrease/sec: %.2f\nInitial Decrease/sec: %.2f\nNext Multiplier: %.2fx\nRateCycleIndex: %d\nLastSecond: %s\nLastSecondIndex: %d\nLast Decrease: %.2f\nConsumed: %.2f\nOverkill: %.2f\nGameOver: %s"),
			GetStellarEvolutionStageLabel(FuelInfo.EvolutionStage),
			FuelInfo.StoredFuel,
			FuelInfo.InitialStageFuel,
			FuelInfo.RequiredFuelPerCycle,
			FuelInfo.InitialFuelDecreasePerSecond,
			FuelInfo.RequirementGrowthPerCycle,
			FuelInfo.LastFuelDecreaseRateCycleIndex,
			FuelInfo.bLastSecondSurvived ? TEXT("Survived") : TEXT("Depleted"),
			FuelInfo.LastSettledSecondIndex,
			FuelInfo.LastSecondFuelDecrease,
			FuelInfo.LastSecondFuelConsumed,
			FuelInfo.LastSecondFuelDeficit,
			FuelInfo.bSupernovaGameOver ? TEXT("Yes") : TEXT("No"));
	}

	bool AreFocusedStarFuelInfosEqual(const FSRFocusedStarFuelInfo& Left, const FSRFocusedStarFuelInfo& Right)
	{
		return Left.bIsValid == Right.bIsValid
			&& Left.EvolutionStage == Right.EvolutionStage
			&& FMath::IsNearlyEqual(Left.StoredFuel, Right.StoredFuel)
			&& FMath::IsNearlyEqual(Left.InitialStageFuel, Right.InitialStageFuel)
			&& FMath::IsNearlyEqual(Left.InitialFuelDecreasePerSecond, Right.InitialFuelDecreasePerSecond)
			&& FMath::IsNearlyEqual(Left.RequiredFuelPerCycle, Right.RequiredFuelPerCycle)
			&& FMath::IsNearlyEqual(Left.RequirementGrowthPerCycle, Right.RequirementGrowthPerCycle)
			&& Left.bUsesStellarPressureCurveV2 == Right.bUsesStellarPressureCurveV2
			&& Left.DemandPhase == Right.DemandPhase
			&& FMath::IsNearlyEqual(Left.NextCycleDemandPerSecond, Right.NextCycleDemandPerSecond)
			&& FMath::IsNearlyEqual(Left.FuelPressureRatio, Right.FuelPressureRatio)
			&& FMath::IsNearlyEqual(Left.LastFuelDeliveryAmount, Right.LastFuelDeliveryAmount)
			&& FMath::IsNearlyEqual(Left.LastFuelReserveGain, Right.LastFuelReserveGain)
			&& FMath::IsNearlyEqual(Left.LastFuelReserveOverflow, Right.LastFuelReserveOverflow)
			&& Left.LastFuelDecreaseRateCycleIndex == Right.LastFuelDecreaseRateCycleIndex
			&& FMath::IsNearlyEqual(Left.RedGiantPressure, Right.RedGiantPressure)
			&& FMath::IsNearlyEqual(Left.RedGiantPressurePerMissingFuel, Right.RedGiantPressurePerMissingFuel)
			&& Left.LastSettledSecondIndex == Right.LastSettledSecondIndex
			&& FMath::IsNearlyEqual(Left.LastSecondFuelConsumed, Right.LastSecondFuelConsumed)
			&& FMath::IsNearlyEqual(Left.LastSecondFuelDecrease, Right.LastSecondFuelDecrease)
			&& FMath::IsNearlyEqual(Left.LastSecondFuelDeficit, Right.LastSecondFuelDeficit)
			&& Left.bLastSecondSurvived == Right.bLastSecondSurvived
			&& Left.bSupernovaGameOver == Right.bSupernovaGameOver;
	}

	constexpr float FocusDetailsBoxWidth = 360.0f;
	constexpr float FocusDetailsBoxHeight = 260.0f;
	constexpr float BodyOperationsRefreshIntervalSeconds = 0.50f;

	FLinearColor GetOperationsPressureColor(const ESRCelestialBodyOperationsPressure Pressure)
	{
		switch (Pressure)
		{
		case ESRCelestialBodyOperationsPressure::OverCapacity:
			return FLinearColor(0.96f, 0.24f, 0.20f, 1.0f);
		case ESRCelestialBodyOperationsPressure::AtCapacity:
		case ESRCelestialBodyOperationsPressure::NearCapacity:
			return FLinearColor(1.0f, 0.68f, 0.16f, 1.0f);
		case ESRCelestialBodyOperationsPressure::Idle:
			return FLinearColor(0.52f, 0.68f, 0.76f, 1.0f);
		case ESRCelestialBodyOperationsPressure::Nominal:
		default:
			return FLinearColor(0.20f, 0.86f, 0.68f, 1.0f);
		}
	}

	FLinearColor GetResourceReservePressureColor(const ESRResourceReservePressure Pressure)
	{
		switch (Pressure)
		{
		case ESRResourceReservePressure::Depleted:
			return FLinearColor(0.96f, 0.24f, 0.20f, 1.0f);
		case ESRResourceReservePressure::Critical:
			return FLinearColor(1.0f, 0.42f, 0.18f, 1.0f);
		case ESRResourceReservePressure::Low:
			return FLinearColor(1.0f, 0.72f, 0.20f, 1.0f);
		case ESRResourceReservePressure::Healthy:
			return FLinearColor(0.30f, 0.86f, 0.58f, 1.0f);
		case ESRResourceReservePressure::Unavailable:
		default:
			return FLinearColor(0.52f, 0.68f, 0.76f, 1.0f);
		}
	}
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
	RefreshBodyOperationsSummary(true);
	RefreshFocusInfoText();
}

void USRCelestialBodyFocusInfoWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildFocusInfoWidgetTree();
	BindAssemblyModeButtonHandler();
	RefreshBodyOperationsSummary(true);
	RefreshFocusInfoText();
}

void USRCelestialBodyFocusInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible())
	{
		return;
	}

	const bool bStarFuelChanged = RefreshStarFuelInfoFromFocusedActor();
	BodyOperationsRefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
	const bool bBodyOperationsChanged = RefreshBodyOperationsSummary();
	if (bStarFuelChanged)
	{
		RefreshFocusInfoText();
	}
	else if (bBodyOperationsChanged)
	{
		RefreshBodyOperationsPanel();
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
	BodyOperationsRefreshAccumulator = BodyOperationsRefreshIntervalSeconds;
	RefreshBodyOperationsSummary(true);
	RefreshFocusInfoText();
	OnFocusInfoChanged(FocusInfo);
}

void USRCelestialBodyFocusInfoWidget::ClearFocusInfo()
{
	FocusInfo = FSRCelestialBodyFocusInfo();
	bHasFocusInfo = false;
	BodyOperationsSummary = FSRCelestialBodyOperationsSummary();
	BodyOperationsPanelSignature.Reset();
	BodyOperationsRefreshAccumulator = 0.0f;
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

FSRCelestialBodyOperationsSummary
USRCelestialBodyFocusInfoWidget::GetBodyOperationsSummary() const
{
	return BodyOperationsSummary;
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
		FocusInfoBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("FocusInfoBorder"))));
		VariableNameTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("VariableNameTextBlock"))));
		UWidget* FocusInfoContentParent = WidgetTree->FindWidget(FName(TEXT("FocusInfoVerticalBox")));
		if (!FocusInfoContentParent)
		{
			FocusInfoContentParent = WidgetTree->RootWidget;
		}
		EnsureBodyOperationsPanel(FocusInfoContentParent);
		EnsureHoveredCellTextBlock(FocusInfoContentParent);
		EnsureAssemblyModeButton(FocusInfoContentParent);
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

	EnsureBodyOperationsPanel(FocusInfoVerticalBox);

	EnsureHoveredCellTextBlock(FocusInfoVerticalBox);

	EnsureAssemblyModeButton(FocusInfoVerticalBox);
	BindAssemblyModeButtonHandler();
	RefreshFocusInfoText();
	RefreshAssemblyModeButton();
}

void USRCelestialBodyFocusInfoWidget::EnsureBodyOperationsPanel(
	UWidget* BodyOperationsPanelParent)
{
	if (!WidgetTree)
	{
		return;
	}

	auto FindTextBlock = [this](const TCHAR* WidgetName)
	{
		return Cast<UTextBlock>(WidgetTree->FindWidget(FName(WidgetName)));
	};

	if (!BodyOperationsContainer)
	{
		BodyOperationsContainer = Cast<USizeBox>(
			WidgetTree->FindWidget(FName(TEXT("BodyOperationsContainer"))));
	}
	if (!BodyOperationsBorder)
	{
		BodyOperationsBorder = Cast<UBorder>(
			WidgetTree->FindWidget(FName(TEXT("BodyOperationsBorder"))));
	}
	if (!BodyOperationsTitleTextBlock)
	{
		BodyOperationsTitleTextBlock = FindTextBlock(TEXT("BodyOperationsTitleTextBlock"));
	}
	if (!OperationalLoadTextBlock)
	{
		OperationalLoadTextBlock = FindTextBlock(TEXT("OperationalLoadTextBlock"));
	}
	if (!OperationalLoadProgressBar)
	{
		OperationalLoadProgressBar = Cast<UProgressBar>(
			WidgetTree->FindWidget(FName(TEXT("OperationalLoadProgressBar"))));
	}
	if (!OperationalStatusTextBlock)
	{
		OperationalStatusTextBlock = FindTextBlock(TEXT("OperationalStatusTextBlock"));
	}
	if (!ResourceReserveTextBlock)
	{
		ResourceReserveTextBlock = FindTextBlock(TEXT("ResourceReserveTextBlock"));
	}
	if (!CapacityBreakdownTextBlock)
	{
		CapacityBreakdownTextBlock = FindTextBlock(TEXT("CapacityBreakdownTextBlock"));
	}
	if (!FacilitySummaryTextBlock)
	{
		FacilitySummaryTextBlock = FindTextBlock(TEXT("FacilitySummaryTextBlock"));
	}
	if (!PrioritySpeedTextBlock)
	{
		PrioritySpeedTextBlock = FindTextBlock(TEXT("PrioritySpeedTextBlock"));
	}
	if (!FleetSummaryTextBlock)
	{
		FleetSummaryTextBlock = FindTextBlock(TEXT("FleetSummaryTextBlock"));
	}
	if (!LogisticsSummaryTextBlock)
	{
		LogisticsSummaryTextBlock = FindTextBlock(TEXT("LogisticsSummaryTextBlock"));
	}

	if (!BodyOperationsContainer)
	{
		BodyOperationsContainer = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("BodyOperationsContainer"));
	}
	if (!BodyOperationsBorder)
	{
		BodyOperationsBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("BodyOperationsBorder"));
	}

	UVerticalBox* BodyOperationsVerticalBox = Cast<UVerticalBox>(
		WidgetTree->FindWidget(FName(TEXT("BodyOperationsVerticalBox"))));
	if (!BodyOperationsVerticalBox)
	{
		BodyOperationsVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("BodyOperationsVerticalBox"));
	}

	auto EnsureTextBlock = [this](TObjectPtr<UTextBlock>& TextBlock, const TCHAR* WidgetName)
	{
		if (!TextBlock)
		{
			TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		}
		TextBlock->SetAutoWrapText(true);
		TextBlock->SetWrapTextAt(FocusDetailsBoxWidth - 44.0f);
		TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.88f, 0.94f, 1.0f)));
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = 12;
		TextBlock->SetFont(Font);
	};

	EnsureTextBlock(BodyOperationsTitleTextBlock, TEXT("BodyOperationsTitleTextBlock"));
	EnsureTextBlock(OperationalLoadTextBlock, TEXT("OperationalLoadTextBlock"));
	EnsureTextBlock(OperationalStatusTextBlock, TEXT("OperationalStatusTextBlock"));
	EnsureTextBlock(ResourceReserveTextBlock, TEXT("ResourceReserveTextBlock"));
	EnsureTextBlock(CapacityBreakdownTextBlock, TEXT("CapacityBreakdownTextBlock"));
	EnsureTextBlock(FacilitySummaryTextBlock, TEXT("FacilitySummaryTextBlock"));
	EnsureTextBlock(PrioritySpeedTextBlock, TEXT("PrioritySpeedTextBlock"));
	EnsureTextBlock(FleetSummaryTextBlock, TEXT("FleetSummaryTextBlock"));
	EnsureTextBlock(LogisticsSummaryTextBlock, TEXT("LogisticsSummaryTextBlock"));

	if (!OperationalLoadProgressBar)
	{
		OperationalLoadProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("OperationalLoadProgressBar"));
	}

	FSlateFontInfo TitleFont = BodyOperationsTitleTextBlock->GetFont();
	TitleFont.Size = 14;
	BodyOperationsTitleTextBlock->SetFont(TitleFont);
	BodyOperationsTitleTextBlock->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.64f, 0.90f, 1.0f, 1.0f)));
	BodyOperationsTitleTextBlock->SetText(
		NSLOCTEXT("StarRoversBodyOperations", "OperationsTitle", "OPERATIONS"));

	FSlateFontInfo LoadFont = OperationalLoadTextBlock->GetFont();
	LoadFont.Size = 16;
	OperationalLoadTextBlock->SetFont(LoadFont);
	OperationalLoadProgressBar->SetPercent(0.0f);

	BodyOperationsContainer->SetWidthOverride(FocusDetailsBoxWidth);
	BodyOperationsBorder->SetPadding(FMargin(12.0f));
	BodyOperationsBorder->SetBrushColor(FLinearColor(0.035f, 0.075f, 0.11f, 0.96f));
	if (BodyOperationsContainer->GetContent() != BodyOperationsBorder)
	{
		BodyOperationsContainer->SetContent(BodyOperationsBorder);
	}
	if (BodyOperationsBorder->GetContent() != BodyOperationsVerticalBox)
	{
		BodyOperationsBorder->SetContent(BodyOperationsVerticalBox);
	}

	auto AddTextRow = [BodyOperationsVerticalBox](UTextBlock* TextBlock, const FMargin& RowPadding)
	{
		if (!TextBlock || TextBlock->GetParent() == BodyOperationsVerticalBox)
		{
			return;
		}
		if (TextBlock->GetParent())
		{
			TextBlock->GetParent()->RemoveChild(TextBlock);
		}
		if (UVerticalBoxSlot* RowSlot = BodyOperationsVerticalBox->AddChildToVerticalBox(TextBlock))
		{
			RowSlot->SetPadding(RowPadding);
		}
	};

	AddTextRow(BodyOperationsTitleTextBlock, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	AddTextRow(OperationalLoadTextBlock, FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	if (OperationalLoadProgressBar->GetParent() != BodyOperationsVerticalBox)
	{
		if (OperationalLoadProgressBar->GetParent())
		{
			OperationalLoadProgressBar->GetParent()->RemoveChild(OperationalLoadProgressBar);
		}
		if (UVerticalBoxSlot* ProgressSlot = BodyOperationsVerticalBox->AddChildToVerticalBox(OperationalLoadProgressBar))
		{
			ProgressSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}
	}
	AddTextRow(OperationalStatusTextBlock, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	AddTextRow(ResourceReserveTextBlock, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	AddTextRow(CapacityBreakdownTextBlock, FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	AddTextRow(FacilitySummaryTextBlock, FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	AddTextRow(PrioritySpeedTextBlock, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	AddTextRow(FleetSummaryTextBlock, FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	AddTextRow(LogisticsSummaryTextBlock, FMargin(0.0f));

	if (!BodyOperationsContainer->GetParent())
	{
		if (UVerticalBox* ParentVerticalBox = Cast<UVerticalBox>(BodyOperationsPanelParent))
		{
			if (UVerticalBoxSlot* OperationsSlot = ParentVerticalBox->AddChildToVerticalBox(BodyOperationsContainer))
			{
				OperationsSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 8.0f));
			}
		}
		else if (UCanvasPanel* ParentCanvasPanel = Cast<UCanvasPanel>(BodyOperationsPanelParent))
		{
			if (UCanvasPanelSlot* OperationsSlot = ParentCanvasPanel->AddChildToCanvas(BodyOperationsContainer))
			{
				OperationsSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
				OperationsSlot->SetAlignment(FVector2D(1.0f, 0.0f));
				OperationsSlot->SetPosition(FVector2D(-408.0f, 32.0f));
				OperationsSlot->SetAutoSize(true);
			}
		}
		else if (UPanelWidget* ParentPanelWidget = Cast<UPanelWidget>(BodyOperationsPanelParent))
		{
			ParentPanelWidget->AddChild(BodyOperationsContainer);
		}
	}

	RefreshBodyOperationsPanel();
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

bool USRCelestialBodyFocusInfoWidget::RefreshStarFuelInfoFromFocusedActor()
{
	if (!bHasFocusInfo || !FocusInfo.bHasStarFuelInfo || !IsValid(FocusInfo.Actor))
	{
		return false;
	}

	const ASRStar* Star = Cast<ASRStar>(FocusInfo.Actor.Get());
	if (!IsValid(Star))
	{
		return false;
	}

	const FSRStellarFuelState FuelState = Star->GetStellarFuelState();
	FSRFocusedStarFuelInfo NewFuelInfo;
	NewFuelInfo.bIsValid = true;
	NewFuelInfo.EvolutionStage = FuelState.EvolutionStage;
	NewFuelInfo.StoredFuel = FuelState.StoredFuel;
	NewFuelInfo.InitialStageFuel = FuelState.InitialStageFuel;
	NewFuelInfo.InitialFuelDecreasePerSecond = FuelState.InitialFuelDecreasePerSecond;
	NewFuelInfo.RequiredFuelPerCycle = FuelState.RequiredFuelPerCycle;
	NewFuelInfo.RequirementGrowthPerCycle = FuelState.RequirementGrowthPerCycle;
	NewFuelInfo.bUsesStellarPressureCurveV2 = FuelState.bUsesStellarPressureCurveV2;
	NewFuelInfo.DemandPhase = FuelState.DemandPhase;
	NewFuelInfo.NextCycleDemandPerSecond = FuelState.NextCycleDemandPerSecond;
	NewFuelInfo.FuelPressureRatio = FuelState.FuelPressureRatio;
	NewFuelInfo.LastFuelDeliveryAmount = FuelState.LastFuelDeliveryAmount;
	NewFuelInfo.LastFuelReserveGain = FuelState.LastFuelReserveGain;
	NewFuelInfo.LastFuelReserveOverflow = FuelState.LastFuelReserveOverflow;
	NewFuelInfo.LastFuelDecreaseRateCycleIndex = FuelState.LastFuelDecreaseRateCycleIndex;
	NewFuelInfo.RedGiantPressure = FuelState.RedGiantPressure;
	NewFuelInfo.RedGiantPressurePerMissingFuel = FuelState.RedGiantPressurePerMissingFuel;
	NewFuelInfo.LastSettledSecondIndex = FuelState.LastSettledSecondIndex;
	NewFuelInfo.LastSecondFuelConsumed = FuelState.LastSecondFuelConsumed;
	NewFuelInfo.LastSecondFuelDecrease = FuelState.LastSecondFuelDecrease;
	NewFuelInfo.LastSecondFuelDeficit = FuelState.LastSecondFuelDeficit;
	NewFuelInfo.bLastSecondSurvived = FuelState.bLastSecondSurvived;
	NewFuelInfo.bSupernovaGameOver = FuelState.bSupernovaGameOver;

	if (AreFocusedStarFuelInfosEqual(FocusInfo.StarFuelInfo, NewFuelInfo))
	{
		return false;
	}

	FocusInfo.StarFuelInfo = NewFuelInfo;
	return true;
}

bool USRCelestialBodyFocusInfoWidget::RefreshBodyOperationsSummary(bool bForceRefresh)
{
	if (!bForceRefresh
		&& BodyOperationsRefreshAccumulator < BodyOperationsRefreshIntervalSeconds)
	{
		return false;
	}

	BodyOperationsRefreshAccumulator = 0.0f;
	FSRCelestialBodyOperationsSummary NewSummary;
	if (bHasFocusInfo && FocusInfo.bIsValid && IsValid(FocusInfo.Actor))
	{
		FSRCelestialBodyOperationsSummaryBuilder::BuildSummary(
			FocusInfo.Actor.Get(), NewSummary);
	}
	BodyOperationsSummary = MoveTemp(NewSummary);
	return true;
}

void USRCelestialBodyFocusInfoWidget::RefreshBodyOperationsPanel()
{
	if (!BodyOperationsContainer)
	{
		return;
	}

	if (!bHasFocusInfo || !BodyOperationsSummary.bIsValid)
	{
		BodyOperationsContainer->SetVisibility(ESlateVisibility::Collapsed);
		BodyOperationsPanelSignature.Reset();
		return;
	}

	BodyOperationsContainer->SetVisibility(ESlateVisibility::Visible);
	const FSROperationalCapacityReportV2& Capacity =
		BodyOperationsSummary.OperationalCapacity;
	const float Utilization =
		FSRCelestialBodyOperationsSummaryBuilder::GetOperationalUtilization(
			BodyOperationsSummary);
	const ESRCelestialBodyOperationsPressure Pressure =
		FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(
			BodyOperationsSummary);
	const FLinearColor PressureColor = GetOperationsPressureColor(Pressure);
	const int32 UtilizationPercent = Capacity.TotalCapacity > 0
		? FMath::RoundToInt(Utilization * 100.0f)
		: (Capacity.TotalDemand > 0 ? 999 : 0);
	const FString StatusText = Capacity.bRulesActive
		? FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalStatusText(
			BodyOperationsSummary)
		: FString(TEXT("Legacy ruleset - Capacity is informational"));
	const FString BusiestHubText = BodyOperationsSummary.HubCount > 1
		? FString::Printf(
			TEXT(" | Busiest %d/%d"),
			BodyOperationsSummary.BusiestHubReservedLoad,
			BodyOperationsSummary.BusiestHubTotalCapacity)
		: FString();
	const FString FleetText = BodyOperationsSummary.HubCount > 0
		? FString::Printf(
			TEXT("Fleet Load %d / %d | Available %d | Queue %d%s"),
			BodyOperationsSummary.FleetReservedLoad,
			BodyOperationsSummary.FleetTotalCapacity,
			BodyOperationsSummary.FleetAvailableCapacity,
			BodyOperationsSummary.FleetQueuedDepartureCount,
			*BusiestHubText)
		: FString(TEXT("Fleet: no Hub on this body"));
	const FString LogisticsText = BodyOperationsSummary.HubCount > 0
		? FString::Printf(
			TEXT("Logistics: %d Hubs | %d Routes | %d Blocked | %d supplied Berths | %d Missiles"),
			BodyOperationsSummary.HubCount,
			BodyOperationsSummary.ConnectedRouteCount,
			BodyOperationsSummary.BlockedRouteCount,
			BodyOperationsSummary.ActiveFleetBerthCount,
			BodyOperationsSummary.ActiveStarFuelMissileCount)
		: FString::Printf(
			TEXT("Logistics: %d Routes | %d Missiles"),
			BodyOperationsSummary.ConnectedRouteCount,
			BodyOperationsSummary.ActiveStarFuelMissileCount);

	const FString NewSignature = FString::Printf(
		TEXT("Rules=%d|Capacity=%d,%d,%d,%d,%d,%d|Tiers=%d,%d,%d,%d,%d,%d|Facilities=%d,%d,%d,%d|Reserve=%d,%d,%d,%lld,%lld,%d,%d|Logistics=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"),
		Capacity.bRulesActive ? 1 : 0,
		Capacity.BaseCapacity,
		Capacity.ActiveServiceCoreCount,
		Capacity.ServiceCoreCapacity,
		Capacity.AugmentCapacity,
		Capacity.TotalCapacity,
		Capacity.TotalDemand,
		Capacity.Critical.Demand,
		FMath::RoundToInt(Capacity.Critical.SpeedFactor * 1000.0f),
		Capacity.Normal.Demand,
		FMath::RoundToInt(Capacity.Normal.SpeedFactor * 1000.0f),
		Capacity.Background.Demand,
		FMath::RoundToInt(Capacity.Background.SpeedFactor * 1000.0f),
		BodyOperationsSummary.FacilityCount,
		BodyOperationsSummary.EnabledFacilityCount,
		BodyOperationsSummary.ProcessingFacilityCount,
		BodyOperationsSummary.ThrottledFacilityCount,
		BodyOperationsSummary.ResourceReserve.DepositCount,
		BodyOperationsSummary.ResourceReserve.ActiveDepositCount,
		BodyOperationsSummary.ResourceReserve.DepletedDepositCount,
		BodyOperationsSummary.ResourceReserve.RemainingCardAmount,
		BodyOperationsSummary.ResourceReserve.RemainingUtilityAmount,
		FMath::RoundToInt(BodyOperationsSummary.ResourceReserve.RemainingRatio * 1000.0f),
		static_cast<int32>(BodyOperationsSummary.ResourceReserve.Pressure),
		BodyOperationsSummary.HubCount,
		BodyOperationsSummary.ConnectedRouteCount,
		BodyOperationsSummary.BlockedRouteCount,
		BodyOperationsSummary.FleetReservedLoad,
		BodyOperationsSummary.FleetTotalCapacity,
		BodyOperationsSummary.FleetAvailableCapacity,
		BodyOperationsSummary.FleetQueuedDepartureCount,
		BodyOperationsSummary.ActiveFleetBerthCount,
		BodyOperationsSummary.BusiestHubReservedLoad,
		BodyOperationsSummary.BusiestHubTotalCapacity,
		BodyOperationsSummary.ActiveStarFuelMissileCount);
	if (BodyOperationsPanelSignature == NewSignature)
	{
		return;
	}
	BodyOperationsPanelSignature = NewSignature;

	if (OperationalLoadTextBlock)
	{
		OperationalLoadTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("OPERATIONAL LOAD  %d / %d  (%s)"),
			Capacity.TotalDemand,
			Capacity.TotalCapacity,
			Capacity.TotalCapacity > 0
				? *FString::Printf(TEXT("%d%%"), UtilizationPercent)
				: (Capacity.TotalDemand > 0 ? TEXT("NO CAPACITY") : TEXT("0%")))));
		OperationalLoadTextBlock->SetColorAndOpacity(FSlateColor(PressureColor));
	}
	if (OperationalLoadProgressBar)
	{
		OperationalLoadProgressBar->SetPercent(FMath::Clamp(Utilization, 0.0f, 1.0f));
		OperationalLoadProgressBar->SetFillColorAndOpacity(PressureColor);
	}
	if (OperationalStatusTextBlock)
	{
		OperationalStatusTextBlock->SetText(FText::FromString(StatusText));
		OperationalStatusTextBlock->SetColorAndOpacity(FSlateColor(PressureColor));
	}
	if (ResourceReserveTextBlock)
	{
		ResourceReserveTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n%s"),
			*FSRCelestialBodyOperationsSummaryBuilder::BuildResourceReserveText(
				BodyOperationsSummary),
			*FSRCelestialBodyOperationsSummaryBuilder::BuildResourceReserveStatusText(
				BodyOperationsSummary))));
		ResourceReserveTextBlock->SetColorAndOpacity(FSlateColor(
			GetResourceReservePressureColor(BodyOperationsSummary.ResourceReserve.Pressure)));
	}
	if (CapacityBreakdownTextBlock)
	{
		CapacityBreakdownTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Capacity %d = Base %d + Cores %d (+%d) + Augments %d"),
			Capacity.TotalCapacity,
			Capacity.BaseCapacity,
			Capacity.ActiveServiceCoreCount,
			Capacity.ServiceCoreCapacity,
			Capacity.AugmentCapacity)));
	}
	if (FacilitySummaryTextBlock)
	{
		FacilitySummaryTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Facilities %d | Enabled %d | Processing %d | Throttled %d"),
			BodyOperationsSummary.FacilityCount,
			BodyOperationsSummary.EnabledFacilityCount,
			BodyOperationsSummary.ProcessingFacilityCount,
			BodyOperationsSummary.ThrottledFacilityCount)));
	}
	if (PrioritySpeedTextBlock)
	{
		PrioritySpeedTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Priority Speed: C %.0f%% (%d) | N %.0f%% (%d) | B %.0f%% (%d)"),
			Capacity.Critical.SpeedFactor * 100.0f,
			Capacity.Critical.Demand,
			Capacity.Normal.SpeedFactor * 100.0f,
			Capacity.Normal.Demand,
			Capacity.Background.SpeedFactor * 100.0f,
			Capacity.Background.Demand)));
	}
	if (FleetSummaryTextBlock)
	{
		FleetSummaryTextBlock->SetText(FText::FromString(FleetText));
		FleetSummaryTextBlock->SetColorAndOpacity(FSlateColor(
			BodyOperationsSummary.FleetQueuedDepartureCount > 0
				? FLinearColor(1.0f, 0.68f, 0.16f, 1.0f)
				: FLinearColor(0.68f, 0.86f, 0.96f, 1.0f)));
	}
	if (LogisticsSummaryTextBlock)
	{
		LogisticsSummaryTextBlock->SetText(FText::FromString(LogisticsText));
		LogisticsSummaryTextBlock->SetColorAndOpacity(FSlateColor(
			BodyOperationsSummary.BlockedRouteCount > 0
				? FLinearColor(1.0f, 0.46f, 0.24f, 1.0f)
				: FLinearColor(0.68f, 0.86f, 0.96f, 1.0f)));
	}

	const FString BodyOperationsToolTipText = FString::Printf(
		TEXT("%s\n\nFleet Capacity is reserved per Hub while a ship is away from its dock. Multiple Hubs are aggregated here; Busiest identifies the tightest individual Hub."),
		*FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalToolTipText(
			BodyOperationsSummary));
	BodyOperationsContainer->SetToolTipText(FText::FromString(BodyOperationsToolTipText));
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

	if (HoveredCellTextBlock)
	{
		const bool bHasSelectedNonFacilityStructure = FocusInfo.bHasSelectedSurfaceStructure
			&& !FocusInfo.SelectedSurfaceStructureInfo.bHasFacilityRuntimeInfo;
		const bool bHasSurfaceFocusDetails = bAssemblyModeActive && (FocusInfo.bHasHoveredSurfaceCell || bHasSelectedNonFacilityStructure);
		const bool bHasStarFuelDetails = FocusInfo.bHasStarFuelInfo && FocusInfo.StarFuelInfo.bIsValid;
		if (bHasFocusInfo && (bHasSurfaceFocusDetails || bHasStarFuelDetails))
		{
			FString CellText;
			if (bHasStarFuelDetails)
			{
				CellText += BuildFocusedStarFuelSummary(FocusInfo.StarFuelInfo);
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
			if (HoveredCellContainer)
			{
				HoveredCellContainer->SetVisibility(ESlateVisibility::Visible);
			}
		}
		else
		{
			HoveredCellTextBlock->SetText(FText::GetEmpty());
			HoveredCellTextBlock->SetVisibility(ESlateVisibility::Hidden);
			if (HoveredCellContainer)
			{
				HoveredCellContainer->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	RefreshBodyOperationsPanel();
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
	if (BodyOperationsBorder
		&& BodyOperationsBorder->IsVisible()
		&& BodyOperationsBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition))
	{
		return true;
	}

	return AssemblyModeButton
		&& AssemblyModeButton->IsVisible()
		&& AssemblyModeButton->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}
