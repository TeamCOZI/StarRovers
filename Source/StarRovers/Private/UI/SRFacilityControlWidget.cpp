#include "UI/SRFacilityControlWidget.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/SRPlayerController.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"

namespace
{
	const TCHAR* GetFacilityTemperatureLabel(ESRFacilityTemperatureState TemperatureState)
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

	const TCHAR* GetResourceKindLabel(ESRResourceKind ResourceKind)
	{
		switch (ResourceKind)
		{
		case ESRResourceKind::Energy:
			return TEXT("Energy");
		case ESRResourceKind::Catalyst:
			return TEXT("Catalyst");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetCatalystOperatorLabel(ESRResourceCatalystOperator CatalystOperator)
	{
		switch (CatalystOperator)
		{
		case ESRResourceCatalystOperator::Add:
			return TEXT("+");
		case ESRResourceCatalystOperator::Multiply:
			return TEXT("*");
		case ESRResourceCatalystOperator::Subtract:
			return TEXT("-");
		case ESRResourceCatalystOperator::Divide:
			return TEXT("/");
		case ESRResourceCatalystOperator::None:
		default:
			return TEXT("None");
		}
	}

	const TCHAR* GetEffectKindLabel(ESRFacilityEffectKind EffectKind)
	{
		switch (EffectKind)
		{
		case ESRFacilityEffectKind::AddEnergy:
			return TEXT("Energy +");
		case ESRFacilityEffectKind::MultiplyEnergy:
			return TEXT("Energy *");
		case ESRFacilityEffectKind::AddProcessLimit:
			return TEXT("Limit +");
		case ESRFacilityEffectKind::AddTag:
			return TEXT("Add Tag");
		case ESRFacilityEffectKind::RemoveTag:
			return TEXT("Remove Tag");
		case ESRFacilityEffectKind::ProduceResource:
			return TEXT("Produce");
		case ESRFacilityEffectKind::SubtractEnergy:
			return TEXT("Energy -");
		case ESRFacilityEffectKind::DivideEnergy:
			return TEXT("Energy /");
		case ESRFacilityEffectKind::SubtractProcessLimit:
			return TEXT("Limit -");
		case ESRFacilityEffectKind::MultiplyProcessLimit:
			return TEXT("Limit *");
		case ESRFacilityEffectKind::DivideProcessLimit:
			return TEXT("Limit /");
		case ESRFacilityEffectKind::AddCellTemperature:
			return TEXT("Cell Temp +");
		case ESRFacilityEffectKind::SubtractCellTemperature:
			return TEXT("Cell Temp -");
		default:
			return TEXT("Effect");
		}
	}

	FString BuildResourceDisplayName(const FSRResourceInstance& ResourceInstance)
	{
		if (const USRResourceDataAsset* ResourceDataAsset = ResourceInstance.ResourceDataAsset.Get())
		{
			if (!ResourceDataAsset->DisplayName.IsEmpty())
			{
				return ResourceDataAsset->DisplayName.ToString();
			}
			if (!ResourceDataAsset->ResourceId.IsNone())
			{
				return ResourceDataAsset->ResourceId.ToString();
			}
		}

		return ResourceInstance.ResourceId.IsNone()
			? TEXT("UnknownResource")
			: ResourceInstance.ResourceId.ToString();
	}

	FString BuildResourceDataAssetDisplayName(const USRResourceDataAsset* ResourceDataAsset)
	{
		if (!IsValid(ResourceDataAsset))
		{
			return TEXT("UnknownResource");
		}

		if (!ResourceDataAsset->DisplayName.IsEmpty())
		{
			return ResourceDataAsset->DisplayName.ToString();
		}

		return ResourceDataAsset->ResourceId.IsNone()
			? GetNameSafe(ResourceDataAsset)
			: ResourceDataAsset->ResourceId.ToString();
	}

	FString BuildResourceSummary(const FSRResourceInstance& ResourceInstance)
	{
		if (ResourceInstance.ResourceId.IsNone())
		{
			return TEXT("None");
		}

		FString Summary = FString::Printf(
			TEXT("%s [%s] x%d"),
			*BuildResourceDisplayName(ResourceInstance),
			GetResourceKindLabel(ResourceInstance.ResourceKind),
			FMath::Max(0, ResourceInstance.StackCount));

		if (ResourceInstance.ResourceKind == ESRResourceKind::Catalyst)
		{
			Summary += FString::Printf(TEXT("\nOp: %s"), GetCatalystOperatorLabel(ResourceInstance.CatalystOperator));
		}
		else
		{
			Summary += FString::Printf(
				TEXT("\nEnergy Total: %.2f\nLimit: %d\nUsed: %d"),
				ResourceInstance.EnergyValue,
				ResourceInstance.RemainingProcessLimit,
				ResourceInstance.ProcessCount);
		}

		if (!ResourceInstance.Tags.IsEmpty())
		{
			Summary += FString::Printf(TEXT("\nTags: %d"), ResourceInstance.Tags.Num());
		}
		return Summary;
	}

	FString BuildCompactResourceSummary(const FSRResourceInstance* ResourceInstance)
	{
		if (!ResourceInstance || ResourceInstance->ResourceId.IsNone())
		{
			return TEXT("Empty");
		}

		FString Summary = BuildResourceDisplayName(*ResourceInstance);
		if (ResourceInstance->ResourceKind == ESRResourceKind::Catalyst)
		{
			Summary += FString::Printf(TEXT("\nCatalyst  Op: %s"), GetCatalystOperatorLabel(ResourceInstance->CatalystOperator));
			return Summary;
		}

		Summary += FString::Printf(
			TEXT("\nEnergy Total: %.2f  Limit: %d"),
			ResourceInstance->EnergyValue,
			ResourceInstance->RemainingProcessLimit);
		if (!ResourceInstance->Tags.IsEmpty())
		{
			Summary += FString::Printf(TEXT("\nTags: %d"), ResourceInstance->Tags.Num());
		}
		return Summary;
	}

	FString BuildResourceSlotText(
		const TCHAR* Label,
		int32 SlotIndex,
		FName PortId,
		const FSRResourceInstance* ResourceInstance,
		const TCHAR* EmptyText)
	{
		const FString PortLabel = PortId.IsNone()
			? FString::Printf(TEXT("%s %d"), Label, SlotIndex + 1)
			: PortId.ToString();
		const FString ResourceText = ResourceInstance
			? BuildCompactResourceSummary(ResourceInstance)
			: FString(EmptyText);
		return FString::Printf(TEXT("%s %d\n%s\n%s"), Label, SlotIndex + 1, *PortLabel, *ResourceText);
	}

	FString BuildInlineResourceSummary(const FSRResourceInstance& ResourceInstance)
	{
		FString Summary = BuildResourceDisplayName(ResourceInstance);
		if (ResourceInstance.ResourceKind == ESRResourceKind::Energy)
		{
			Summary += FString::Printf(TEXT("  Energy Total: %.2f"), ResourceInstance.EnergyValue);
		}
		else if (ResourceInstance.ResourceKind == ESRResourceKind::Catalyst)
		{
			Summary += FString::Printf(TEXT("  Op: %s"), GetCatalystOperatorLabel(ResourceInstance.CatalystOperator));
		}
		return Summary;
	}

	FString BuildInventorySummary(const TCHAR* Label, const TArray<FSRResourceInstance>& Inventory)
	{
		FString Summary = FString::Printf(TEXT("%s (%d)"), Label, Inventory.Num());
		for (int32 ResourceIndex = 0; ResourceIndex < Inventory.Num(); ++ResourceIndex)
		{
			Summary += FString::Printf(
				TEXT("\n%d. %s"),
				ResourceIndex + 1,
				*BuildInlineResourceSummary(Inventory[ResourceIndex]));
		}
		return Summary;
	}

	FString BuildPortInventorySummary(const TCHAR* Label, const TArray<FSRFacilityPortInventory>& PortInventories)
	{
		FString Summary = FString::Printf(TEXT("%s (%d ports)"), Label, PortInventories.Num());
		if (PortInventories.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		for (int32 PortIndex = 0; PortIndex < PortInventories.Num(); ++PortIndex)
		{
			const FSRFacilityPortInventory& PortInventory = PortInventories[PortIndex];
			Summary += FString::Printf(
				TEXT("\n%s (%d/%d)"),
				*PortInventory.PortId.ToString(),
				PortInventory.Inventory.Num(),
				FMath::Max(1, PortInventory.Capacity));

			if (PortInventory.Inventory.IsEmpty())
			{
				Summary += TEXT("\n  Empty");
				continue;
			}

			for (int32 ResourceIndex = 0; ResourceIndex < PortInventory.Inventory.Num(); ++ResourceIndex)
			{
				Summary += FString::Printf(
					TEXT("\n  %d. %s"),
					ResourceIndex + 1,
					*BuildInlineResourceSummary(PortInventory.Inventory[ResourceIndex]));
			}
		}
		return Summary;
	}

	FString BuildMiningTargetSummary(USRFacilityNetworkComponent* FacilityNetwork, FName OccupantId)
	{
		if (!IsValid(FacilityNetwork) || OccupantId.IsNone())
		{
			return TEXT("Mining Target\nNo adjacent deposit");
		}

		FSRResourceDepositInstance ResourceDeposit;
		if (!FacilityNetwork->GetFacilityMiningTarget(OccupantId, ResourceDeposit))
		{
			return TEXT("Mining Target\nNo adjacent deposit");
		}

		return FString::Printf(
			TEXT("Mining Target\nDeposit: %s\nResource: %s\nRemaining: %d / %d"),
			ResourceDeposit.StructureId.IsNone() ? *ResourceDeposit.OccupantId.ToString() : *ResourceDeposit.StructureId.ToString(),
			*BuildResourceDataAssetDisplayName(ResourceDeposit.ResourceDataAsset.Get()),
			FMath::Max(0, ResourceDeposit.RemainingAmount),
			FMath::Max(0, ResourceDeposit.TotalAmount));
	}

	bool HasAvailableInputPortCapacity(const FSRFacilityInstance& FacilityInstance)
	{
		for (const FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
		{
			if (InputPortInventory.Inventory.Num() < FMath::Max(1, InputPortInventory.Capacity))
			{
				return true;
			}
		}
		return false;
	}

	FString BuildResourceListSummary(const TCHAR* Label, const TArray<FSRResourceInstance>& ResourceInstances)
	{
		FString Summary = Label;
		if (ResourceInstances.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		const int32 VisibleCount = FMath::Min(ResourceInstances.Num(), 3);
		for (int32 ResourceIndex = 0; ResourceIndex < VisibleCount; ++ResourceIndex)
		{
			Summary += FString::Printf(
				TEXT("\n%d. %s"),
				ResourceIndex + 1,
				*BuildResourceSummary(ResourceInstances[ResourceIndex]));
		}
		if (ResourceInstances.Num() > VisibleCount)
		{
			Summary += FString::Printf(TEXT("\n... +%d"), ResourceInstances.Num() - VisibleCount);
		}
		return Summary;
	}

	FString BuildEffectsSummary(const USRFacilityDataAsset* FacilityDataAsset)
	{
		if (!IsValid(FacilityDataAsset))
		{
			return TEXT("Effects\nNone");
		}

		FString Summary = TEXT("Effects / Tags");
		if (FacilityDataAsset->Effects.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		const int32 VisibleCount = FMath::Min(FacilityDataAsset->Effects.Num(), 5);
		for (int32 EffectIndex = 0; EffectIndex < VisibleCount; ++EffectIndex)
		{
			const FSRFacilityEffectSpec& EffectSpec = FacilityDataAsset->Effects[EffectIndex];
			Summary += FString::Printf(
				TEXT("\n- %s %.2f"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				EffectSpec.Value);
		}
		if (FacilityDataAsset->Effects.Num() > VisibleCount)
		{
			Summary += FString::Printf(TEXT("\n... +%d"), FacilityDataAsset->Effects.Num() - VisibleCount);
		}
		return Summary;
	}

	float ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		float ProcessSeconds = IsValid(FacilityDataAsset) ? FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds) : 1.0f;
		if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			ProcessSeconds *= 2.0f;
		}
		return ProcessSeconds;
	}

	bool CanToggleProcess(const FSRFacilityInstance& FacilityInstance, FString& OutReason)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			OutReason = TEXT("Invalid facility");
			return false;
		}
		if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Frozen
			|| FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Overheated)
		{
			OutReason = FString::Printf(TEXT("Blocked by %s"), GetFacilityTemperatureLabel(FacilityInstance.TemperatureState));
			return false;
		}
		if (FacilityDataAsset->bRequiresColdTemperature && FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Cold)
		{
			OutReason = TEXT("Requires Cold");
			return false;
		}
		if (FacilityDataAsset->bRequiresHotTemperature && FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Hot)
		{
			OutReason = TEXT("Requires Hot");
			return false;
		}

		OutReason = TEXT("Ready");
		return true;
	}

	UTextBlock* ConstructTextBlock(UWidgetTree* WidgetTree, const FName& Name, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}

	void AddWidgetToCanvas(UCanvasPanel* CanvasPanel, UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
	{
		if (!CanvasPanel || !Widget)
		{
			return;
		}

		if (UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(Widget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetSize(Size);
		}
	}

	UBorder* ConstructSectionBorder(
		UWidgetTree* WidgetTree,
		const FName& Name,
		UWidget* Content,
		const FLinearColor& Color = FLinearColor(0.075f, 0.095f, 0.115f, 0.96f),
		const FMargin& Padding = FMargin(10.0f))
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		if (Content)
		{
			Border->SetContent(Content);
		}
		return Border;
	}

	void AddResourceSlotCard(
		UWidgetTree* WidgetTree,
		UVerticalBox* SlotBox,
		const FString& Text,
		const FLinearColor& TextColor,
		const FLinearColor& CardColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		UTextBlock* SlotTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 11, TextColor);
		SlotTextBlock->SetText(FText::FromString(Text));
		SlotTextBlock->SetJustification(ETextJustify::Left);

		UBorder* SlotBorder = ConstructSectionBorder(WidgetTree, NAME_None, SlotTextBlock, CardColor, FMargin(7.0f, 5.0f));
		if (UVerticalBoxSlot* Slot = SlotBox->AddChildToVerticalBox(SlotBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	UButton* ConstructDebugInputButton(UWidgetTree* WidgetTree, const FName& ButtonName, const FText& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		Button->SetBackgroundColor(FLinearColor(0.16f, 0.22f, 0.28f, 0.95f));

		UTextBlock* LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelTextBlock->SetText(Label);
		LabelTextBlock->SetJustification(ETextJustify::Center);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo FontInfo = LabelTextBlock->GetFont();
		FontInfo.Size = 12;
		LabelTextBlock->SetFont(FontInfo);
		Button->AddChild(LabelTextBlock);
		return Button;
	}

	bool IsWidgetUnderScreenPosition(const UWidget* Widget, const FVector2D& ScreenPosition)
	{
		return IsValid(Widget)
			&& Widget->IsVisible()
			&& Widget->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	}

	void BindInputSlotDebugButton(
		UButton* Button,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRFacilityInputSlotDebugAction>>& OutActions,
		int32 InputPortIndex,
		FName ResourceId)
	{
		if (!Button || !OwnerWidget || InputPortIndex == INDEX_NONE || ResourceId.IsNone())
		{
			return;
		}

		USRFacilityInputSlotDebugAction* Action = NewObject<USRFacilityInputSlotDebugAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, InputPortIndex, ResourceId);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRFacilityInputSlotDebugAction::HandleClicked);
	}

	void AddInputResourceSlotCard(
		UWidgetTree* WidgetTree,
		UVerticalBox* SlotBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRFacilityInputSlotDebugAction>>& OutActions,
		const FString& Text,
		int32 InputPortIndex,
		bool bCanAddResource,
		bool bShowDebugButtons)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* SlotTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 11, FLinearColor(0.84f, 0.91f, 1.0f, 1.0f));
		SlotTextBlock->SetText(FText::FromString(Text));
		if (UVerticalBoxSlot* TextSlot = CardBox->AddChildToVerticalBox(SlotTextBlock))
		{
			TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		if (bShowDebugButtons)
		{
			UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			struct FInputDebugButtonSpec
			{
				FName ResourceId;
				FText Label;
			};
			const FInputDebugButtonSpec ButtonSpecs[] =
			{
				{ TEXT("Territe"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddTerrite", "+T") },
				{ TEXT("Aquid"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddAquid", "+A") },
				{ TEXT("Nitain"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddNitain", "+N") },
			};

			for (int32 ButtonIndex = 0; ButtonIndex < UE_ARRAY_COUNT(ButtonSpecs); ++ButtonIndex)
			{
				UButton* Button = ConstructDebugInputButton(WidgetTree, NAME_None, ButtonSpecs[ButtonIndex].Label);
				Button->SetIsEnabled(bCanAddResource);
				BindInputSlotDebugButton(Button, OwnerWidget, OutActions, InputPortIndex, ButtonSpecs[ButtonIndex].ResourceId);
				if (UHorizontalBoxSlot* ButtonSlot = ButtonRow->AddChildToHorizontalBox(Button))
				{
					ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, ButtonIndex < UE_ARRAY_COUNT(ButtonSpecs) - 1 ? 4.0f : 0.0f, 0.0f));
					ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				}
			}
			if (UVerticalBoxSlot* ButtonRowSlot = CardBox->AddChildToVerticalBox(ButtonRow))
			{
				ButtonRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}

		UBorder* SlotBorder = ConstructSectionBorder(
			WidgetTree,
			NAME_None,
			CardBox,
			bCanAddResource ? FLinearColor(0.080f, 0.105f, 0.135f, 0.98f) : FLinearColor(0.060f, 0.070f, 0.082f, 0.98f),
			FMargin(7.0f, 5.0f));
		if (UVerticalBoxSlot* Slot = SlotBox->AddChildToVerticalBox(SlotBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	FSRResourceInstance MakeDebugEnergyResource(FName ResourceId, double EnergyValue, int32 RemainingProcessLimit)
	{
		FSRResourceInstance ResourceInstance;
		ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		ResourceInstance.ResourceId = ResourceId;
		ResourceInstance.ResourceKind = ESRResourceKind::Energy;
		ResourceInstance.EnergyValue = EnergyValue;
		ResourceInstance.CatalystOperator = ESRResourceCatalystOperator::None;
		ResourceInstance.RemainingProcessLimit = FMath::Max(0, RemainingProcessLimit);
		ResourceInstance.ProcessCount = 0;
		ResourceInstance.StackCount = 1;
		return ResourceInstance;
	}

	FSRResourceInstance MakeDebugCatalystResource(FName ResourceId, ESRResourceCatalystOperator CatalystOperator)
	{
		FSRResourceInstance ResourceInstance;
		ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		ResourceInstance.ResourceId = ResourceId;
		ResourceInstance.ResourceKind = ESRResourceKind::Catalyst;
		ResourceInstance.EnergyValue = 0.0;
		ResourceInstance.CatalystOperator = CatalystOperator;
		ResourceInstance.RemainingProcessLimit = 0;
		ResourceInstance.ProcessCount = 0;
		ResourceInstance.StackCount = 1;
		return ResourceInstance;
	}
}

void USRFacilityInputSlotDebugAction::Initialize(USRFacilityControlWidget* InOwnerWidget, int32 InInputPortIndex, FName InResourceId)
{
	OwnerWidget = InOwnerWidget;
	InputPortIndex = InInputPortIndex;
	ResourceId = InResourceId;
}

void USRFacilityInputSlotDebugAction::HandleClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl InputSlotDebug OnClicked InputPortIndex=%d ResourceId=%s"),
		InputPortIndex,
		*ResourceId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->AddDebugInputResourceToPort(InputPortIndex, ResourceId);
	}
}

TSharedRef<SWidget> USRFacilityControlWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildFacilityControlWidgetTree();
	return Super::RebuildWidget();
}

void USRFacilityControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFacilityControlWidgetTree();
	BindControlHandlers();
	RefreshControlText();
}

void USRFacilityControlWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildFacilityControlWidgetTree();
	RefreshControlText();
}

void USRFacilityControlWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible() || !bHasFocusedFacility)
	{
		return;
	}

	RefreshControlText();
}

FReply USRFacilityControlWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverControlPanel(ScreenPosition))
	{
		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRFacilityControlWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverControlPanel(ScreenPosition))
	{
		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRFacilityControlWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverControlPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRFacilityControlWidget::SetFocusedFacility(AActor* NewFocusedActor, FName NewOccupantId)
{
	FocusedActor = NewFocusedActor;
	FocusedOccupantId = NewOccupantId;
	bHasFocusedFacility = IsValid(NewFocusedActor) && !NewOccupantId.IsNone();
	RefreshControlText();
}

void USRFacilityControlWidget::ClearFocusedFacility()
{
	FocusedActor.Reset();
	FocusedOccupantId = NAME_None;
	bHasFocusedFacility = false;
	RefreshControlText();
}

bool USRFacilityControlWidget::HasFocusedFacility() const
{
	return bHasFocusedFacility;
}

bool USRFacilityControlWidget::IsPointerOverControlPanel() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverControlPanel(FSlateApplication::Get().GetCursorPos());
}

bool USRFacilityControlWidget::TryHandleFacilityControlPointerClick()
{
	if (!IsVisible() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D ScreenPosition = FSlateApplication::Get().GetCursorPos();
	if (!IsScreenPositionOverControlPanel(ScreenPosition))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl TryHandleFacilityControlPointerClick Mouse=(%.1f, %.1f)"),
		ScreenPosition.X,
		ScreenPosition.Y);

	if (IsWidgetUnderScreenPosition(CloseButton, ScreenPosition) && CloseButton->GetIsEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved CloseButton"));
		HandleCloseClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(ProcessCheckBox, ScreenPosition) && ProcessCheckBox->GetIsEnabled())
	{
		const bool bNewChecked = !ProcessCheckBox->IsChecked();
		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved ProcessCheckBox bNewChecked=%s"),
			bNewChecked ? TEXT("true") : TEXT("false"));
		HandleProcessCheckStateChanged(bNewChecked);
		return true;
	}

	if (IsWidgetUnderScreenPosition(DeliverCheckBox, ScreenPosition) && DeliverCheckBox->GetIsEnabled())
	{
		const bool bNewChecked = !DeliverCheckBox->IsChecked();
		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DeliverCheckBox bNewChecked=%s"),
			bNewChecked ? TEXT("true") : TEXT("false"));
		HandleDeliverCheckStateChanged(bNewChecked);
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddTerriteButton, ScreenPosition) && DebugAddTerriteButton->GetIsEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddTerriteButton"));
		HandleDebugAddTerriteClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddAquidButton, ScreenPosition) && DebugAddAquidButton->GetIsEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddAquidButton"));
		HandleDebugAddAquidClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddNitainButton, ScreenPosition) && DebugAddNitainButton->GetIsEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddNitainButton"));
		HandleDebugAddNitainClicked();
		return true;
	}

	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click consumed panel background."));
	return true;
}

bool USRFacilityControlWidget::AddDebugInputResourceToPort(int32 InputPortIndex, FName ResourceId)
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone() || InputPortIndex == INDEX_NONE)
	{
		return false;
	}

	FSRResourceInstance ResourceInstance;
	if (ResourceId == FName(TEXT("Territe")))
	{
		ResourceInstance = MakeDebugEnergyResource(TEXT("Territe"), 1.0, 3);
	}
	else if (ResourceId == FName(TEXT("Aquid")))
	{
		ResourceInstance = MakeDebugCatalystResource(TEXT("Aquid"), ESRResourceCatalystOperator::Add);
	}
	else if (ResourceId == FName(TEXT("Nitain")))
	{
		ResourceInstance = MakeDebugCatalystResource(TEXT("Nitain"), ESRResourceCatalystOperator::Multiply);
	}
	else
	{
		return false;
	}

	const bool bAdded = FacilityNetwork->AddInputResourceToPort(FocusedOccupantId, InputPortIndex, ResourceInstance);
	RefreshControlText();
	return bAdded;
}

USRFacilityNetworkComponent* USRFacilityControlWidget::GetFocusedFacilityNetwork() const
{
	AActor* Actor = FocusedActor.Get();
	return IsValid(Actor) ? Actor->FindComponentByClass<USRFacilityNetworkComponent>() : nullptr;
}

bool USRFacilityControlWidget::IsScreenPositionOverControlPanel(const FVector2D& ScreenPosition) const
{
	return IsVisible()
		&& PanelBorder
		&& PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

void USRFacilityControlWidget::HandleCloseClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl CloseButton OnClicked"));

	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->ClearFacilityFocus();
		return;
	}

	ClearFocusedFacility();
}

void USRFacilityControlWidget::HandleProcessCheckStateChanged(bool bIsChecked)
{
	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl ProcessCheckBox changed bIsChecked=%s"),
		bIsChecked ? TEXT("true") : TEXT("false"));

	if (bUpdatingControls)
	{
		return;
	}

	if (USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork())
	{
		FacilityNetwork->SetFacilityProcessEnabled(FocusedOccupantId, bIsChecked);
	}
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDeliverCheckStateChanged(bool bIsChecked)
{
	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DeliverCheckBox changed bIsChecked=%s"),
		bIsChecked ? TEXT("true") : TEXT("false"));

	if (bUpdatingControls)
	{
		return;
	}

	if (USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork())
	{
		FacilityNetwork->SetFacilityDeliverEnabled(FocusedOccupantId, bIsChecked);
	}
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddTerriteClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddTerriteButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugEnergyResource(TEXT("Territe"), 1.0, 3));
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddAquidClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddAquidButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugCatalystResource(TEXT("Aquid"), ESRResourceCatalystOperator::Add));
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddNitainClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddNitainButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugCatalystResource(TEXT("Nitain"), ESRResourceCatalystOperator::Multiply));
	RefreshControlText();
}

void USRFacilityControlWidget::BuildFacilityControlWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		PanelBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("FacilityControlPanelBorder"))));
		TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlTitleTextBlock"))));
		CloseButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlCloseButton"))));
		ProcessCheckBox = Cast<UCheckBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessCheckBox"))));
		ProcessStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessStatusTextBlock"))));
		InputResourceTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputResourceTextBlock"))));
		InputResourceSlotBox = Cast<UVerticalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputResourceSlotBox"))));
		EffectsTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlEffectsTextBlock"))));
		ProcessProgressBar = Cast<UProgressBar>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessProgressBar"))));
		ProcessTimeTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessTimeTextBlock"))));
		OutputPreviewTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputPreviewTextBlock"))));
		OutputResourceSlotBox = Cast<UVerticalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputResourceSlotBox"))));
		InputInventoryTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputInventoryTextBlock"))));
		OutputInventoryTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputInventoryTextBlock"))));
		DebugAddTerriteButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddTerriteButton"))));
		DebugAddAquidButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddAquidButton"))));
		DebugAddNitainButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddNitainButton"))));
		DeliverCheckBox = Cast<UCheckBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDeliverCheckBox"))));
		DeliverStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDeliverStatusTextBlock"))));
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FacilityControlCanvasPanel"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FacilityControlPanelBorder"));
	PanelBorder->SetPadding(FMargin(16.0f));
	PanelBorder->SetBrushColor(FLinearColor(0.025f, 0.032f, 0.040f, 0.96f));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D(0.0f, 18.0f));
		PanelSlot->SetSize(FVector2D(920.0f, 660.0f));
	}

	UCanvasPanel* PanelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FacilityControlPanelCanvas"));
	PanelCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PanelBorder->SetContent(PanelCanvas);

	TitleTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlTitleTextBlock"), 18, FLinearColor::White);
	TitleTextBlock->SetJustification(ETextJustify::Center);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlTitleBorder"),
			TitleTextBlock,
			FLinearColor(0.055f, 0.070f, 0.085f, 0.98f),
			FMargin(12.0f, 8.0f)),
		FVector2D(16.0f, 12.0f),
		FVector2D(816.0f, 56.0f));

	CloseButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlCloseButton"),
		NSLOCTEXT("StarRoversFacilityControl", "CloseButton", "X"));
	CloseButton->SetBackgroundColor(FLinearColor(0.28f, 0.075f, 0.070f, 0.95f));
	AddWidgetToCanvas(
		PanelCanvas,
		CloseButton,
		FVector2D(842.0f, 12.0f),
		FVector2D(40.0f, 56.0f));

	UHorizontalBox* ProcessRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlProcessRow"));
	ProcessCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("FacilityControlProcessCheckBox"));
	if (UHorizontalBoxSlot* CheckSlot = ProcessRow->AddChildToHorizontalBox(ProcessCheckBox))
	{
		CheckSlot->SetPadding(FMargin(0.0f, 3.0f, 10.0f, 0.0f));
		CheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	ProcessStatusTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlProcessStatusTextBlock"), 15, FLinearColor(0.86f, 0.92f, 0.96f, 1.0f));
	ProcessRow->AddChildToHorizontalBox(ProcessStatusTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlProcessBorder"), ProcessRow, FLinearColor(0.07f, 0.085f, 0.105f, 0.98f), FMargin(14.0f)),
		FVector2D(308.0f, 82.0f),
		FVector2D(264.0f, 62.0f));

	UVerticalBox* InputResourceSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlInputResourceSectionBox"));
	InputResourceTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlInputResourceTextBlock"), 13, FLinearColor(0.80f, 0.88f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* InputResourceTitleSlot = InputResourceSectionBox->AddChildToVerticalBox(InputResourceTextBlock))
	{
		InputResourceTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		InputResourceTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	InputResourceSlotBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlInputResourceSlotBox"));
	if (UVerticalBoxSlot* InputResourceSlotBoxSlot = InputResourceSectionBox->AddChildToVerticalBox(InputResourceSlotBox))
	{
		InputResourceSlotBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	EffectsTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlEffectsTextBlock"), 13, FLinearColor(0.96f, 0.90f, 0.72f, 1.0f));
	UVerticalBox* OutputResourceSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlOutputResourceSectionBox"));
	OutputPreviewTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlOutputPreviewTextBlock"), 13, FLinearColor(0.78f, 1.0f, 0.86f, 1.0f));
	if (UVerticalBoxSlot* OutputResourceTitleSlot = OutputResourceSectionBox->AddChildToVerticalBox(OutputPreviewTextBlock))
	{
		OutputResourceTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		OutputResourceTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	OutputResourceSlotBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlOutputResourceSlotBox"));
	if (UVerticalBoxSlot* OutputResourceSlotBoxSlot = OutputResourceSectionBox->AddChildToVerticalBox(OutputResourceSlotBox))
	{
		OutputResourceSlotBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlInputResourceBorder"), InputResourceSectionBox),
		FVector2D(18.0f, 166.0f),
		FVector2D(266.0f, 190.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlEffectsBorder"), EffectsTextBlock, FLinearColor(0.085f, 0.080f, 0.060f, 0.96f)),
		FVector2D(308.0f, 166.0f),
		FVector2D(264.0f, 122.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlOutputPreviewBorder"), OutputResourceSectionBox),
		FVector2D(596.0f, 166.0f),
		FVector2D(266.0f, 190.0f));

	ProcessProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FacilityControlProcessProgressBar"));
	ProcessProgressBar->SetFillColorAndOpacity(FLinearColor(0.40f, 0.72f, 1.0f, 1.0f));
	UVerticalBox* ProgressBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlProgressBox"));
	if (UVerticalBoxSlot* ProgressSlot = ProgressBox->AddChildToVerticalBox(ProcessProgressBar))
	{
		ProgressSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}
	ProcessTimeTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlProcessTimeTextBlock"), 12, FLinearColor(0.82f, 0.84f, 0.86f, 1.0f));
	ProcessTimeTextBlock->SetJustification(ETextJustify::Center);
	ProgressBox->AddChildToVerticalBox(ProcessTimeTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlProgressBorder"),
			ProgressBox,
			FLinearColor(0.060f, 0.073f, 0.088f, 0.96f),
			FMargin(10.0f, 9.0f)),
		FVector2D(332.0f, 306.0f),
		FVector2D(216.0f, 58.0f));

	InputInventoryTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlInputInventoryTextBlock"), 13, FLinearColor(0.82f, 0.88f, 1.0f, 1.0f));
	OutputInventoryTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlOutputInventoryTextBlock"), 13, FLinearColor(0.82f, 1.0f, 0.88f, 1.0f));
	UScrollBox* InputInventoryScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlInputInventoryScrollBox"));
	InputInventoryScrollBox->AddChild(InputInventoryTextBlock);
	UScrollBox* OutputInventoryScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlOutputInventoryScrollBox"));
	OutputInventoryScrollBox->AddChild(OutputInventoryTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlInputInventoryBorder"), InputInventoryScrollBox),
		FVector2D(18.0f, 386.0f),
		FVector2D(390.0f, 154.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlOutputInventoryBorder"), OutputInventoryScrollBox),
		FVector2D(472.0f, 386.0f),
		FVector2D(390.0f, 154.0f));

	UVerticalBox* DebugInputBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlDebugInputBox"));
	UTextBlock* DebugInputLabelTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlDebugInputLabelTextBlock"),
		11,
		FLinearColor(0.92f, 0.82f, 0.64f, 1.0f));
	DebugInputLabelTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "DebugInputLabel", "Debug Input"));
	if (UVerticalBoxSlot* DebugInputLabelSlot = DebugInputBox->AddChildToVerticalBox(DebugInputLabelTextBlock))
	{
		DebugInputLabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}

	UHorizontalBox* DebugInputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlDebugInputRow"));

	DebugAddTerriteButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddTerriteButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddTerrite", "+ Territe"));
	if (UHorizontalBoxSlot* TerriteButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddTerriteButton))
	{
		TerriteButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		TerriteButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	DebugAddAquidButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddAquidButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddAquid", "+ Aquid"));
	if (UHorizontalBoxSlot* AquidButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddAquidButton))
	{
		AquidButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		AquidButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	DebugAddNitainButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddNitainButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddNitain", "+ Nitain"));
	if (UHorizontalBoxSlot* NitainButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddNitainButton))
	{
		NitainButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	DebugInputBox->AddChildToVerticalBox(DebugInputRow);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlDebugInputBorder"),
			DebugInputBox,
			FLinearColor(0.070f, 0.065f, 0.050f, 0.96f),
			FMargin(10.0f, 8.0f)),
		FVector2D(18.0f, 556.0f),
		FVector2D(390.0f, 52.0f));

	UHorizontalBox* DeliverRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlDeliverRow"));
	DeliverCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("FacilityControlDeliverCheckBox"));
	if (UHorizontalBoxSlot* DeliverCheckSlot = DeliverRow->AddChildToHorizontalBox(DeliverCheckBox))
	{
		DeliverCheckSlot->SetPadding(FMargin(0.0f, 3.0f, 10.0f, 0.0f));
		DeliverCheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	DeliverStatusTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlDeliverStatusTextBlock"), 14, FLinearColor(0.86f, 0.92f, 0.96f, 1.0f));
	DeliverRow->AddChildToHorizontalBox(DeliverStatusTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlDeliverBorder"),
			DeliverRow,
			FLinearColor(0.060f, 0.085f, 0.070f, 0.96f),
			FMargin(14.0f)),
		FVector2D(554.0f, 552.0f),
		FVector2D(254.0f, 58.0f));
}

void USRFacilityControlWidget::BindControlHandlers()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleCloseClicked);
	}
	if (ProcessCheckBox)
	{
		ProcessCheckBox->OnCheckStateChanged.RemoveDynamic(this, &USRFacilityControlWidget::HandleProcessCheckStateChanged);
		ProcessCheckBox->OnCheckStateChanged.AddDynamic(this, &USRFacilityControlWidget::HandleProcessCheckStateChanged);
	}
	if (DeliverCheckBox)
	{
		DeliverCheckBox->OnCheckStateChanged.RemoveDynamic(this, &USRFacilityControlWidget::HandleDeliverCheckStateChanged);
		DeliverCheckBox->OnCheckStateChanged.AddDynamic(this, &USRFacilityControlWidget::HandleDeliverCheckStateChanged);
	}
	if (DebugAddTerriteButton)
	{
		DebugAddTerriteButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddTerriteClicked);
		DebugAddTerriteButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddTerriteClicked);
	}
	if (DebugAddAquidButton)
	{
		DebugAddAquidButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddAquidClicked);
		DebugAddAquidButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddAquidClicked);
	}
	if (DebugAddNitainButton)
	{
		DebugAddNitainButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddNitainClicked);
		DebugAddNitainButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddNitainClicked);
	}
}

void USRFacilityControlWidget::RefreshInputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance)
{
	if (!InputResourceSlotBox)
	{
		return;
	}

	TArray<FString> SlotTexts;
	TArray<int32> SlotPortIndices;
	TArray<bool> SlotCanAddResources;
	TArray<bool> SlotShowDebugButtons;
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	const bool bIsMiningFacility = IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
	if (bIsMiningFacility)
	{
		SlotTexts.Add(BuildMiningTargetSummary(FacilityNetwork, FacilityInstance.OccupantId));
		SlotPortIndices.Add(INDEX_NONE);
		SlotCanAddResources.Add(false);
		SlotShowDebugButtons.Add(false);
	}
	else if (FacilityInstance.InputPortInventories.IsEmpty())
	{
		SlotTexts.Add(TEXT("No Input Port\n-\nProcess cannot start"));
		SlotPortIndices.Add(INDEX_NONE);
		SlotCanAddResources.Add(false);
		SlotShowDebugButtons.Add(false);
	}
	else
	{
		for (int32 SlotIndex = 0; SlotIndex < FacilityInstance.InputPortInventories.Num(); ++SlotIndex)
		{
			const FSRFacilityPortInventory& PortInventory = FacilityInstance.InputPortInventories[SlotIndex];
			const FSRResourceInstance* ResourceInstance = nullptr;
			if (FacilityInstance.ProcessingInventory.IsValidIndex(SlotIndex))
			{
				ResourceInstance = &FacilityInstance.ProcessingInventory[SlotIndex];
			}
			else if (!PortInventory.Inventory.IsEmpty())
			{
				ResourceInstance = &PortInventory.Inventory[0];
			}

			const int32 PortCapacity = FMath::Max(1, PortInventory.Capacity);
			const bool bCanAddResource = PortInventory.Inventory.Num() < PortCapacity;
			const FString PortLabel = PortInventory.PortId.IsNone()
				? FString::Printf(TEXT("Input %d"), SlotIndex + 1)
				: PortInventory.PortId.ToString();
			SlotTexts.Add(FString::Printf(
				TEXT("Input %d\n%s (%d/%d)\n%s"),
				SlotIndex + 1,
				*PortLabel,
				PortInventory.Inventory.Num(),
				PortCapacity,
				ResourceInstance ? *BuildCompactResourceSummary(ResourceInstance) : TEXT("Empty")));
			SlotPortIndices.Add(SlotIndex);
			SlotCanAddResources.Add(bCanAddResource);
			SlotShowDebugButtons.Add(true);
		}
	}

	FString NewSignature = FString::Printf(TEXT("Input:%d"), SlotTexts.Num());
	for (int32 SlotIndex = 0; SlotIndex < SlotTexts.Num(); ++SlotIndex)
	{
		NewSignature += TEXT("|");
		NewSignature += SlotTexts[SlotIndex];
		NewSignature += SlotCanAddResources.IsValidIndex(SlotIndex) && SlotCanAddResources[SlotIndex] ? TEXT(":Add") : TEXT(":Full");
		NewSignature += SlotShowDebugButtons.IsValidIndex(SlotIndex) && SlotShowDebugButtons[SlotIndex] ? TEXT(":Debug") : TEXT(":Info");
	}
	if (InputResourcePanelSignature == NewSignature)
	{
		return;
	}

	InputResourcePanelSignature = NewSignature;
	InputResourceSlotBox->ClearChildren();
	InputSlotDebugActions.Reset();
	for (int32 SlotIndex = 0; SlotIndex < SlotTexts.Num(); ++SlotIndex)
	{
		AddInputResourceSlotCard(
			WidgetTree,
			InputResourceSlotBox,
			this,
			InputSlotDebugActions,
			SlotTexts[SlotIndex],
			SlotPortIndices.IsValidIndex(SlotIndex) ? SlotPortIndices[SlotIndex] : INDEX_NONE,
			SlotCanAddResources.IsValidIndex(SlotIndex) && SlotCanAddResources[SlotIndex],
			SlotShowDebugButtons.IsValidIndex(SlotIndex) && SlotShowDebugButtons[SlotIndex]);
	}
}

void USRFacilityControlWidget::RefreshOutputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance)
{
	if (!OutputResourceSlotBox)
	{
		return;
	}

	TArray<FSRResourceInstance> PreviewOutputs;
	if (IsValid(FacilityNetwork))
	{
		FSRResourceInstance PrimaryOutput;
		TArray<FSRResourceInstance> AdditionalOutputs;
		int32 OutputCount = 0;
		if (FacilityNetwork->GetFacilityOutputPreview(FocusedOccupantId, PrimaryOutput, AdditionalOutputs, OutputCount))
		{
			const int32 PrimaryOutputCount = FMath::Max(0, OutputCount);
			PreviewOutputs.Reserve(PrimaryOutputCount + AdditionalOutputs.Num());
			for (int32 OutputIndex = 0; OutputIndex < PrimaryOutputCount; ++OutputIndex)
			{
				PreviewOutputs.Add(PrimaryOutput);
			}
			PreviewOutputs.Append(AdditionalOutputs);
		}
	}

	TArray<FString> SlotTexts;
	const int32 OutputSlotCount = FMath::Max(FacilityInstance.OutputPortInventories.Num(), PreviewOutputs.Num());
	if (OutputSlotCount <= 0)
	{
		SlotTexts.Add(TEXT("No Output Port\n-\nProcess cannot complete"));
	}
	else
	{
		for (int32 SlotIndex = 0; SlotIndex < OutputSlotCount; ++SlotIndex)
		{
			const FSRFacilityPortInventory* PortInventory = FacilityInstance.OutputPortInventories.IsValidIndex(SlotIndex)
				? &FacilityInstance.OutputPortInventories[SlotIndex]
				: nullptr;
			const FSRResourceInstance* PreviewResource = PreviewOutputs.IsValidIndex(SlotIndex)
				? &PreviewOutputs[SlotIndex]
				: nullptr;
			SlotTexts.Add(BuildResourceSlotText(
				TEXT("Output"),
				SlotIndex,
				PortInventory ? PortInventory->PortId : NAME_None,
				PreviewResource,
				PreviewOutputs.IsEmpty() ? TEXT("No Preview") : TEXT("Empty")));
		}
	}

	FString NewSignature = FString::Printf(TEXT("Output:%d"), SlotTexts.Num());
	for (const FString& SlotText : SlotTexts)
	{
		NewSignature += TEXT("|");
		NewSignature += SlotText;
	}
	if (OutputResourcePanelSignature == NewSignature)
	{
		return;
	}

	OutputResourcePanelSignature = NewSignature;
	OutputResourceSlotBox->ClearChildren();
	for (const FString& SlotText : SlotTexts)
	{
		AddResourceSlotCard(
			WidgetTree,
			OutputResourceSlotBox,
			SlotText,
			FLinearColor(0.84f, 1.0f, 0.90f, 1.0f),
			FLinearColor(0.065f, 0.120f, 0.090f, 0.98f));
	}
}

void USRFacilityControlWidget::RefreshControlText()
{
	FSRFacilityInstance FacilityInstance;
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	const bool bHasFacility = bHasFocusedFacility
		&& IsValid(FacilityNetwork)
		&& FacilityNetwork->GetFacilityInstance(FocusedOccupantId, FacilityInstance);

	if (!bHasFacility)
	{
		if (TitleTextBlock)
		{
			TitleTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "NoFacility", "No facility selected"));
		}
		if (InputResourceSlotBox)
		{
			InputResourceSlotBox->ClearChildren();
			InputSlotDebugActions.Reset();
			InputResourcePanelSignature.Reset();
		}
		if (OutputResourceSlotBox)
		{
			OutputResourceSlotBox->ClearChildren();
			OutputResourcePanelSignature.Reset();
		}
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	const FString FacilityName = IsValid(FacilityDataAsset) && !FacilityDataAsset->DisplayName.IsEmpty()
		? FacilityDataAsset->DisplayName.ToString()
		: FocusedOccupantId.ToString();
	const float ProcessSeconds = ResolveProcessSeconds(FacilityInstance);
	const float ProgressRatio = ProcessSeconds > 0.0f
		? FMath::Clamp(FacilityInstance.ProcessProgressSeconds / ProcessSeconds, 0.0f, 1.0f)
		: 0.0f;

	FString ProcessReason;
	bool bCanToggleProcess = CanToggleProcess(FacilityInstance, ProcessReason);
	const bool bHasOutputConveyor = FacilityNetwork->HasConnectedConveyorForFacilityPort(FocusedOccupantId, ESRFacilityPortKind::Output);
	const bool bCanDebugAddInput = HasAvailableInputPortCapacity(FacilityInstance);
	const bool bIsMiningFacility = IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
	FSRResourceDepositInstance MiningTarget;
	const bool bHasMiningTarget = bIsMiningFacility && FacilityNetwork->GetFacilityMiningTarget(FocusedOccupantId, MiningTarget);
	if (bIsMiningFacility && !bHasMiningTarget)
	{
		bCanToggleProcess = false;
		ProcessReason = TEXT("No adjacent deposit");
	}

	bUpdatingControls = true;
	if (ProcessCheckBox)
	{
		ProcessCheckBox->SetIsChecked(FacilityInstance.bProcessEnabled);
		ProcessCheckBox->SetIsEnabled(bCanToggleProcess);
	}
	if (DeliverCheckBox)
	{
		DeliverCheckBox->SetIsChecked(FacilityInstance.bDeliverEnabled);
		DeliverCheckBox->SetIsEnabled(bHasOutputConveyor);
	}
	if (DebugAddTerriteButton)
	{
		DebugAddTerriteButton->SetIsEnabled(bCanDebugAddInput);
	}
	if (DebugAddAquidButton)
	{
		DebugAddAquidButton->SetIsEnabled(bCanDebugAddInput);
	}
	if (DebugAddNitainButton)
	{
		DebugAddNitainButton->SetIsEnabled(bCanDebugAddInput);
	}
	bUpdatingControls = false;

	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("%s\nOccupant: %s  Temp: %s"),
			*FacilityName,
			*FocusedOccupantId.ToString(),
			GetFacilityTemperatureLabel(FacilityInstance.TemperatureState))));
	}
	if (ProcessStatusTextBlock)
	{
		ProcessStatusTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Process: %s  %s"),
			FacilityInstance.bProcessEnabled ? TEXT("ON") : TEXT("OFF"),
			*ProcessReason)));
	}
	if (InputResourceTextBlock)
	{
		InputResourceTextBlock->SetText(FText::FromString(
			bIsMiningFacility
				? FString::Printf(TEXT("Mining Target (%s)"), bHasMiningTarget ? TEXT("Ready") : TEXT("None"))
				: FString::Printf(TEXT("Input Resource (%d)"), FacilityInstance.InputPortInventories.Num())));
	}
	RefreshInputResourceSlots(FacilityNetwork, FacilityInstance);
	if (EffectsTextBlock)
	{
		EffectsTextBlock->SetText(FText::FromString(BuildEffectsSummary(FacilityDataAsset)));
	}
	if (ProcessProgressBar)
	{
		ProcessProgressBar->SetPercent(ProgressRatio);
	}
	if (ProcessTimeTextBlock)
	{
		ProcessTimeTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Process Time: %.2f / %.2f sec"),
			FacilityInstance.ProcessProgressSeconds,
			ProcessSeconds)));
	}
	if (OutputPreviewTextBlock)
	{
		OutputPreviewTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Output Resource (%d)"),
			FacilityInstance.OutputPortInventories.Num())));
	}
	RefreshOutputResourceSlots(FacilityNetwork, FacilityInstance);
	if (InputInventoryTextBlock)
	{
		InputInventoryTextBlock->SetText(FText::FromString(
			bIsMiningFacility
				? BuildMiningTargetSummary(FacilityNetwork, FocusedOccupantId)
				: BuildPortInventorySummary(TEXT("Input Inventory"), FacilityInstance.InputPortInventories)));
	}
	if (OutputInventoryTextBlock)
	{
		OutputInventoryTextBlock->SetText(FText::FromString(BuildPortInventorySummary(TEXT("Output Inventory"), FacilityInstance.OutputPortInventories)));
	}
	if (DeliverStatusTextBlock)
	{
		DeliverStatusTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Deliver: %s  %s"),
			FacilityInstance.bDeliverEnabled ? TEXT("ON") : TEXT("OFF"),
			bHasOutputConveyor ? TEXT("Output conveyor connected") : TEXT("No output conveyor"))));
	}
}
