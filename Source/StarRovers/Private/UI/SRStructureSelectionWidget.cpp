#include "UI/SRStructureSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Structure/SRStructureDataAsset.h"
#include "Styling/SlateColor.h"

namespace
{
	constexpr int32 StructureCategoryButtonCount = 4;
	constexpr int32 FacilityButtonCount = 5;
	constexpr int32 StructureTabCount = 4;
	constexpr int32 StructureCategoryIconTextureSize = 64;
	constexpr float DefaultCategoryButtonLength = 48.0f;
	constexpr float DefaultFacilityButtonLength = 72.0f;

	FText GetBuildOptionDisplayName(const FSRStructureBuildOption& BuildOption)
	{
		return BuildOption.DisplayName.IsEmpty()
			? FText::FromName(BuildOption.StructureId)
			: BuildOption.DisplayName;
	}

	FText MakeSelectedStructureText(const FSRStructureBuildOption* SelectedBuildOption)
	{
		return SelectedBuildOption
			? FText::Format(
				FTextFormat(NSLOCTEXT("StarRoversStructureSelection", "SelectedStructureFormat", "Selected: {0}")),
				GetBuildOptionDisplayName(*SelectedBuildOption))
			: NSLOCTEXT("StarRoversStructureSelection", "NoSelectedStructure", "Select a structure");
	}

	bool IsInsideStructureCategoryIconShape(int32 CategoryIndex, int32 X, int32 Y)
	{
		const float NormalizedX = (static_cast<float>(X) + 0.5f) / static_cast<float>(StructureCategoryIconTextureSize);
		const float NormalizedY = (static_cast<float>(Y) + 0.5f) / static_cast<float>(StructureCategoryIconTextureSize);
		const float CenterX = NormalizedX - 0.5f;
		const float CenterY = NormalizedY - 0.5f;
		const float DistanceFromCenter = FMath::Sqrt(CenterX * CenterX + CenterY * CenterY);

		switch (CategoryIndex)
		{
		case 0:
			return (NormalizedX >= 0.18f && NormalizedX <= 0.82f && NormalizedY >= 0.57f && NormalizedY <= 0.67f)
				|| (FMath::Abs(DistanceFromCenter - 0.24f) <= 0.025f && NormalizedY >= 0.35f)
				|| (FMath::Abs(NormalizedX - 0.32f) <= 0.035f && NormalizedY >= 0.35f && NormalizedY <= 0.72f)
				|| (FMath::Abs(NormalizedX - 0.68f) <= 0.035f && NormalizedY >= 0.35f && NormalizedY <= 0.72f);
		case 1:
			return (NormalizedX >= 0.45f && NormalizedX <= 0.55f && NormalizedY >= 0.20f && NormalizedY <= 0.80f)
				|| (NormalizedX >= 0.28f && NormalizedX <= 0.72f && NormalizedY >= 0.74f && NormalizedY <= 0.84f)
				|| (NormalizedY >= 0.26f && NormalizedY <= 0.38f && NormalizedX >= 0.28f && NormalizedX <= 0.72f)
				|| (FMath::Abs((NormalizedX - 0.68f) - (NormalizedY - 0.28f)) <= 0.04f && NormalizedX >= 0.56f && NormalizedY >= 0.28f && NormalizedY <= 0.70f);
		case 2:
			return ((NormalizedX >= 0.23f && NormalizedX <= 0.77f)
					&& (NormalizedY >= 0.35f && NormalizedY <= 0.78f)
					&& (NormalizedX <= 0.30f || NormalizedX >= 0.70f || NormalizedY <= 0.42f || NormalizedY >= 0.71f))
				|| (NormalizedX >= 0.36f && NormalizedX <= 0.64f && NormalizedY >= 0.18f && NormalizedY <= 0.34f)
				|| (NormalizedX >= 0.43f && NormalizedX <= 0.57f && NormalizedY >= 0.12f && NormalizedY <= 0.24f);
		case 3:
			return (NormalizedX >= 0.44f && NormalizedX <= 0.56f && NormalizedY >= 0.16f && NormalizedY <= 0.40f)
				|| (NormalizedY >= 0.38f && NormalizedY <= 0.78f
					&& NormalizedX >= 0.24f + (0.38f - FMath::Min(NormalizedY, 0.60f)) * 0.25f
					&& NormalizedX <= 0.76f - (0.38f - FMath::Min(NormalizedY, 0.60f)) * 0.25f
					&& (NormalizedX <= 0.31f || NormalizedX >= 0.69f || NormalizedY >= 0.70f))
				|| (NormalizedX >= 0.36f && NormalizedX <= 0.64f && NormalizedY >= 0.13f && NormalizedY <= 0.20f);
		default:
			return DistanceFromCenter <= 0.36f && DistanceFromCenter >= 0.26f;
		}
	}

	UTexture2D* CreateStructureCategoryIconTexture(int32 CategoryIndex, const FName TextureName)
	{
		UTexture2D* IconTexture = UTexture2D::CreateTransient(
			StructureCategoryIconTextureSize,
			StructureCategoryIconTextureSize,
			PF_B8G8R8A8,
			TextureName);
		if (!IconTexture)
		{
			return nullptr;
		}

		TArray<FColor> Pixels;
		Pixels.SetNumZeroed(StructureCategoryIconTextureSize * StructureCategoryIconTextureSize);
		for (int32 Y = 0; Y < StructureCategoryIconTextureSize; ++Y)
		{
			for (int32 X = 0; X < StructureCategoryIconTextureSize; ++X)
			{
				Pixels[Y * StructureCategoryIconTextureSize + X] = IsInsideStructureCategoryIconShape(CategoryIndex, X, Y)
					? FColor(248, 250, 252, 255)
					: FColor(0, 0, 0, 0);
			}
		}

		FTexturePlatformData* PlatformData = IconTexture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			return IconTexture;
		}

		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (TextureData)
		{
			FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
		}
		Mip.BulkData.Unlock();

		IconTexture->Filter = TF_Bilinear;
		IconTexture->SRGB = true;
		IconTexture->UpdateResource();
		return IconTexture;
	}

	UTexture2D* CreateStructureTabIndicatorTexture(const FName TextureName)
	{
		UTexture2D* IndicatorTexture = UTexture2D::CreateTransient(
			StructureCategoryIconTextureSize,
			StructureCategoryIconTextureSize,
			PF_B8G8R8A8,
			TextureName);
		if (!IndicatorTexture)
		{
			return nullptr;
		}

		TArray<FColor> Pixels;
		Pixels.SetNumZeroed(StructureCategoryIconTextureSize * StructureCategoryIconTextureSize);
		for (int32 Y = 0; Y < StructureCategoryIconTextureSize; ++Y)
		{
			for (int32 X = 0; X < StructureCategoryIconTextureSize; ++X)
			{
				const float NormalizedX = (static_cast<float>(X) + 0.5f) / static_cast<float>(StructureCategoryIconTextureSize);
				const float NormalizedY = (static_cast<float>(Y) + 0.5f) / static_cast<float>(StructureCategoryIconTextureSize);
				const float CenterX = NormalizedX - 0.5f;
				const float CenterY = NormalizedY - 0.5f;
				const float DistanceFromCenter = FMath::Sqrt(CenterX * CenterX + CenterY * CenterY);
				Pixels[Y * StructureCategoryIconTextureSize + X] = DistanceFromCenter <= 0.43f
					? FColor(255, 255, 255, 255)
					: FColor(0, 0, 0, 0);
			}
		}

		FTexturePlatformData* PlatformData = IndicatorTexture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			return IndicatorTexture;
		}

		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (TextureData)
		{
			FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
		}
		Mip.BulkData.Unlock();

		IndicatorTexture->Filter = TF_Bilinear;
		IndicatorTexture->SRGB = true;
		IndicatorTexture->UpdateResource();
		return IndicatorTexture;
	}

	void SetCategoryButtonImageBrush(UImage* TargetImage, const FSlateBrush& ConfiguredBrush, UTexture2D* DefaultTexture)
	{
		if (!TargetImage)
		{
			return;
		}

		if (ConfiguredBrush.GetResourceObject())
		{
			TargetImage->SetBrush(ConfiguredBrush);
		}
		else if (DefaultTexture)
		{
			TargetImage->SetBrushFromTexture(DefaultTexture, true);
		}

		TargetImage->SetColorAndOpacity(FLinearColor::White);
	}
}

void USRStructureSelectionEntryAction::Initialize(USRStructureSelectionWidget* InOwnerWidget, FName InStructureId)
{
	OwnerWidget = InOwnerWidget;
	StructureId = InStructureId;
}

void USRStructureSelectionEntryAction::HandleClicked()
{
	if (IsValid(OwnerWidget))
	{
		OwnerWidget->DispatchBuildOptionSelected(StructureId);
	}
}

TSharedRef<SWidget> USRStructureSelectionWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildStructureSelectionWidgetTree();
	return Super::RebuildWidget();
}

void USRStructureSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildStructureSelectionWidgetTree();
	EnsureDefaultCategoryIconTextures();
	EnsureDefaultStructureTabIndicatorTexture();
	ApplyCategoryButtonIconBrushes();
	RefreshStructureTabIndicatorBrushes();
	RebuildCategorizedBuildOptions();
	RefreshFacilityButtonLabels();
	RefreshCategoryButtonStyles();
	SyncCategoryBarLayout();
	SyncFacilityButtonBarLayout();
	SyncStructureTabBarLayout();
	RefreshFacilityButtonBarVisibility();
	RefreshFacilityButtonStyles();
	RefreshStructureTabIndicatorStyles();
	RebuildBuildOptions();
	RefreshSelectedStructureText();
}

void USRStructureSelectionWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildStructureSelectionWidgetTree();
	EnsureDefaultCategoryIconTextures();
	EnsureDefaultStructureTabIndicatorTexture();
	ApplyCategoryButtonIconBrushes();
	RefreshStructureTabIndicatorBrushes();
	RebuildCategorizedBuildOptions();
	RefreshFacilityButtonLabels();
	RefreshCategoryButtonStyles();
	SyncCategoryBarLayout();
	SyncFacilityButtonBarLayout();
	SyncStructureTabBarLayout();
	RefreshFacilityButtonBarVisibility();
	RefreshFacilityButtonStyles();
	RefreshStructureTabIndicatorStyles();
	RebuildBuildOptions();
	RefreshSelectedStructureText();
}

void USRStructureSelectionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible())
	{
		return;
	}

	SyncCategoryBarLayout();
	SyncFacilityButtonBarLayout();
	SyncStructureTabBarLayout();
	RefreshPointerHoverState();
}

FReply USRStructureSelectionWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverStructureSelectionPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRStructureSelectionWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverStructureSelectionPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRStructureSelectionWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverStructureSelectionPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRStructureSelectionWidget::SetBuildOptions(const TArray<FSRStructureBuildOption>& NewBuildOptions)
{
	BuildOptions = NewBuildOptions;
	if (bHasSelectedStructureId && !FindBuildOption(SelectedStructureId))
	{
		ClearSelectedStructureId();
	}

	RebuildCategorizedBuildOptions();
	RebuildBuildOptions();
	RefreshCategoryButtonStyles();
	RefreshFacilityButtonLabels();
	RefreshFacilityButtonStyles();
	RefreshSelectedStructureText();
}

void USRStructureSelectionWidget::SetBuildOptionsFromDataAssets(const TArray<USRStructureDataAsset*>& StructureDataAssets)
{
	TArray<FSRStructureBuildOption> NewBuildOptions;
	NewBuildOptions.Reserve(StructureDataAssets.Num());

	for (USRStructureDataAsset* StructureDataAsset : StructureDataAssets)
	{
		if (!IsValid(StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.StructureId.IsNone())
		{
			continue;
		}

		FSRStructureBuildOption BuildOption;
		BuildOption.StructureId = StructureData.StructureId;
		BuildOption.DisplayName = StructureData.DisplayName;
		BuildOption.Description = StructureData.Description;
		BuildOption.StructureDataAsset = StructureDataAsset;
		BuildOption.bEnabled = StructureData.bAvailableForConstruction;
		NewBuildOptions.Add(BuildOption);
	}

	SetBuildOptions(NewBuildOptions);
}

TArray<FSRStructureBuildOption> USRStructureSelectionWidget::GetBuildOptions() const
{
	return BuildOptions;
}

void USRStructureSelectionWidget::SetSelectedStructureId(FName NewSelectedStructureId)
{
	const FSRStructureBuildOption* BuildOption = FindBuildOption(NewSelectedStructureId);
	if (!BuildOption || !BuildOption->bEnabled)
	{
		return;
	}

	SelectedStructureId = NewSelectedStructureId;
	bHasSelectedStructureId = !SelectedStructureId.IsNone();
	RebuildBuildOptions();
	RefreshCategoryButtonStyles();
	RefreshFacilityButtonStyles();
	RefreshSelectedStructureText();
	OnSelectedStructureChanged(SelectedStructureId, bHasSelectedStructureId);
}

void USRStructureSelectionWidget::ClearSelectedStructureId()
{
	if (!bHasSelectedStructureId && SelectedStructureId.IsNone())
	{
		return;
	}

	SelectedStructureId = NAME_None;
	bHasSelectedStructureId = false;
	RebuildBuildOptions();
	RefreshFacilityButtonStyles();
	RefreshSelectedStructureText();
	OnSelectedStructureChanged(SelectedStructureId, bHasSelectedStructureId);
}

bool USRStructureSelectionWidget::HasSelectedStructureId() const
{
	return bHasSelectedStructureId;
}

FName USRStructureSelectionWidget::GetSelectedStructureId() const
{
	return SelectedStructureId;
}

USRStructureDataAsset* USRStructureSelectionWidget::GetSelectedStructureDataAsset() const
{
	const FSRStructureBuildOption* BuildOption = bHasSelectedStructureId ? FindBuildOption(SelectedStructureId) : nullptr;
	return BuildOption ? BuildOption->StructureDataAsset.Get() : nullptr;
}

bool USRStructureSelectionWidget::IsPointerOverStructureSelectionPanel() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverStructureSelectionPanel(FSlateApplication::Get().GetCursorPos());
}

void USRStructureSelectionWidget::SetStructureCategoryButtonIconBrush(int32 CategoryIndex, const FSlateBrush& IconBrush)
{
	switch (CategoryIndex)
	{
	case 0:
		Category1IconBrush = IconBrush;
		break;
	case 1:
		Category2IconBrush = IconBrush;
		break;
	case 2:
		Category3IconBrush = IconBrush;
		break;
	case 3:
		Category4IconBrush = IconBrush;
		break;
	default:
		return;
	}

	ApplyCategoryButtonIconBrushes();
}

bool USRStructureSelectionWidget::TryHandleStructureSelectionPointerClick()
{
	if (!IsVisible() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D ScreenPosition = FSlateApplication::Get().GetCursorPos();
	const int32 FacilityButtonIndex = FindFacilityButtonIndexAtScreenPosition(ScreenPosition);
	if (FacilityButtonIndex != INDEX_NONE)
	{
		SelectFacilityButton(FacilityButtonIndex);
		return true;
	}

	const int32 CategoryIndex = FindCategoryButtonIndexAtScreenPosition(ScreenPosition);
	if (CategoryIndex != INDEX_NONE)
	{
		SelectStructureCategory(CategoryIndex);
		return true;
	}

	return IsScreenPositionOverStructureSelectionPanel(ScreenPosition);
}

bool USRStructureSelectionWidget::AdvanceStructureSelectionTab()
{
	if (!IsVisible() || SelectedCategoryIndex != 2 && SelectedCategoryIndex != 3)
	{
		return false;
	}

	SetStructureSelectionTabIndex((SelectedStructureTabIndex + 1) % StructureTabCount);
	return true;
}

void USRStructureSelectionWidget::DispatchBuildOptionSelected(FName StructureId)
{
	const FSRStructureBuildOption* RequestedBuildOption = FindBuildOption(StructureId);
	if (!RequestedBuildOption || !RequestedBuildOption->bEnabled)
	{
		return;
	}

	SetSelectedStructureId(StructureId);
	if (bHasSelectedStructureId && SelectedStructureId == StructureId)
	{
		BuildOptionSelectedEvent.Broadcast(SelectedStructureId, GetSelectedStructureDataAsset());
	}
}

FSRStarRoversStructureBuildOptionSelectedSignature& USRStructureSelectionWidget::OnBuildOptionSelected()
{
	return BuildOptionSelectedEvent;
}

void USRStructureSelectionWidget::BuildStructureSelectionWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		CategoryBarBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("StructureCategoryBarBorder"))));
		StructureSelectionBorder = CategoryBarBorder;
		CategoryButtonRowBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("StructureCategoryButtonRowBox"))));
		FacilityButtonBarBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("StructureFacilityButtonBarBorder"))));
		FacilityButtonRowBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("StructureFacilityButtonRowBox"))));
		StructureTabBarBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("StructureTabBarBorder"))));
		StructureTabIndicatorRowBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("StructureTabIndicatorRowBox"))));

		StructureSelectionVerticalBox = nullptr;
		TitleTextBlock = nullptr;
		SelectedStructureTextBlock = nullptr;
		BuildOptionsScrollBox = nullptr;

		CategoryButtonSizeBoxes.Reset();
		CategoryButtonGapSizeBoxes.Reset();
		CategoryButtons.Reset();
		CategoryButtonIconSizeBoxes.Reset();
		CategoryButtonImages.Reset();
		CategoryButtonLabelSizeBoxes.Reset();
		CategoryButtonLabelTextBlocks.Reset();
		FacilityButtonSizeBoxes.Reset();
		FacilityButtonGapSizeBoxes.Reset();
		FacilityButtons.Reset();
		FacilityButtonTextBlocks.Reset();
		StructureTabIndicatorSizeBoxes.Reset();
		StructureTabIndicatorImages.Reset();

		for (int32 ButtonIndex = 0; ButtonIndex < StructureCategoryButtonCount; ++ButtonIndex)
		{
			const int32 DisplayIndex = ButtonIndex + 1;
			CategoryButtonSizeBoxes.Add(Cast<USizeBox>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureCategoryButtonSizeBox%d"), DisplayIndex)))));
			CategoryButtons.Add(Cast<UButton>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureCategoryButton%d"), DisplayIndex)))));
			CategoryButtonIconSizeBoxes.Add(Cast<USizeBox>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureCategoryButtonIconSizeBox%d"), DisplayIndex)))));
			CategoryButtonImages.Add(Cast<UImage>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureCategoryButtonImage%d"), DisplayIndex)))));
			CategoryButtonLabelSizeBoxes.Add(Cast<USizeBox>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureCategoryButtonLabelSizeBox%d"), DisplayIndex)))));
			CategoryButtonLabelTextBlocks.Add(Cast<UTextBlock>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureCategoryButtonLabel%d"), DisplayIndex)))));
			if (ButtonIndex > 0)
			{
				CategoryButtonGapSizeBoxes.Add(Cast<USizeBox>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureCategoryButtonGapSizeBox%d"), ButtonIndex)))));
			}
		}

		for (int32 ButtonIndex = 0; ButtonIndex < FacilityButtonCount; ++ButtonIndex)
		{
			const int32 DisplayIndex = ButtonIndex + 1;
			FacilityButtonSizeBoxes.Add(Cast<USizeBox>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButtonSizeBox%d"), DisplayIndex)))));
			FacilityButtons.Add(Cast<UButton>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButton%d"), DisplayIndex)))));
			FacilityButtonTextBlocks.Add(Cast<UTextBlock>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButtonLabel%d"), DisplayIndex)))));
			if (ButtonIndex > 0)
			{
				FacilityButtonGapSizeBoxes.Add(Cast<USizeBox>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButtonGapSizeBox%d"), ButtonIndex)))));
			}
		}

		for (int32 TabIndex = 0; TabIndex < StructureTabCount; ++TabIndex)
		{
			const int32 DisplayIndex = TabIndex + 1;
			StructureTabIndicatorSizeBoxes.Add(Cast<USizeBox>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureTabIndicatorSizeBox%d"), DisplayIndex)))));
			StructureTabIndicatorImages.Add(Cast<UImage>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureTabIndicatorImage%d"), DisplayIndex)))));
		}

		bool bHasRequiredCategoryButtons = CategoryBarBorder
			&& CategoryButtonRowBox
			&& CategoryButtons.Num() == StructureCategoryButtonCount
			&& CategoryButtonGapSizeBoxes.Num() == StructureCategoryButtonCount - 1;
		for (UButton* CategoryButton : CategoryButtons)
		{
			bHasRequiredCategoryButtons = bHasRequiredCategoryButtons && IsValid(CategoryButton);
		}
		for (USizeBox* CategoryButtonGapSizeBox : CategoryButtonGapSizeBoxes)
		{
			bHasRequiredCategoryButtons = bHasRequiredCategoryButtons && IsValid(CategoryButtonGapSizeBox);
		}

		bool bHasRequiredFacilityButtons = FacilityButtonBarBorder
			&& FacilityButtonRowBox
			&& FacilityButtons.Num() == FacilityButtonCount
			&& FacilityButtonGapSizeBoxes.Num() == FacilityButtonCount - 1;
		for (int32 ButtonIndex = 0; ButtonIndex < FacilityButtons.Num(); ++ButtonIndex)
		{
			bHasRequiredFacilityButtons = bHasRequiredFacilityButtons
				&& IsValid(FacilityButtons[ButtonIndex])
				&& FacilityButtonTextBlocks.IsValidIndex(ButtonIndex)
				&& IsValid(FacilityButtonTextBlocks[ButtonIndex]);
		}
		for (USizeBox* FacilityButtonGapSizeBox : FacilityButtonGapSizeBoxes)
		{
			bHasRequiredFacilityButtons = bHasRequiredFacilityButtons && IsValid(FacilityButtonGapSizeBox);
		}

		bool bHasRequiredTabIndicators = StructureTabBarBorder && StructureTabIndicatorRowBox && StructureTabIndicatorImages.Num() == StructureTabCount;
		for (UImage* TabIndicatorImage : StructureTabIndicatorImages)
		{
			bHasRequiredTabIndicators = bHasRequiredTabIndicators && IsValid(TabIndicatorImage);
		}

		if (bHasRequiredCategoryButtons && bHasRequiredFacilityButtons && bHasRequiredTabIndicators)
		{
			for (int32 ButtonIndex = 0; ButtonIndex < CategoryButtons.Num(); ++ButtonIndex)
			{
				UButton* CategoryButton = CategoryButtons[ButtonIndex];
				if (!CategoryButton)
				{
					continue;
				}

				CategoryButton->OnClicked.RemoveAll(this);
				CategoryButton->OnHovered.RemoveAll(this);
				CategoryButton->OnUnhovered.RemoveAll(this);
				if (ButtonIndex == 0)
				{
					CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory1Clicked);
					CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory1Hovered);
				}
				else if (ButtonIndex == 1)
				{
					CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory2Clicked);
					CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory2Hovered);
				}
				else if (ButtonIndex == 2)
				{
					CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory3Clicked);
					CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory3Hovered);
				}
				else if (ButtonIndex == 3)
				{
					CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory4Clicked);
					CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory4Hovered);
				}
				CategoryButton->OnUnhovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategoryUnhovered);
			}

			for (int32 ButtonIndex = 0; ButtonIndex < FacilityButtons.Num(); ++ButtonIndex)
			{
				UButton* FacilityButton = FacilityButtons[ButtonIndex];
				if (!FacilityButton)
				{
					continue;
				}

				FacilityButton->OnClicked.RemoveAll(this);
				FacilityButton->OnHovered.RemoveAll(this);
				FacilityButton->OnUnhovered.RemoveAll(this);
				if (ButtonIndex == 0)
				{
					FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton1Clicked);
					FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton1Hovered);
				}
				else if (ButtonIndex == 1)
				{
					FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton2Clicked);
					FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton2Hovered);
				}
				else if (ButtonIndex == 2)
				{
					FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton3Clicked);
					FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton3Hovered);
				}
				else if (ButtonIndex == 3)
				{
					FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton4Clicked);
					FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton4Hovered);
				}
				else if (ButtonIndex == 4)
				{
					FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton5Clicked);
					FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton5Hovered);
				}
				FacilityButton->OnUnhovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButtonUnhovered);
			}

			return;
		}

		WidgetTree->RootWidget = nullptr;
		StructureSelectionBorder = nullptr;
		CategoryBarBorder = nullptr;
		CategoryButtonRowBox = nullptr;
		FacilityButtonBarBorder = nullptr;
		FacilityButtonRowBox = nullptr;
		StructureTabBarBorder = nullptr;
		StructureTabIndicatorRowBox = nullptr;
		CategoryButtonSizeBoxes.Reset();
		CategoryButtonGapSizeBoxes.Reset();
		CategoryButtons.Reset();
		CategoryButtonIconSizeBoxes.Reset();
		CategoryButtonImages.Reset();
		CategoryButtonLabelSizeBoxes.Reset();
		CategoryButtonLabelTextBlocks.Reset();
		FacilityButtonSizeBoxes.Reset();
		FacilityButtonGapSizeBoxes.Reset();
		FacilityButtons.Reset();
		FacilityButtonTextBlocks.Reset();
		StructureTabIndicatorSizeBoxes.Reset();
		StructureTabIndicatorImages.Reset();
	}

	UCanvasPanel* StructureSelectionCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("StructureSelectionCanvasPanel"));
	WidgetTree->RootWidget = StructureSelectionCanvasPanel;
	StructureSelectionCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	StructureTabBarBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("StructureTabBarBorder"));
	StructureTabBarBorder->SetPadding(FMargin(0.0f));
	StructureTabBarBorder->SetBrushColor(StructureTabBarColor);
	StructureTabBarBorder->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* CanvasSlot = StructureSelectionCanvasPanel->AddChildToCanvas(StructureTabBarBorder))
	{
		const float ClampedWidthRatio = FMath::Clamp(StructureTabBarWidthViewportRatio, 0.0f, 1.0f);
		const float ClampedCategoryHeightRatio = FMath::Clamp(CategoryBarHeightViewportRatio, 0.0f, 1.0f);
		const float ClampedFacilityHeightRatio = FMath::Clamp(FacilityButtonBarHeightViewportRatio, 0.0f, 1.0f);
		const float ClampedTabHeightRatio = FMath::Clamp(StructureTabBarHeightViewportRatio, 0.0f, 1.0f);
		const float LeftAnchor = (1.0f - ClampedWidthRatio) * 0.5f;
		const float RightAnchor = 1.0f - LeftAnchor;
		const float BottomAnchor = 1.0f - ClampedCategoryHeightRatio - ClampedFacilityHeightRatio;
		const float TopAnchor = FMath::Clamp(BottomAnchor - ClampedTabHeightRatio, 0.0f, 1.0f);
		CanvasSlot->SetAnchors(FAnchors(LeftAnchor, TopAnchor, RightAnchor, FMath::Clamp(BottomAnchor, 0.0f, 1.0f)));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}

	StructureTabIndicatorRowBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("StructureTabIndicatorRowBox"));
	StructureTabBarBorder->SetContent(StructureTabIndicatorRowBox);

	auto AddTabEqualSpacer = [this](const TCHAR* SpacerName)
	{
		if (!WidgetTree || !StructureTabIndicatorRowBox)
		{
			return;
		}

		USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), SpacerName);
		if (UHorizontalBoxSlot* SpacerSlot = StructureTabIndicatorRowBox->AddChildToHorizontalBox(Spacer))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};

	StructureTabIndicatorSizeBoxes.Reset();
	StructureTabIndicatorImages.Reset();

	AddTabEqualSpacer(TEXT("StructureTabIndicatorSpacer0"));
	for (int32 TabIndex = 0; TabIndex < StructureTabCount; ++TabIndex)
	{
		const int32 DisplayIndex = TabIndex + 1;

		USizeBox* TabIndicatorSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureTabIndicatorSizeBox%d"), DisplayIndex)));
		TabIndicatorSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		StructureTabIndicatorSizeBoxes.Add(TabIndicatorSizeBox);

		UImage* TabIndicatorImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			FName(*FString::Printf(TEXT("StructureTabIndicatorImage%d"), DisplayIndex)));
		TabIndicatorImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		StructureTabIndicatorImages.Add(TabIndicatorImage);
		TabIndicatorSizeBox->AddChild(TabIndicatorImage);

		if (UHorizontalBoxSlot* IndicatorSlot = StructureTabIndicatorRowBox->AddChildToHorizontalBox(TabIndicatorSizeBox))
		{
			IndicatorSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			IndicatorSlot->SetVerticalAlignment(VAlign_Center);
		}

		AddTabEqualSpacer(*FString::Printf(TEXT("StructureTabIndicatorSpacer%d"), DisplayIndex));
	}

	FacilityButtonBarBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("StructureFacilityButtonBarBorder"));
	FacilityButtonBarBorder->SetPadding(FMargin(0.0f));
	FacilityButtonBarBorder->SetBrushColor(FacilityButtonBarColor);
	FacilityButtonBarBorder->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* CanvasSlot = StructureSelectionCanvasPanel->AddChildToCanvas(FacilityButtonBarBorder))
	{
		const float ClampedWidthRatio = FMath::Clamp(FacilityButtonBarWidthViewportRatio, 0.0f, 1.0f);
		const float ClampedCategoryHeightRatio = FMath::Clamp(CategoryBarHeightViewportRatio, 0.0f, 1.0f);
		const float ClampedHeightRatio = FMath::Clamp(FacilityButtonBarHeightViewportRatio, 0.0f, 1.0f);
		const float LeftAnchor = (1.0f - ClampedWidthRatio) * 0.5f;
		const float RightAnchor = 1.0f - LeftAnchor;
		const float BottomAnchor = 1.0f - ClampedCategoryHeightRatio;
		const float TopAnchor = FMath::Clamp(BottomAnchor - ClampedHeightRatio, 0.0f, 1.0f);
		CanvasSlot->SetAnchors(FAnchors(LeftAnchor, TopAnchor, RightAnchor, BottomAnchor));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}

	FacilityButtonRowBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("StructureFacilityButtonRowBox"));
	FacilityButtonBarBorder->SetContent(FacilityButtonRowBox);

	auto AddFacilityFillSpacer = [this](const TCHAR* SpacerName)
	{
		if (!WidgetTree || !FacilityButtonRowBox)
		{
			return;
		}

		USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), SpacerName);
		if (UHorizontalBoxSlot* SpacerSlot = FacilityButtonRowBox->AddChildToHorizontalBox(Spacer))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};

	auto AddFacilityFixedGap = [this](int32 GapIndex)
	{
		if (!WidgetTree || !FacilityButtonRowBox)
		{
			return;
		}

		USizeBox* GapSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonGapSizeBox%d"), GapIndex)));
		GapSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		FacilityButtonGapSizeBoxes.Add(GapSizeBox);

		USpacer* GapSpacer = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonGapSpacer%d"), GapIndex)));
		GapSizeBox->AddChild(GapSpacer);

		if (UHorizontalBoxSlot* SpacerSlot = FacilityButtonRowBox->AddChildToHorizontalBox(GapSizeBox))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			SpacerSlot->SetVerticalAlignment(VAlign_Center);
		}
	};

	FacilityButtonSizeBoxes.Reset();
	FacilityButtonGapSizeBoxes.Reset();
	FacilityButtons.Reset();
	FacilityButtonTextBlocks.Reset();

	AddFacilityFillSpacer(TEXT("StructureFacilityLeadingSpacer"));
	for (int32 ButtonIndex = 0; ButtonIndex < FacilityButtonCount; ++ButtonIndex)
	{
		const int32 DisplayIndex = ButtonIndex + 1;

		if (ButtonIndex > 0)
		{
			AddFacilityFixedGap(ButtonIndex);
		}

		USizeBox* FacilityButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonSizeBox%d"), DisplayIndex)));
		FacilityButtonSizeBoxes.Add(FacilityButtonSizeBox);

		UButton* FacilityButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButton%d"), DisplayIndex)));
		FacilityButton->SetBackgroundColor(FacilityButtonColor);
		FacilityButtons.Add(FacilityButton);
		FacilityButtonSizeBox->AddChild(FacilityButton);

		UTextBlock* FacilityButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonLabel%d"), DisplayIndex)));
		FacilityButtonTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		FacilityButtonTextBlock->SetJustification(ETextJustify::Center);
		FacilityButtonTextBlock->SetAutoWrapText(true);
		FacilityButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo FacilityButtonFont = FacilityButtonTextBlock->GetFont();
		FacilityButtonFont.Size = 9;
		FacilityButtonTextBlock->SetFont(FacilityButtonFont);
		FacilityButtonTextBlocks.Add(FacilityButtonTextBlock);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(FacilityButton->AddChild(FacilityButtonTextBlock)))
		{
			ButtonSlot->SetPadding(FMargin(2.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (ButtonIndex == 0)
		{
			FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton1Clicked);
			FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton1Hovered);
		}
		else if (ButtonIndex == 1)
		{
			FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton2Clicked);
			FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton2Hovered);
		}
		else if (ButtonIndex == 2)
		{
			FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton3Clicked);
			FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton3Hovered);
		}
		else if (ButtonIndex == 3)
		{
			FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton4Clicked);
			FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton4Hovered);
		}
		else if (ButtonIndex == 4)
		{
			FacilityButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton5Clicked);
			FacilityButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButton5Hovered);
		}
		FacilityButton->OnUnhovered.AddDynamic(this, &USRStructureSelectionWidget::HandleFacilityButtonUnhovered);

		if (UHorizontalBoxSlot* ButtonRowSlot = FacilityButtonRowBox->AddChildToHorizontalBox(FacilityButtonSizeBox))
		{
			ButtonRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ButtonRowSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	AddFacilityFillSpacer(TEXT("StructureFacilityTrailingSpacer"));

	CategoryBarBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("StructureCategoryBarBorder"));
	CategoryBarBorder->SetPadding(FMargin(0.0f));
	CategoryBarBorder->SetBrushColor(CategoryBarColor);
	StructureSelectionBorder = CategoryBarBorder;

	if (UCanvasPanelSlot* CanvasSlot = StructureSelectionCanvasPanel->AddChildToCanvas(CategoryBarBorder))
	{
		const float ClampedWidthRatio = FMath::Clamp(CategoryBarWidthViewportRatio, 0.0f, 1.0f);
		const float ClampedHeightRatio = FMath::Clamp(CategoryBarHeightViewportRatio, 0.0f, 1.0f);
		const float LeftAnchor = (1.0f - ClampedWidthRatio) * 0.5f;
		const float RightAnchor = 1.0f - LeftAnchor;
		CanvasSlot->SetAnchors(FAnchors(LeftAnchor, 1.0f - ClampedHeightRatio, RightAnchor, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}

	CategoryButtonRowBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("StructureCategoryButtonRowBox"));
	CategoryBarBorder->SetContent(CategoryButtonRowBox);

	auto AddCategoryFillSpacer = [this](const TCHAR* SpacerName)
	{
		if (!WidgetTree || !CategoryButtonRowBox)
		{
			return;
		}

		USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), SpacerName);
		if (UHorizontalBoxSlot* SpacerSlot = CategoryButtonRowBox->AddChildToHorizontalBox(Spacer))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};

	auto AddCategoryFixedGap = [this](int32 GapIndex)
	{
		if (!WidgetTree || !CategoryButtonRowBox)
		{
			return;
		}

		USizeBox* GapSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButtonGapSizeBox%d"), GapIndex)));
		GapSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		CategoryButtonGapSizeBoxes.Add(GapSizeBox);

		USpacer* GapSpacer = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButtonGapSpacer%d"), GapIndex)));
		GapSizeBox->AddChild(GapSpacer);

		if (UHorizontalBoxSlot* SpacerSlot = CategoryButtonRowBox->AddChildToHorizontalBox(GapSizeBox))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			SpacerSlot->SetVerticalAlignment(VAlign_Center);
		}
	};

	CategoryButtonSizeBoxes.Reset();
	CategoryButtonGapSizeBoxes.Reset();
	CategoryButtons.Reset();
	CategoryButtonIconSizeBoxes.Reset();
	CategoryButtonImages.Reset();
	CategoryButtonLabelSizeBoxes.Reset();
	CategoryButtonLabelTextBlocks.Reset();

	AddCategoryFillSpacer(TEXT("StructureCategoryLeadingSpacer"));
	for (int32 ButtonIndex = 0; ButtonIndex < StructureCategoryButtonCount; ++ButtonIndex)
	{
		const int32 DisplayIndex = ButtonIndex + 1;

		if (ButtonIndex > 0)
		{
			AddCategoryFixedGap(ButtonIndex);
		}

		USizeBox* CategoryButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButtonSizeBox%d"), DisplayIndex)));
		CategoryButtonSizeBoxes.Add(CategoryButtonSizeBox);

		UButton* CategoryButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButton%d"), DisplayIndex)));
		CategoryButton->SetBackgroundColor(CategoryButtonColor);
		CategoryButtons.Add(CategoryButton);
		CategoryButtonSizeBox->AddChild(CategoryButton);

		if (ButtonIndex == 0)
		{
			CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory1Clicked);
			CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory1Hovered);
		}
		else if (ButtonIndex == 1)
		{
			CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory2Clicked);
			CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory2Hovered);
		}
		else if (ButtonIndex == 2)
		{
			CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory3Clicked);
			CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory3Hovered);
		}
		else if (ButtonIndex == 3)
		{
			CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory4Clicked);
			CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory4Hovered);
		}
		CategoryButton->OnUnhovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategoryUnhovered);

		UVerticalBox* ButtonContentBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButtonContent%d"), DisplayIndex)));
		ButtonContentBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(CategoryButton->AddChild(ButtonContentBox)))
		{
			ButtonSlot->SetPadding(FMargin(0.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}

		USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButtonIconSizeBox%d"), DisplayIndex)));
		IconSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		CategoryButtonIconSizeBoxes.Add(IconSizeBox);

		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButtonImage%d"), DisplayIndex)));
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		CategoryButtonImages.Add(IconImage);
		IconSizeBox->AddChild(IconImage);
		if (UVerticalBoxSlot* IconSlot = ButtonContentBox->AddChildToVerticalBox(IconSizeBox))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}

		USizeBox* LabelSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButtonLabelSizeBox%d"), DisplayIndex)));
		LabelSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		CategoryButtonLabelSizeBoxes.Add(LabelSizeBox);

		UTextBlock* LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("StructureCategoryButtonLabel%d"), DisplayIndex)));
		LabelTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		LabelTextBlock->SetText(FText::AsNumber(DisplayIndex));
		LabelTextBlock->SetJustification(ETextJustify::Center);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo LabelFont = LabelTextBlock->GetFont();
		LabelFont.Size = 11;
		LabelTextBlock->SetFont(LabelFont);
		LabelSizeBox->AddChild(LabelTextBlock);
		if (UVerticalBoxSlot* LabelSlot = ButtonContentBox->AddChildToVerticalBox(LabelSizeBox))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (UHorizontalBoxSlot* ButtonRowSlot = CategoryButtonRowBox->AddChildToHorizontalBox(CategoryButtonSizeBox))
		{
			ButtonRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ButtonRowSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	AddCategoryFillSpacer(TEXT("StructureCategoryTrailingSpacer"));
}

void USRStructureSelectionWidget::RebuildBuildOptions()
{
	if (!WidgetTree || !BuildOptionsScrollBox)
	{
		return;
	}

	BuildOptionsScrollBox->ClearChildren();
	EntryActions.Reset();

	for (const FSRStructureBuildOption& BuildOption : BuildOptions)
	{
		if (BuildOption.StructureId.IsNone())
		{
			continue;
		}

		UButton* BuildOptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		BuildOptionButton->SetIsEnabled(BuildOption.bEnabled);
		BuildOptionButton->SetBackgroundColor(
			BuildOption.bEnabled
				? (bHasSelectedStructureId && SelectedStructureId == BuildOption.StructureId ? SelectedButtonColor : ButtonColor)
				: DisabledButtonColor);

		UVerticalBox* BuildOptionVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		BuildOptionButton->AddChild(BuildOptionVerticalBox);

		UTextBlock* NameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameTextBlock->SetText(GetBuildOptionDisplayName(BuildOption));
		NameTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo NameFont = NameTextBlock->GetFont();
		NameFont.Size = 14;
		NameTextBlock->SetFont(NameFont);
		NameTextBlock->SetAutoWrapText(false);
		if (UVerticalBoxSlot* NameSlot = BuildOptionVerticalBox->AddChildToVerticalBox(NameTextBlock))
		{
			NameSlot->SetPadding(FMargin(8.0f, 6.0f, 8.0f, BuildOption.Description.IsEmpty() ? 6.0f : 2.0f));
		}

		if (!BuildOption.Description.IsEmpty())
		{
			UTextBlock* DescriptionTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			DescriptionTextBlock->SetText(BuildOption.Description);
			DescriptionTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.78f, 0.82f, 1.0f)));
			FSlateFontInfo DescriptionFont = DescriptionTextBlock->GetFont();
			DescriptionFont.Size = 11;
			DescriptionTextBlock->SetFont(DescriptionFont);
			DescriptionTextBlock->SetAutoWrapText(true);
			if (UVerticalBoxSlot* DescriptionSlot = BuildOptionVerticalBox->AddChildToVerticalBox(DescriptionTextBlock))
			{
				DescriptionSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 7.0f));
			}
		}

		USRStructureSelectionEntryAction* EntryAction = NewObject<USRStructureSelectionEntryAction>(this);
		EntryAction->Initialize(this, BuildOption.StructureId);
		EntryActions.Add(EntryAction);
		BuildOptionButton->OnClicked.AddDynamic(EntryAction, &USRStructureSelectionEntryAction::HandleClicked);

		if (UScrollBoxSlot* ButtonSlot = Cast<UScrollBoxSlot>(BuildOptionsScrollBox->AddChild(BuildOptionButton)))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	}
}

void USRStructureSelectionWidget::RebuildCategorizedBuildOptions()
{
	ConveyorBuildOptionId = NAME_None;
	MinerBuildOptionId = NAME_None;
	ProcessingBuildOptionIds.Reset();
	SynthesisBuildOptionIds.Reset();

	for (const FSRStructureBuildOption& BuildOption : BuildOptions)
	{
		if (BuildOption.StructureId.IsNone() || !IsValid(BuildOption.StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = BuildOption.StructureDataAsset->BuildData();
		if (StructureData.bIsResourceDeposit)
		{
			continue;
		}

		if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
		{
			if (ConveyorBuildOptionId.IsNone())
			{
				ConveyorBuildOptionId = BuildOption.StructureId;
			}
			continue;
		}

		const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset;
		if (!IsValid(FacilityDataAsset))
		{
			continue;
		}

		switch (FacilityDataAsset->OperationKind)
		{
		case ESRFacilityOperationKind::Mine:
			if (MinerBuildOptionId.IsNone())
			{
				MinerBuildOptionId = BuildOption.StructureId;
			}
			break;
		case ESRFacilityOperationKind::Process:
		case ESRFacilityOperationKind::Split:
			ProcessingBuildOptionIds.Add(BuildOption.StructureId);
			break;
		case ESRFacilityOperationKind::Synthesize:
			SynthesisBuildOptionIds.Add(BuildOption.StructureId);
			break;
		default:
			break;
		}
	}

	if (SelectedCategoryIndex != INDEX_NONE && !IsStructureCategoryAvailable(SelectedCategoryIndex))
	{
		SelectedCategoryIndex = INDEX_NONE;
		SelectedFacilityButtonIndex = INDEX_NONE;
		HoveredFacilityButtonIndex = INDEX_NONE;
	}
	else if (SelectedCategoryIndex != 2 && SelectedCategoryIndex != 3)
	{
		SelectedFacilityButtonIndex = INDEX_NONE;
	}
	else if (SelectedFacilityButtonIndex != INDEX_NONE
		&& !IsBuildOptionSelectable(GetFacilityButtonStructureId(SelectedFacilityButtonIndex)))
	{
		SelectedFacilityButtonIndex = INDEX_NONE;
	}
}

void USRStructureSelectionWidget::SyncCategoryBarLayout()
{
	if (!CategoryBarBorder)
	{
		return;
	}

	const FVector2D BarLocalSize = CategoryBarBorder->GetCachedGeometry().GetLocalSize();
	const float ButtonHeightRatio = FMath::Clamp(CategoryButtonHeightRatio, 0.0f, 1.0f);
	const float BarHeight = BarLocalSize.Y > UE_SMALL_NUMBER
		? BarLocalSize.Y
		: DefaultCategoryButtonLength / FMath::Max(ButtonHeightRatio, UE_SMALL_NUMBER);
	const float ButtonLength = FMath::Max(1.0f, BarHeight * ButtonHeightRatio);
	const float ButtonGapLength = FMath::Max(0.0f, BarHeight * (1.0f - ButtonHeightRatio) * 0.5f);
	const float IconLength = FMath::Max(1.0f, ButtonLength * FMath::Clamp(CategoryButtonIconRatio, 0.0f, 1.0f));
	const float LabelLength = FMath::Max(1.0f, ButtonLength * FMath::Clamp(CategoryButtonLabelRatio, 0.0f, 1.0f));

	for (USizeBox* CategoryButtonSizeBox : CategoryButtonSizeBoxes)
	{
		if (!CategoryButtonSizeBox)
		{
			continue;
		}

		CategoryButtonSizeBox->SetWidthOverride(ButtonLength);
		CategoryButtonSizeBox->SetHeightOverride(ButtonLength);
	}

	for (USizeBox* CategoryButtonGapSizeBox : CategoryButtonGapSizeBoxes)
	{
		if (!CategoryButtonGapSizeBox)
		{
			continue;
		}

		CategoryButtonGapSizeBox->SetWidthOverride(ButtonGapLength);
		CategoryButtonGapSizeBox->SetHeightOverride(ButtonLength);
	}

	for (USizeBox* IconSizeBox : CategoryButtonIconSizeBoxes)
	{
		if (!IconSizeBox)
		{
			continue;
		}

		IconSizeBox->SetWidthOverride(IconLength);
		IconSizeBox->SetHeightOverride(IconLength);
	}

	for (USizeBox* LabelSizeBox : CategoryButtonLabelSizeBoxes)
	{
		if (!LabelSizeBox)
		{
			continue;
		}

		LabelSizeBox->SetWidthOverride(LabelLength);
		LabelSizeBox->SetHeightOverride(LabelLength);
	}
}

void USRStructureSelectionWidget::SyncFacilityButtonBarLayout()
{
	if (!FacilityButtonBarBorder)
	{
		return;
	}

	const FVector2D BarLocalSize = FacilityButtonBarBorder->GetCachedGeometry().GetLocalSize();
	const float ButtonHeightRatio = FMath::Clamp(FacilityButtonHeightRatio, 0.0f, 1.0f);
	const float BarHeight = BarLocalSize.Y > UE_SMALL_NUMBER
		? BarLocalSize.Y
		: DefaultFacilityButtonLength / FMath::Max(ButtonHeightRatio, UE_SMALL_NUMBER);
	const float ButtonLength = FMath::Max(1.0f, BarHeight * ButtonHeightRatio);
	const float ButtonGapLength = FMath::Max(0.0f, BarHeight * (1.0f - ButtonHeightRatio) * 0.5f);

	for (USizeBox* FacilityButtonSizeBox : FacilityButtonSizeBoxes)
	{
		if (!FacilityButtonSizeBox)
		{
			continue;
		}

		FacilityButtonSizeBox->SetWidthOverride(ButtonLength);
		FacilityButtonSizeBox->SetHeightOverride(ButtonLength);
	}

	for (USizeBox* FacilityButtonGapSizeBox : FacilityButtonGapSizeBoxes)
	{
		if (!FacilityButtonGapSizeBox)
		{
			continue;
		}

		FacilityButtonGapSizeBox->SetWidthOverride(ButtonGapLength);
		FacilityButtonGapSizeBox->SetHeightOverride(ButtonLength);
	}
}

void USRStructureSelectionWidget::SyncStructureTabBarLayout()
{
	if (!StructureTabBarBorder)
	{
		return;
	}

	const FVector2D BarLocalSize = StructureTabBarBorder->GetCachedGeometry().GetLocalSize();
	const float IndicatorHeightRatio = FMath::Clamp(StructureTabIndicatorHeightRatio, 0.0f, 1.0f);
	const float BarHeight = BarLocalSize.Y > UE_SMALL_NUMBER
		? BarLocalSize.Y
		: DefaultCategoryButtonLength * 0.5f;
	const float IndicatorLength = FMath::Max(1.0f, BarHeight * IndicatorHeightRatio);

	for (USizeBox* IndicatorSizeBox : StructureTabIndicatorSizeBoxes)
	{
		if (!IndicatorSizeBox)
		{
			continue;
		}

		IndicatorSizeBox->SetWidthOverride(IndicatorLength);
		IndicatorSizeBox->SetHeightOverride(IndicatorLength);
	}
}

void USRStructureSelectionWidget::RefreshFacilityButtonBarVisibility()
{
	if (!FacilityButtonBarBorder)
	{
		return;
	}

	const bool bShouldShowFacilityButtons = SelectedCategoryIndex == 2 || SelectedCategoryIndex == 3;
	FacilityButtonBarBorder->SetVisibility(bShouldShowFacilityButtons ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (StructureTabBarBorder)
	{
		StructureTabBarBorder->SetVisibility(bShouldShowFacilityButtons ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bShouldShowFacilityButtons)
	{
		SelectedFacilityButtonIndex = INDEX_NONE;
		HoveredFacilityButtonIndex = INDEX_NONE;
	}

	RefreshFacilityButtonLabels();
	RefreshFacilityButtonStyles();
}

void USRStructureSelectionWidget::RefreshFacilityButtonLabels()
{
	for (int32 ButtonIndex = 0; ButtonIndex < FacilityButtonTextBlocks.Num(); ++ButtonIndex)
	{
		const FName StructureId = GetFacilityButtonStructureId(ButtonIndex);
		const FSRStructureBuildOption* BuildOption = FindBuildOption(StructureId);
		const FText ButtonText = BuildOption ? GetBuildOptionDisplayName(*BuildOption) : FText::GetEmpty();

		if (UTextBlock* FacilityButtonTextBlock = FacilityButtonTextBlocks[ButtonIndex])
		{
			FacilityButtonTextBlock->SetText(ButtonText);
		}

		if (FacilityButtons.IsValidIndex(ButtonIndex))
		{
			if (UButton* FacilityButton = FacilityButtons[ButtonIndex])
			{
				FacilityButton->SetToolTipText(ButtonText);
			}
		}
	}
}

void USRStructureSelectionWidget::RefreshFacilityButtonStyles()
{
	for (int32 ButtonIndex = 0; ButtonIndex < FacilityButtons.Num(); ++ButtonIndex)
	{
		if (UButton* FacilityButton = FacilityButtons[ButtonIndex])
		{
			const FName StructureId = GetFacilityButtonStructureId(ButtonIndex);
			const bool bEnabled = IsBuildOptionSelectable(StructureId);
			const bool bSelected = bEnabled
				&& (ButtonIndex == SelectedFacilityButtonIndex
					|| (bHasSelectedStructureId && SelectedStructureId == StructureId));
			const bool bHovered = bEnabled && ButtonIndex == HoveredFacilityButtonIndex;
			FacilityButton->SetIsEnabled(bEnabled);
			FacilityButton->SetBackgroundColor(bEnabled
				? (bSelected ? SelectedFacilityButtonColor : (bHovered ? HoveredFacilityButtonColor : FacilityButtonColor))
				: DisabledButtonColor);

			if (FacilityButtonTextBlocks.IsValidIndex(ButtonIndex))
			{
				if (UTextBlock* FacilityButtonTextBlock = FacilityButtonTextBlocks[ButtonIndex])
				{
					FacilityButtonTextBlock->SetColorAndOpacity(FSlateColor(bEnabled
						? FLinearColor::White
						: FLinearColor(1.0f, 1.0f, 1.0f, 0.35f)));
				}
			}
		}
	}
}

void USRStructureSelectionWidget::RefreshPointerHoverState()
{
	if (!IsVisible() || !FSlateApplication::IsInitialized())
	{
		ClearHoveredStructureCategory();
		ClearHoveredFacilityButton();
		return;
	}

	const FVector2D ScreenPosition = FSlateApplication::Get().GetCursorPos();
	const int32 NewHoveredFacilityButtonIndex = FindFacilityButtonIndexAtScreenPosition(ScreenPosition);
	const int32 NewHoveredCategoryIndex = FindCategoryButtonIndexAtScreenPosition(ScreenPosition);

	if (HoveredFacilityButtonIndex != NewHoveredFacilityButtonIndex)
	{
		HoveredFacilityButtonIndex = NewHoveredFacilityButtonIndex;
		RefreshFacilityButtonStyles();
	}

	if (HoveredCategoryIndex != NewHoveredCategoryIndex)
	{
		HoveredCategoryIndex = NewHoveredCategoryIndex;
		RefreshCategoryButtonStyles();
	}
}

void USRStructureSelectionWidget::EnsureDefaultStructureTabIndicatorTexture()
{
	if (DefaultStructureTabIndicatorTexture)
	{
		return;
	}

	DefaultStructureTabIndicatorTexture = CreateStructureTabIndicatorTexture(TEXT("SR_TempStructureTabIndicator"));
}

void USRStructureSelectionWidget::RefreshStructureTabIndicatorBrushes()
{
	EnsureDefaultStructureTabIndicatorTexture();

	for (UImage* IndicatorImage : StructureTabIndicatorImages)
	{
		if (!IndicatorImage || !DefaultStructureTabIndicatorTexture)
		{
			continue;
		}

		IndicatorImage->SetBrushFromTexture(DefaultStructureTabIndicatorTexture, true);
		IndicatorImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void USRStructureSelectionWidget::RefreshStructureTabIndicatorStyles()
{
	SelectedStructureTabIndex = FMath::Clamp(SelectedStructureTabIndex, 0, StructureTabCount - 1);

	for (int32 TabIndex = 0; TabIndex < StructureTabIndicatorImages.Num(); ++TabIndex)
	{
		if (UImage* IndicatorImage = StructureTabIndicatorImages[TabIndex])
		{
			IndicatorImage->SetColorAndOpacity(TabIndex == SelectedStructureTabIndex
				? SelectedStructureTabIndicatorColor
				: InactiveStructureTabIndicatorColor);
		}
	}
}

void USRStructureSelectionWidget::SetStructureSelectionTabIndex(int32 NewTabIndex)
{
	if (StructureTabCount <= 0)
	{
		SelectedStructureTabIndex = INDEX_NONE;
		return;
	}

	SelectedStructureTabIndex = (NewTabIndex % StructureTabCount + StructureTabCount) % StructureTabCount;
	RefreshStructureTabIndicatorStyles();
	RefreshFacilityButtonLabels();
	RefreshFacilityButtonStyles();
}

void USRStructureSelectionWidget::EnsureDefaultCategoryIconTextures()
{
	DefaultCategoryIconTextures.SetNum(StructureCategoryButtonCount);
	for (int32 ButtonIndex = 0; ButtonIndex < StructureCategoryButtonCount; ++ButtonIndex)
	{
		if (DefaultCategoryIconTextures[ButtonIndex])
		{
			continue;
		}

		DefaultCategoryIconTextures[ButtonIndex] = CreateStructureCategoryIconTexture(
			ButtonIndex,
			FName(*FString::Printf(TEXT("SR_TempStructureCategoryIcon%d"), ButtonIndex + 1)));
	}
}

void USRStructureSelectionWidget::ApplyCategoryButtonIconBrushes()
{
	EnsureDefaultCategoryIconTextures();

	for (int32 ButtonIndex = 0; ButtonIndex < CategoryButtonImages.Num(); ++ButtonIndex)
	{
		const FSlateBrush* ConfiguredBrush = nullptr;
		switch (ButtonIndex)
		{
		case 0:
			ConfiguredBrush = &Category1IconBrush;
			break;
		case 1:
			ConfiguredBrush = &Category2IconBrush;
			break;
		case 2:
			ConfiguredBrush = &Category3IconBrush;
			break;
		case 3:
			ConfiguredBrush = &Category4IconBrush;
			break;
		default:
			break;
		}

		SetCategoryButtonImageBrush(
			CategoryButtonImages[ButtonIndex],
			ConfiguredBrush ? *ConfiguredBrush : FSlateBrush(),
			DefaultCategoryIconTextures.IsValidIndex(ButtonIndex) ? DefaultCategoryIconTextures[ButtonIndex] : nullptr);
	}
}

void USRStructureSelectionWidget::RefreshCategoryButtonStyles()
{
	for (int32 ButtonIndex = 0; ButtonIndex < CategoryButtons.Num(); ++ButtonIndex)
	{
		if (UButton* CategoryButton = CategoryButtons[ButtonIndex])
		{
			const bool bEnabled = IsStructureCategoryAvailable(ButtonIndex);
			const bool bSelected = bEnabled && ButtonIndex == SelectedCategoryIndex;
			const bool bHovered = bEnabled && ButtonIndex == HoveredCategoryIndex;
			CategoryButton->SetIsEnabled(bEnabled);
			CategoryButton->SetBackgroundColor(bEnabled
				? (bSelected ? SelectedCategoryButtonColor : (bHovered ? HoveredCategoryButtonColor : CategoryButtonColor))
				: DisabledButtonColor);
		}
	}
}

void USRStructureSelectionWidget::SelectStructureCategory(int32 CategoryIndex)
{
	if (CategoryIndex < 0 || CategoryIndex >= StructureCategoryButtonCount || !IsStructureCategoryAvailable(CategoryIndex))
	{
		return;
	}

	SelectedCategoryIndex = CategoryIndex;
	SelectedFacilityButtonIndex = INDEX_NONE;
	RefreshCategoryButtonStyles();
	RefreshFacilityButtonBarVisibility();

	if (CategoryIndex == 0)
	{
		SelectBuildOptionIfAvailable(ConveyorBuildOptionId);
	}
	else if (CategoryIndex == 1)
	{
		SelectBuildOptionIfAvailable(MinerBuildOptionId);
	}
	else
	{
		SetStructureSelectionTabIndex(0);
	}
}

void USRStructureSelectionWidget::SelectFacilityButton(int32 FacilityButtonIndex)
{
	if (FacilityButtonIndex < 0 || FacilityButtonIndex >= FacilityButtonCount)
	{
		return;
	}

	const FName StructureId = GetFacilityButtonStructureId(FacilityButtonIndex);
	if (!IsBuildOptionSelectable(StructureId))
	{
		return;
	}

	SelectedFacilityButtonIndex = FacilityButtonIndex;
	RefreshFacilityButtonStyles();
	SelectBuildOptionIfAvailable(StructureId);
}

bool USRStructureSelectionWidget::SelectBuildOptionIfAvailable(FName StructureId)
{
	if (!IsBuildOptionSelectable(StructureId))
	{
		return false;
	}

	DispatchBuildOptionSelected(StructureId);
	return bHasSelectedStructureId && SelectedStructureId == StructureId;
}

void USRStructureSelectionWidget::SetHoveredStructureCategory(int32 CategoryIndex)
{
	if (CategoryIndex < 0 || CategoryIndex >= StructureCategoryButtonCount)
	{
		return;
	}

	HoveredCategoryIndex = CategoryIndex;
	RefreshCategoryButtonStyles();
}

void USRStructureSelectionWidget::ClearHoveredStructureCategory()
{
	if (HoveredCategoryIndex == INDEX_NONE)
	{
		return;
	}

	HoveredCategoryIndex = INDEX_NONE;
	RefreshCategoryButtonStyles();
}

void USRStructureSelectionWidget::SetHoveredFacilityButton(int32 FacilityButtonIndex)
{
	if (FacilityButtonIndex < 0 || FacilityButtonIndex >= FacilityButtonCount)
	{
		return;
	}

	HoveredFacilityButtonIndex = FacilityButtonIndex;
	RefreshFacilityButtonStyles();
}

void USRStructureSelectionWidget::ClearHoveredFacilityButton()
{
	if (HoveredFacilityButtonIndex == INDEX_NONE)
	{
		return;
	}

	HoveredFacilityButtonIndex = INDEX_NONE;
	RefreshFacilityButtonStyles();
}

const TArray<FName>* USRStructureSelectionWidget::GetSelectedFacilityBuildOptionIds() const
{
	if (SelectedCategoryIndex == 2)
	{
		return &ProcessingBuildOptionIds;
	}

	if (SelectedCategoryIndex == 3)
	{
		return &SynthesisBuildOptionIds;
	}

	return nullptr;
}

FName USRStructureSelectionWidget::GetFacilityButtonStructureId(int32 FacilityButtonIndex) const
{
	const TArray<FName>* FacilityBuildOptionIds = GetSelectedFacilityBuildOptionIds();
	if (!FacilityBuildOptionIds || FacilityButtonIndex < 0 || FacilityButtonIndex >= FacilityButtonCount)
	{
		return NAME_None;
	}

	const int32 BuildOptionIndex = SelectedStructureTabIndex * FacilityButtonCount + FacilityButtonIndex;
	return FacilityBuildOptionIds->IsValidIndex(BuildOptionIndex)
		? (*FacilityBuildOptionIds)[BuildOptionIndex]
		: NAME_None;
}

bool USRStructureSelectionWidget::IsStructureCategoryAvailable(int32 CategoryIndex) const
{
	if (CategoryIndex == 0)
	{
		return IsBuildOptionSelectable(ConveyorBuildOptionId);
	}

	if (CategoryIndex == 1)
	{
		return IsBuildOptionSelectable(MinerBuildOptionId);
	}

	const TArray<FName>* FacilityBuildOptionIds = nullptr;
	if (CategoryIndex == 2)
	{
		FacilityBuildOptionIds = &ProcessingBuildOptionIds;
	}
	else if (CategoryIndex == 3)
	{
		FacilityBuildOptionIds = &SynthesisBuildOptionIds;
	}

	if (!FacilityBuildOptionIds)
	{
		return false;
	}

	for (const FName StructureId : *FacilityBuildOptionIds)
	{
		if (IsBuildOptionSelectable(StructureId))
		{
			return true;
		}
	}

	return false;
}

bool USRStructureSelectionWidget::IsBuildOptionSelectable(FName StructureId) const
{
	const FSRStructureBuildOption* BuildOption = FindBuildOption(StructureId);
	return BuildOption && BuildOption->bEnabled;
}

int32 USRStructureSelectionWidget::FindCategoryButtonIndexAtScreenPosition(const FVector2D& ScreenPosition) const
{
	for (int32 ButtonIndex = 0; ButtonIndex < CategoryButtons.Num(); ++ButtonIndex)
	{
		const UButton* CategoryButton = CategoryButtons[ButtonIndex];
		if (IsValid(CategoryButton)
			&& CategoryButton->IsVisible()
			&& CategoryButton->GetIsEnabled()
			&& CategoryButton->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return ButtonIndex;
		}
	}

	return INDEX_NONE;
}

int32 USRStructureSelectionWidget::FindFacilityButtonIndexAtScreenPosition(const FVector2D& ScreenPosition) const
{
	if (!FacilityButtonBarBorder || !FacilityButtonBarBorder->IsVisible())
	{
		return INDEX_NONE;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < FacilityButtons.Num(); ++ButtonIndex)
	{
		const UButton* FacilityButton = FacilityButtons[ButtonIndex];
		if (IsValid(FacilityButton)
			&& FacilityButton->IsVisible()
			&& FacilityButton->GetIsEnabled()
			&& FacilityButton->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return ButtonIndex;
		}
	}

	return INDEX_NONE;
}

void USRStructureSelectionWidget::HandleCategory1Clicked()
{
	SelectStructureCategory(0);
}

void USRStructureSelectionWidget::HandleCategory1Hovered()
{
	SetHoveredStructureCategory(0);
}

void USRStructureSelectionWidget::HandleCategory2Clicked()
{
	SelectStructureCategory(1);
}

void USRStructureSelectionWidget::HandleCategory2Hovered()
{
	SetHoveredStructureCategory(1);
}

void USRStructureSelectionWidget::HandleCategory3Clicked()
{
	SelectStructureCategory(2);
}

void USRStructureSelectionWidget::HandleCategory3Hovered()
{
	SetHoveredStructureCategory(2);
}

void USRStructureSelectionWidget::HandleCategory4Clicked()
{
	SelectStructureCategory(3);
}

void USRStructureSelectionWidget::HandleCategory4Hovered()
{
	SetHoveredStructureCategory(3);
}

void USRStructureSelectionWidget::HandleCategoryUnhovered()
{
	ClearHoveredStructureCategory();
}

void USRStructureSelectionWidget::HandleFacilityButton1Clicked()
{
	SelectFacilityButton(0);
}

void USRStructureSelectionWidget::HandleFacilityButton1Hovered()
{
	SetHoveredFacilityButton(0);
}

void USRStructureSelectionWidget::HandleFacilityButton2Clicked()
{
	SelectFacilityButton(1);
}

void USRStructureSelectionWidget::HandleFacilityButton2Hovered()
{
	SetHoveredFacilityButton(1);
}

void USRStructureSelectionWidget::HandleFacilityButton3Clicked()
{
	SelectFacilityButton(2);
}

void USRStructureSelectionWidget::HandleFacilityButton3Hovered()
{
	SetHoveredFacilityButton(2);
}

void USRStructureSelectionWidget::HandleFacilityButton4Clicked()
{
	SelectFacilityButton(3);
}

void USRStructureSelectionWidget::HandleFacilityButton4Hovered()
{
	SetHoveredFacilityButton(3);
}

void USRStructureSelectionWidget::HandleFacilityButton5Clicked()
{
	SelectFacilityButton(4);
}

void USRStructureSelectionWidget::HandleFacilityButton5Hovered()
{
	SetHoveredFacilityButton(4);
}

void USRStructureSelectionWidget::HandleFacilityButtonUnhovered()
{
	ClearHoveredFacilityButton();
}

void USRStructureSelectionWidget::RefreshSelectedStructureText()
{
	if (!SelectedStructureTextBlock)
	{
		return;
	}

	SelectedStructureTextBlock->SetText(MakeSelectedStructureText(
		bHasSelectedStructureId ? FindBuildOption(SelectedStructureId) : nullptr));
}

const FSRStructureBuildOption* USRStructureSelectionWidget::FindBuildOption(FName StructureId) const
{
	if (StructureId.IsNone())
	{
		return nullptr;
	}

	return BuildOptions.FindByPredicate([StructureId](const FSRStructureBuildOption& BuildOption)
	{
		return BuildOption.StructureId == StructureId;
	});
}

bool USRStructureSelectionWidget::IsScreenPositionOverStructureSelectionPanel(const FVector2D& ScreenPosition) const
{
	if (!IsVisible())
	{
		return false;
	}

	const bool bOverCategoryBar = CategoryBarBorder
		&& CategoryBarBorder->IsVisible()
		&& CategoryBarBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	const bool bOverFacilityButtonBar = FacilityButtonBarBorder
		&& FacilityButtonBarBorder->IsVisible()
		&& FacilityButtonBarBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	const bool bOverStructureTabBar = StructureTabBarBorder
		&& StructureTabBarBorder->IsVisible()
		&& StructureTabBarBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);

	return bOverCategoryBar || bOverFacilityButtonBar || bOverStructureTabBar;
}
