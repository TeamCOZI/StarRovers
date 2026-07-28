#include "UI/SRStructureSelectionWidget.h"

#include "Assembly/SRAssemblyStructurePlacementPreview.h"
#include "Camera/SRPlayerController.h"
#include "Simulation/SRRunMilestoneSubsystem.h"
#include "UI/SRStructureBuildPresentation.h"
#include "UI/SRResourceGlyph.h"
#include "UI/SRUIComponents.h"
#include "UI/SRUILayoutPolicy.h"
#include "UI/SRUITheme.h"
#include "Utility/SRLog.h"
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
	constexpr int32 StructureCategoryButtonCount = 7;
	constexpr int32 FacilityButtonCount = 5;
	constexpr int32 StructureCategoryIconTextureSize = 64;
	constexpr float DefaultCategoryButtonLength = 48.0f;
	constexpr float DefaultFacilityButtonLength = 108.0f;
	constexpr float MinimumFamilyCategoryBarWidthRatio = 0.62f;
	constexpr float MinimumFamilyCategoryBarHeightRatio = 0.06f;
	constexpr float MinimumFamilyFacilityBarWidthRatio = 0.72f;
	constexpr float MinimumFamilyFacilityBarHeightRatio = 0.12f;
	constexpr float MinimumDetailPanelWidthRatio = 0.72f;
	constexpr float MinimumDetailPanelHeightRatio = 0.21f;
	constexpr float MinimumStructureTabBarWidthRatio = 0.20f;
	constexpr float MaximumStructureTabBarWidthRatio = 0.34f;
	constexpr float BuildRecommendationRefreshIntervalSeconds = 0.50f;

	FText GetBuildOptionDisplayName(const FSRStructureBuildOption& BuildOption)
	{
		return BuildOption.DisplayName.IsEmpty()
			? FText::FromName(BuildOption.StructureId)
			: BuildOption.DisplayName;
	}

	FText GetBuildOptionButtonLabel(const FSRStructureBuildOption& BuildOption)
	{
		if (BuildOption.Availability != ESRStructureBuildAvailability::Available)
		{
			return FText::Format(
				NSLOCTEXT("StarRoversStructureSelection", "UnavailableBuildOptionFormat", "{0} | {1}"),
				USRUIThemeLibrary::ResolveBuildAvailabilityLabel(BuildOption.Availability),
				GetBuildOptionDisplayName(BuildOption));
		}
		return GetBuildOptionDisplayName(BuildOption);
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
		case 4:
			return (FMath::Abs(CenterX) <= 0.055f && NormalizedY >= 0.16f && NormalizedY <= 0.84f)
				|| (FMath::Abs(CenterY) <= 0.055f && NormalizedX >= 0.16f && NormalizedX <= 0.84f)
				|| FMath::Abs(CenterX + CenterY) <= 0.045f && DistanceFromCenter <= 0.34f;
		case 5:
			return FMath::Abs(DistanceFromCenter - 0.28f) <= 0.045f
				|| (FMath::Abs(CenterX) <= 0.035f && DistanceFromCenter <= 0.18f)
				|| (FMath::Abs(CenterY) <= 0.035f && DistanceFromCenter <= 0.18f);
		case 6:
			return (FMath::Abs(CenterX) <= 0.045f && NormalizedY >= 0.18f && NormalizedY <= 0.82f)
				|| (FMath::Abs(CenterY) <= 0.045f && NormalizedX >= 0.18f && NormalizedX <= 0.82f)
				|| FMath::Abs(DistanceFromCenter - 0.34f) <= 0.025f;
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
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection BuildOption OnClicked StructureId=%s"),
		*StructureId.ToString());

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

	ApplySharedUITheme();
	BuildStructureSelectionWidgetTree();
	return Super::RebuildWidget();
}

void USRStructureSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplySharedUITheme();
	BuildStructureSelectionWidgetTree();
	ApplySharedUITheme();
	EnsureDefaultCategoryIconTextures();
	EnsureDefaultStructureTabIndicatorTexture();
	ApplyCategoryButtonIconBrushes();
	RefreshStructureTabIndicatorBrushes();
	RebuildBuildOptionIndex();
	RebuildCategorizedBuildOptions();
	RefreshBuildRecommendation(true);
	RefreshCategoryButtonLabels();
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
	SyncDetailPanelLayout();
	RefreshBuildOptionDetail();
}

void USRStructureSelectionWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplySharedUITheme();
	BuildStructureSelectionWidgetTree();
	ApplySharedUITheme();
	EnsureDefaultCategoryIconTextures();
	EnsureDefaultStructureTabIndicatorTexture();
	ApplyCategoryButtonIconBrushes();
	RefreshStructureTabIndicatorBrushes();
	RebuildBuildOptionIndex();
	RebuildCategorizedBuildOptions();
	RefreshBuildRecommendation(true);
	RefreshCategoryButtonLabels();
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
	SyncDetailPanelLayout();
	RefreshBuildOptionDetail();
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
	SyncDetailPanelLayout();
	BuildRecommendationRefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
	if (BuildRecommendationRefreshAccumulator >= BuildRecommendationRefreshIntervalSeconds)
	{
		BuildRecommendationRefreshAccumulator = 0.0f;
		RefreshBuildRecommendation(true);
	}
	RefreshPointerHoverState();
	RefreshBuildOptionDetail();
}

FReply USRStructureSelectionWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverStructureSelectionPanel(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f) CategoryIndex=%d FacilityButtonIndex=%d"),
			ScreenPosition.X,
			ScreenPosition.Y,
			FindCategoryButtonIndexAtScreenPosition(ScreenPosition),
			FindFacilityButtonIndexAtScreenPosition(ScreenPosition));
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRStructureSelectionWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverStructureSelectionPanel(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f) CategoryIndex=%d FacilityButtonIndex=%d"),
			ScreenPosition.X,
			ScreenPosition.Y,
			FindCategoryButtonIndexAtScreenPosition(ScreenPosition),
			FindFacilityButtonIndexAtScreenPosition(ScreenPosition));
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
	RebuildBuildOptionIndex();
	if (bHasSelectedStructureId && !IsBuildOptionSelectable(SelectedStructureId))
	{
		ClearSelectedStructureId();
	}

	RebuildCategorizedBuildOptions();
	RebuildBuildOptions();
	RefreshBuildRecommendation(true);
	RefreshCategoryButtonLabels();
	RefreshCategoryButtonStyles();
	RefreshFacilityButtonBarVisibility();
	RefreshFacilityButtonLabels();
	RefreshFacilityButtonStyles();
	RefreshSelectedStructureText();
	RefreshBuildOptionDetail();
}

void USRStructureSelectionWidget::SetBuildCatalog(const FSRStructureBuildCatalog& NewBuildCatalog)
{
	SetBuildOptions(NewBuildCatalog.BuildOptions);
}

void USRStructureSelectionWidget::SetBuildOptionsFromDataAssets(const TArray<USRStructureDataAsset*>& StructureDataAssets)
{
	FSRStructureBuildCatalog BuildCatalog;
	FSRStructureBuildCatalogBuilder::BuildCatalog(StructureDataAssets, nullptr, BuildCatalog);
	SetBuildCatalog(BuildCatalog);
}

TArray<FSRStructureBuildOption> USRStructureSelectionWidget::GetBuildOptions() const
{
	return BuildOptions;
}

void USRStructureSelectionWidget::SetSelectedStructureId(FName NewSelectedStructureId)
{
	const FSRStructureBuildOption* BuildOption = FindBuildOption(NewSelectedStructureId);
	if (!BuildOption || !BuildOption->IsSelectable())
	{
		return;
	}

	SelectedStructureId = NewSelectedStructureId;
	bHasSelectedStructureId = !SelectedStructureId.IsNone();
	RevealBuildOptionInDock(SelectedStructureId);
	RebuildBuildOptions();
	RefreshCategoryButtonStyles();
	RefreshFacilityButtonStyles();
	RefreshSelectedStructureText();
	RefreshBuildOptionDetail();
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
	RefreshBuildOptionDetail();
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
	case 4:
		Category5IconBrush = IconBrush;
		break;
	case 5:
		Category6IconBrush = IconBrush;
		break;
	case 6:
		Category7IconBrush = IconBrush;
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
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection TryHandleStructureSelectionPointerClick Mouse=(%.1f, %.1f)"),
		ScreenPosition.X,
		ScreenPosition.Y);

	const int32 FacilityButtonIndex = FindFacilityButtonIndexAtScreenPosition(ScreenPosition);
	if (FacilityButtonIndex != INDEX_NONE)
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection manual click resolved FacilityButtonIndex=%d"),
			FacilityButtonIndex);
		SelectFacilityButton(FacilityButtonIndex);
		return true;
	}

	const int32 CategoryIndex = FindCategoryButtonIndexAtScreenPosition(ScreenPosition);
	if (CategoryIndex != INDEX_NONE)
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection manual click resolved CategoryIndex=%d"),
			CategoryIndex);
		SelectStructureCategory(CategoryIndex);
		return true;
	}

	return IsScreenPositionOverStructureSelectionPanel(ScreenPosition);
}

bool USRStructureSelectionWidget::AdvanceStructureSelectionTab()
{
	if (!IsVisible() || !IsStructureCategoryAvailable(SelectedCategoryIndex))
	{
		return false;
	}

	const int32 PageCount = GetSelectedFacilityPageCount();
	if (PageCount <= 0)
	{
		return false;
	}

	SetStructureSelectionTabIndex((SelectedStructureTabIndex + 1) % PageCount);
	return true;
}

bool USRStructureSelectionWidget::RetreatStructureSelectionTab()
{
	if (!IsVisible() || !IsStructureCategoryAvailable(SelectedCategoryIndex))
	{
		return false;
	}

	const int32 PageCount = GetSelectedFacilityPageCount();
	if (PageCount <= 0)
	{
		return false;
	}

	SetStructureSelectionTabIndex((SelectedStructureTabIndex - 1 + PageCount) % PageCount);
	return true;
}

int32 USRStructureSelectionWidget::GetBuildDockPageCount() const
{
	return GetSelectedFacilityPageCount();
}

int32 USRStructureSelectionWidget::GetBuildDockPageIndex() const
{
	const int32 PageCount = GetSelectedFacilityPageCount();
	return PageCount > 0
		? FMath::Clamp(SelectedStructureTabIndex, 0, PageCount - 1)
		: 0;
}

int32 USRStructureSelectionWidget::GetVisibleBuildDockPageIndicatorCount() const
{
	return StructureTabIndicatorImages.Num();
}

bool USRStructureSelectionWidget::SelectStructureCategoryByShortcut(int32 CategoryIndex)
{
	if (!IsVisible()
		|| CategoryIndex < 0
		|| CategoryIndex >= StructureCategoryButtonCount
		|| !IsStructureCategoryAvailable(CategoryIndex))
	{
		return false;
	}

	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection shortcut resolved CategoryIndex=%d"),
		CategoryIndex);
	SelectStructureCategory(CategoryIndex);
	return true;
}

bool USRStructureSelectionWidget::SelectBuildDockFamily(ESRStructureBuildFamilyFilter FamilyFilter)
{
	const int32 FamilyTabIndex = FamilyTabs.IndexOfByPredicate(
		[FamilyFilter](const FSRStructureBuildFamilyTab& Tab)
		{
			return Tab.Filter == FamilyFilter;
		});
	if (FamilyTabIndex == INDEX_NONE || !IsStructureCategoryAvailable(FamilyTabIndex))
	{
		return false;
	}

	SelectStructureCategory(FamilyTabIndex);
	return SelectedCategoryIndex == FamilyTabIndex;
}

ESRStructureBuildFamilyFilter USRStructureSelectionWidget::GetSelectedBuildDockFamily() const
{
	return SelectedFamilyFilter;
}

TArray<FSRStructureBuildOption> USRStructureSelectionWidget::GetVisibleBuildOptions() const
{
	TArray<FSRStructureBuildOption> VisibleBuildOptions;
	VisibleBuildOptions.Reserve(VisibleBuildOptionIds.Num());
	for (const FName StructureId : VisibleBuildOptionIds)
	{
		if (const FSRStructureBuildOption* BuildOption = FindBuildOption(StructureId))
		{
			VisibleBuildOptions.Add(*BuildOption);
		}
	}
	return VisibleBuildOptions;
}

TArray<FSRStructureBuildFamilyTab> USRStructureSelectionWidget::GetBuildDockFamilyTabs() const
{
	return FamilyTabs;
}

FName USRStructureSelectionWidget::GetRecommendedBuildOptionId() const
{
	return BuildRecommendationContext.bActive
		? BuildRecommendationContext.RecommendedStructureId
		: NAME_None;
}

bool USRStructureSelectionWidget::SelectRecommendedBuildOption(
	ESRStructureBuildRole Role,
	ESRResourceFamily PreferredFamily,
	bool bDispatchSelection)
{
	RefreshBuildRecommendation(true);
	const FName CandidateId = FSRStructureBuildDockModel::FindRecommendedOptionId(
		BuildOptions,
		Role,
		PreferredFamily);
	const FSRStructureBuildOption* Candidate = FindBuildOption(CandidateId);

	if (!Candidate)
	{
		return false;
	}

	const ESRStructureBuildFamilyFilter PreferredFilter =
		FSRStructureBuildDockModel::ResolvePreferredFilter(*Candidate);
	if (!SelectBuildDockFamily(PreferredFilter))
	{
		SelectBuildDockFamily(ESRStructureBuildFamilyFilter::All);
	}

	if (bDispatchSelection)
	{
		DispatchBuildOptionSelected(Candidate->StructureId);
	}
	else
	{
		SetSelectedStructureId(Candidate->StructureId);
	}
	return HasSelectedStructureId() && GetSelectedStructureId() == Candidate->StructureId;
}

void USRStructureSelectionWidget::DispatchBuildOptionSelected(FName StructureId)
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection DispatchBuildOptionSelected StructureId=%s"),
		*StructureId.ToString());

	const FSRStructureBuildOption* RequestedBuildOption = FindBuildOption(StructureId);
	if (!RequestedBuildOption || !RequestedBuildOption->IsSelectable())
	{
		SR_LOG(UIClickTrace, LogTemp, Log,
			TEXT("SR UI Click Trace: StructureSelection DispatchBuildOptionSelected ignored StructureId=%s BlockReason=%s"),
			*StructureId.ToString(),
			RequestedBuildOption
				? *StaticEnum<ESRStructureBuildBlockReason>()->GetNameStringByValue(static_cast<int64>(RequestedBuildOption->BlockReason))
				: TEXT("Missing"));
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

void USRStructureSelectionWidget::ApplySharedUITheme()
{
	if (!bUseSharedUITheme)
	{
		return;
	}

	const USRUIThemeSettings* Theme = USRUIThemeLibrary::GetThemeSettings();
	if (!IsValid(Theme))
	{
		return;
	}

	const FSRUIStatePalette Neutral = USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Neutral);
	const FSRUIStatePalette Hovered = USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Hovered);
	const FSRUIStatePalette Selected = USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Selected);
	const FSRUIStatePalette Disabled = USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Disabled);

	PanelColor = Theme->PanelColor;
	ButtonColor = Neutral.SurfaceColor;
	SelectedButtonColor = Selected.SurfaceColor;
	DisabledButtonColor = Disabled.SurfaceColor;
	CategoryBarColor = Theme->PanelColor;
	CategoryButtonColor = Neutral.SurfaceColor;
	SelectedCategoryButtonColor = Selected.SurfaceColor;
	HoveredCategoryButtonColor = Hovered.SurfaceColor;
	FacilityButtonBarColor = Theme->PanelColor;
	FacilityButtonColor = Neutral.SurfaceColor;
	SelectedFacilityButtonColor = Selected.SurfaceColor;
	HoveredFacilityButtonColor = Hovered.SurfaceColor;
	StructureTabBarColor = Theme->PanelColor;
	SelectedStructureTabIndicatorColor = Selected.AccentColor;
	InactiveStructureTabIndicatorColor = Neutral.SecondaryTextColor.CopyWithNewOpacity(0.45f);

	if (CategoryBarBorder)
	{
		CategoryBarBorder->SetBrushColor(CategoryBarColor);
	}
	if (FacilityButtonBarBorder)
	{
		FacilityButtonBarBorder->SetBrushColor(FacilityButtonBarColor);
	}
	if (StructureTabBarBorder)
	{
		StructureTabBarBorder->SetBrushColor(StructureTabBarColor);
	}
	if (StructureDetailBorder)
	{
		USRUIThemeLibrary::ApplyCardStyle(
			StructureDetailBorder,
			ESRUIVisualState::Neutral,
			FMargin(14.0f, 10.0f));
	}
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
		StructurePageTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructurePageTextBlock"))));
		StructureDetailBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("StructureDetailBorder"))));
		DetailTitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureDetailTitle"))));
		DetailClassificationTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureDetailClassification"))));
		DetailSpecificationTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureDetailSpecification"))));
		DetailDescriptionTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureDetailDescription"))));
		DetailAvailabilityBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("StructureDetailAvailabilityBadge"))));
		DetailFamilyGlyph = Cast<USRResourceGlyphWidget>(WidgetTree->FindWidget(FName(TEXT("StructureDetailFamilyGlyph"))));
		DetailRecommendationRow = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("StructureDetailRecommendationRow"))));
		DetailRecommendationBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("StructureDetailRecommendationBadge"))));
		DetailRecommendationTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureDetailRecommendationText"))));
		DetailFlowPanel = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("StructureBuildFlowPanel"))));
		DetailFlowInputBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("StructureBuildFlowInput"))));
		DetailFlowProcessBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("StructureBuildFlowProcess"))));
		DetailFlowOutputBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("StructureBuildFlowOutput"))));
		DetailFlowEffectTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureBuildFlowEffect"))));
		DetailPlacementMetricRow = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("StructurePlacementMetricRow"))));
		DetailPlacementTargetBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("StructurePlacementTargetBadge"))));
		DetailPlacementFootprintBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("StructurePlacementFootprintBadge"))));
		DetailPlacementCapacityBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("StructurePlacementCapacityBadge"))));
		DetailPlacementStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureDetailPlacementStatus"))));
		DetailPlacementTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureDetailPlacement"))));

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
		FacilityButtonRoleTextBlocks.Reset();
		FacilityButtonMetadataTextBlocks.Reset();
		FacilityButtonStatusBadges.Reset();
		FacilityButtonFamilyGlyphs.Reset();
		StructureTabIndicatorSizeBoxes.Reset();
		StructureTabIndicatorImages.Reset();
		StructureTabIndicatorFirstPageIndex = 0;

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
			FacilityButtonRoleTextBlocks.Add(Cast<UTextBlock>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButtonRole%d"), DisplayIndex)))));
			FacilityButtonMetadataTextBlocks.Add(Cast<UTextBlock>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButtonMetadata%d"), DisplayIndex)))));
			FacilityButtonStatusBadges.Add(Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButtonStatus%d"), DisplayIndex)))));
			FacilityButtonFamilyGlyphs.Add(Cast<USRResourceGlyphWidget>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButtonFamilyGlyph%d"), DisplayIndex)))));
			if (ButtonIndex > 0)
			{
				FacilityButtonGapSizeBoxes.Add(Cast<USizeBox>(WidgetTree->FindWidget(FName(*FString::Printf(TEXT("StructureFacilityButtonGapSizeBox%d"), ButtonIndex)))));
			}
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
				&& IsValid(FacilityButtonTextBlocks[ButtonIndex])
				&& FacilityButtonRoleTextBlocks.IsValidIndex(ButtonIndex)
				&& IsValid(FacilityButtonRoleTextBlocks[ButtonIndex])
				&& FacilityButtonMetadataTextBlocks.IsValidIndex(ButtonIndex)
				&& IsValid(FacilityButtonMetadataTextBlocks[ButtonIndex])
				&& FacilityButtonStatusBadges.IsValidIndex(ButtonIndex)
				&& IsValid(FacilityButtonStatusBadges[ButtonIndex])
				&& FacilityButtonFamilyGlyphs.IsValidIndex(ButtonIndex)
				&& IsValid(FacilityButtonFamilyGlyphs[ButtonIndex]);
		}
		for (USizeBox* FacilityButtonGapSizeBox : FacilityButtonGapSizeBoxes)
		{
			bHasRequiredFacilityButtons = bHasRequiredFacilityButtons && IsValid(FacilityButtonGapSizeBox);
		}

		const bool bHasRequiredTabIndicatorHost = StructureTabBarBorder && StructureTabIndicatorRowBox;
		const bool bHasRequiredDetailPanel = StructureDetailBorder
			&& DetailTitleTextBlock
			&& DetailClassificationTextBlock
			&& DetailSpecificationTextBlock
			&& DetailDescriptionTextBlock
			&& DetailAvailabilityBadge
			&& DetailFamilyGlyph
			&& DetailRecommendationRow
			&& DetailRecommendationBadge
			&& DetailRecommendationTextBlock
			&& DetailFlowPanel
			&& DetailFlowInputBadge
			&& DetailFlowProcessBadge
			&& DetailFlowOutputBadge
			&& DetailFlowEffectTextBlock
			&& DetailPlacementMetricRow
			&& DetailPlacementTargetBadge
			&& DetailPlacementFootprintBadge
			&& DetailPlacementCapacityBadge
			&& DetailPlacementStatusTextBlock
			&& DetailPlacementTextBlock;

		if (bHasRequiredCategoryButtons
			&& bHasRequiredFacilityButtons
			&& bHasRequiredTabIndicatorHost
			&& bHasRequiredDetailPanel)
		{
			RebuildStructureTabIndicators();

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
				else if (ButtonIndex == 4)
				{
					CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory5Clicked);
					CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory5Hovered);
				}
				else if (ButtonIndex == 5)
				{
					CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory6Clicked);
					CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory6Hovered);
				}
				else if (ButtonIndex == 6)
				{
					CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory7Clicked);
					CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory7Hovered);
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
		StructurePageTextBlock = nullptr;
		StructureDetailBorder = nullptr;
		DetailTitleTextBlock = nullptr;
		DetailClassificationTextBlock = nullptr;
		DetailSpecificationTextBlock = nullptr;
		DetailDescriptionTextBlock = nullptr;
		DetailAvailabilityBadge = nullptr;
		DetailFamilyGlyph = nullptr;
		DetailRecommendationRow = nullptr;
		DetailRecommendationBadge = nullptr;
		DetailRecommendationTextBlock = nullptr;
		DetailFlowPanel = nullptr;
		DetailFlowInputBadge = nullptr;
		DetailFlowProcessBadge = nullptr;
		DetailFlowOutputBadge = nullptr;
		DetailFlowEffectTextBlock = nullptr;
		DetailPlacementMetricRow = nullptr;
		DetailPlacementTargetBadge = nullptr;
		DetailPlacementFootprintBadge = nullptr;
		DetailPlacementCapacityBadge = nullptr;
		DetailPlacementStatusTextBlock = nullptr;
		DetailPlacementTextBlock = nullptr;
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
		FacilityButtonRoleTextBlocks.Reset();
		FacilityButtonMetadataTextBlocks.Reset();
		FacilityButtonStatusBadges.Reset();
		FacilityButtonFamilyGlyphs.Reset();
		StructureTabIndicatorSizeBoxes.Reset();
		StructureTabIndicatorImages.Reset();
		StructureTabIndicatorFirstPageIndex = 0;
	}

	UCanvasPanel* StructureSelectionCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("StructureSelectionCanvasPanel"));
	WidgetTree->RootWidget = StructureSelectionCanvasPanel;
	StructureSelectionCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	StructureDetailBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("StructureDetailBorder"));
	StructureDetailBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	USRUIThemeLibrary::ApplyCardStyle(
		StructureDetailBorder,
		ESRUIVisualState::Neutral,
		FMargin(14.0f, 10.0f));
	StructureSelectionCanvasPanel->AddChildToCanvas(StructureDetailBorder);

	UVerticalBox* DetailVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("StructureDetailVerticalBox"));
	StructureDetailBorder->SetContent(DetailVerticalBox);

	UHorizontalBox* DetailHeader = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("StructureDetailHeader"));
	if (UVerticalBoxSlot* HeaderSlot = DetailVerticalBox->AddChildToVerticalBox(DetailHeader))
	{
		HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}

	DetailTitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("StructureDetailTitle"));
	DetailTitleTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	USRUIThemeLibrary::ApplyTextStyle(DetailTitleTextBlock, ESRUITextStyle::Heading);
	if (UHorizontalBoxSlot* TitleSlot = DetailHeader->AddChildToHorizontalBox(DetailTitleTextBlock))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	DetailFamilyGlyph = WidgetTree->ConstructWidget<USRResourceGlyphWidget>(
		USRResourceGlyphWidget::StaticClass(),
		TEXT("StructureDetailFamilyGlyph"));
	DetailFamilyGlyph->SetGlyphMode(ESRResourceGlyphMode::FamilyOnly);
	DetailFamilyGlyph->SetVisibility(ESlateVisibility::Collapsed);
	if (UHorizontalBoxSlot* GlyphSlot = DetailHeader->AddChildToHorizontalBox(DetailFamilyGlyph))
	{
		GlyphSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		GlyphSlot->SetPadding(FMargin(4.0f, 0.0f, 5.0f, 0.0f));
		GlyphSlot->SetVerticalAlignment(VAlign_Center);
	}

	DetailAvailabilityBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("StructureDetailAvailabilityBadge"));
	DetailAvailabilityBadge->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UHorizontalBoxSlot* BadgeSlot = DetailHeader->AddChildToHorizontalBox(DetailAvailabilityBadge))
	{
		BadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BadgeSlot->SetVerticalAlignment(VAlign_Center);
	}

	auto AddDetailText = [this, DetailVerticalBox](
		const TCHAR* WidgetName,
		ESRUITextStyle TextStyle,
		TObjectPtr<UTextBlock>& OutTextBlock,
		bool bAccent,
		float BottomPadding)
	{
		OutTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		OutTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		OutTextBlock->SetAutoWrapText(true);
		USRUIThemeLibrary::ApplyTextStyle(OutTextBlock, TextStyle, ESRUIVisualState::Neutral, bAccent);
		if (UVerticalBoxSlot* TextSlot = DetailVerticalBox->AddChildToVerticalBox(OutTextBlock))
		{
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
		}
	};

	AddDetailText(TEXT("StructureDetailClassification"), ESRUITextStyle::Caption, DetailClassificationTextBlock, true, 2.0f);
	AddDetailText(TEXT("StructureDetailSpecification"), ESRUITextStyle::Caption, DetailSpecificationTextBlock, false, 3.0f);
	AddDetailText(TEXT("StructureDetailDescription"), ESRUITextStyle::Body, DetailDescriptionTextBlock, false, 3.0f);

	DetailRecommendationRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("StructureDetailRecommendationRow"));
	DetailRecommendationRow->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* RecommendationSlot = DetailVerticalBox->AddChildToVerticalBox(DetailRecommendationRow))
	{
		RecommendationSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		RecommendationSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}
	DetailRecommendationBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("StructureDetailRecommendationBadge"));
	if (UHorizontalBoxSlot* RecommendationBadgeSlot =
		DetailRecommendationRow->AddChildToHorizontalBox(DetailRecommendationBadge))
	{
		RecommendationBadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		RecommendationBadgeSlot->SetVerticalAlignment(VAlign_Center);
	}
	DetailRecommendationTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("StructureDetailRecommendationText"));
	DetailRecommendationTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	DetailRecommendationTextBlock->SetAutoWrapText(false);
	USRUIThemeLibrary::ApplyTextStyle(
		DetailRecommendationTextBlock,
		ESRUITextStyle::Caption,
		ESRUIVisualState::Warning,
		true);
	if (UHorizontalBoxSlot* RecommendationTextSlot =
		DetailRecommendationRow->AddChildToHorizontalBox(DetailRecommendationTextBlock))
	{
		RecommendationTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RecommendationTextSlot->SetVerticalAlignment(VAlign_Center);
		RecommendationTextSlot->SetPadding(FMargin(7.0f, 0.0f, 0.0f, 0.0f));
	}

	DetailFlowPanel = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("StructureBuildFlowPanel"));
	DetailFlowPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* FlowSlot = DetailVerticalBox->AddChildToVerticalBox(DetailFlowPanel))
	{
		FlowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		FlowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	auto AddFlowBadge = [this](
		const TCHAR* WidgetName,
		ESRUIVisualState VisualState,
		TObjectPtr<USRStatusBadgeWidget>& OutBadge)
	{
		OutBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
			USRStatusBadgeWidget::StaticClass(),
			WidgetName);
		OutBadge->SetBadge(FText::GetEmpty(), VisualState);
		if (UHorizontalBoxSlot* BadgeSlot = DetailFlowPanel->AddChildToHorizontalBox(OutBadge))
		{
			BadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			BadgeSlot->SetVerticalAlignment(VAlign_Center);
		}
	};
	auto AddFlowArrow = [this](const TCHAR* WidgetName)
	{
		UTextBlock* Arrow = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		Arrow->SetText(NSLOCTEXT("StarRoversBuildDock", "FlowArrow", ">"));
		Arrow->SetVisibility(ESlateVisibility::HitTestInvisible);
		USRUIThemeLibrary::ApplyTextStyle(Arrow, ESRUITextStyle::Caption, ESRUIVisualState::Info, true);
		if (UHorizontalBoxSlot* ArrowSlot = DetailFlowPanel->AddChildToHorizontalBox(Arrow))
		{
			ArrowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			ArrowSlot->SetVerticalAlignment(VAlign_Center);
			ArrowSlot->SetPadding(FMargin(7.0f, 0.0f));
		}
	};
	AddFlowBadge(TEXT("StructureBuildFlowInput"), ESRUIVisualState::Neutral, DetailFlowInputBadge);
	AddFlowArrow(TEXT("StructureBuildFlowArrow1"));
	AddFlowBadge(TEXT("StructureBuildFlowProcess"), ESRUIVisualState::Info, DetailFlowProcessBadge);
	AddFlowArrow(TEXT("StructureBuildFlowArrow2"));
	AddFlowBadge(TEXT("StructureBuildFlowOutput"), ESRUIVisualState::Positive, DetailFlowOutputBadge);
	AddDetailText(TEXT("StructureBuildFlowEffect"), ESRUITextStyle::Caption, DetailFlowEffectTextBlock, true, 3.0f);
	DetailFlowEffectTextBlock->SetVisibility(ESlateVisibility::Collapsed);

	DetailPlacementMetricRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("StructurePlacementMetricRow"));
	DetailPlacementMetricRow->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* MetricSlot = DetailVerticalBox->AddChildToVerticalBox(DetailPlacementMetricRow))
	{
		MetricSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		MetricSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}
	auto AddPlacementMetric = [this](
		const TCHAR* WidgetName,
		TObjectPtr<USRStatusBadgeWidget>& OutBadge,
		float RightPadding)
	{
		OutBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
			USRStatusBadgeWidget::StaticClass(),
			WidgetName);
		if (UHorizontalBoxSlot* MetricBadgeSlot =
			DetailPlacementMetricRow->AddChildToHorizontalBox(OutBadge))
		{
			MetricBadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			MetricBadgeSlot->SetVerticalAlignment(VAlign_Center);
			MetricBadgeSlot->SetPadding(FMargin(0.0f, 0.0f, RightPadding, 0.0f));
		}
	};
	AddPlacementMetric(TEXT("StructurePlacementTargetBadge"), DetailPlacementTargetBadge, 6.0f);
	AddPlacementMetric(TEXT("StructurePlacementFootprintBadge"), DetailPlacementFootprintBadge, 6.0f);
	AddPlacementMetric(TEXT("StructurePlacementCapacityBadge"), DetailPlacementCapacityBadge, 0.0f);
	AddDetailText(TEXT("StructureDetailPlacementStatus"), ESRUITextStyle::Caption, DetailPlacementStatusTextBlock, true, 2.0f);
	AddDetailText(TEXT("StructureDetailPlacement"), ESRUITextStyle::Caption, DetailPlacementTextBlock, false, 0.0f);
	SyncDetailPanelLayout();

	StructureTabBarBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("StructureTabBarBorder"));
	StructureTabBarBorder->SetPadding(FMargin(0.0f));
	StructureTabBarBorder->SetBrushColor(StructureTabBarColor);
	StructureTabBarBorder->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* CanvasSlot = StructureSelectionCanvasPanel->AddChildToCanvas(StructureTabBarBorder))
	{
		const float ClampedWidthRatio = FMath::Clamp(StructureTabBarWidthViewportRatio, 0.0f, 1.0f);
		const float ClampedCategoryHeightRatio = FMath::Clamp(FMath::Max(CategoryBarHeightViewportRatio, MinimumFamilyCategoryBarHeightRatio), 0.0f, 1.0f);
		const float ClampedFacilityHeightRatio = FMath::Clamp(FMath::Max(FacilityButtonBarHeightViewportRatio, MinimumFamilyFacilityBarHeightRatio), 0.0f, 1.0f);
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

	StructureTabIndicatorSizeBoxes.Reset();
	StructureTabIndicatorImages.Reset();

	RebuildStructureTabIndicators();

	FacilityButtonBarBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("StructureFacilityButtonBarBorder"));
	FacilityButtonBarBorder->SetPadding(FMargin(0.0f));
	FacilityButtonBarBorder->SetBrushColor(FacilityButtonBarColor);
	FacilityButtonBarBorder->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* CanvasSlot = StructureSelectionCanvasPanel->AddChildToCanvas(FacilityButtonBarBorder))
	{
		const float ClampedWidthRatio = FMath::Clamp(FMath::Max(FacilityButtonBarWidthViewportRatio, MinimumFamilyFacilityBarWidthRatio), 0.0f, 1.0f);
		const float ClampedCategoryHeightRatio = FMath::Clamp(FMath::Max(CategoryBarHeightViewportRatio, MinimumFamilyCategoryBarHeightRatio), 0.0f, 1.0f);
		const float ClampedHeightRatio = FMath::Clamp(FMath::Max(FacilityButtonBarHeightViewportRatio, MinimumFamilyFacilityBarHeightRatio), 0.0f, 1.0f);
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
	FacilityButtonRoleTextBlocks.Reset();
	FacilityButtonMetadataTextBlocks.Reset();
	FacilityButtonStatusBadges.Reset();
	FacilityButtonFamilyGlyphs.Reset();

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

		UVerticalBox* FacilityCard = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityCard%d"), DisplayIndex)));
		FacilityCard->SetVisibility(ESlateVisibility::HitTestInvisible);

		UHorizontalBox* FacilityCardHeader = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityCardHeader%d"), DisplayIndex)));
		if (UVerticalBoxSlot* HeaderSlot = FacilityCard->AddChildToVerticalBox(FacilityCardHeader))
		{
			HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		UTextBlock* FacilityButtonRoleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonRole%d"), DisplayIndex)));
		FacilityButtonRoleTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		USRUIThemeLibrary::ApplyTextStyle(
			FacilityButtonRoleTextBlock,
			ESRUITextStyle::Caption,
			ESRUIVisualState::Neutral,
			true);
		FacilityButtonRoleTextBlocks.Add(FacilityButtonRoleTextBlock);
		if (UHorizontalBoxSlot* RoleSlot = FacilityCardHeader->AddChildToHorizontalBox(FacilityButtonRoleTextBlock))
		{
			RoleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RoleSlot->SetVerticalAlignment(VAlign_Center);
		}

		USRResourceGlyphWidget* FacilityFamilyGlyph = WidgetTree->ConstructWidget<USRResourceGlyphWidget>(
			USRResourceGlyphWidget::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonFamilyGlyph%d"), DisplayIndex)));
		FacilityFamilyGlyph->SetGlyphMode(ESRResourceGlyphMode::FamilyOnly);
		FacilityFamilyGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
		FacilityButtonFamilyGlyphs.Add(FacilityFamilyGlyph);
		if (UHorizontalBoxSlot* GlyphSlot = FacilityCardHeader->AddChildToHorizontalBox(FacilityFamilyGlyph))
		{
			GlyphSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			GlyphSlot->SetPadding(FMargin(3.0f, 0.0f, 4.0f, 0.0f));
			GlyphSlot->SetVerticalAlignment(VAlign_Center);
		}

		USRStatusBadgeWidget* FacilityStatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
			USRStatusBadgeWidget::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonStatus%d"), DisplayIndex)));
		FacilityStatusBadge->SetVisibility(ESlateVisibility::HitTestInvisible);
		FacilityButtonStatusBadges.Add(FacilityStatusBadge);
		if (UHorizontalBoxSlot* StatusSlot = FacilityCardHeader->AddChildToHorizontalBox(FacilityStatusBadge))
		{
			StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			StatusSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* FacilityButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonLabel%d"), DisplayIndex)));
		FacilityButtonTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		FacilityButtonTextBlock->SetJustification(ETextJustify::Left);
		FacilityButtonTextBlock->SetAutoWrapText(true);
		USRUIThemeLibrary::ApplyTextStyle(FacilityButtonTextBlock, ESRUITextStyle::Heading);
		FacilityButtonTextBlocks.Add(FacilityButtonTextBlock);
		if (UVerticalBoxSlot* LabelSlot = FacilityCard->AddChildToVerticalBox(FacilityButtonTextBlock))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* FacilityButtonMetadataTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("StructureFacilityButtonMetadata%d"), DisplayIndex)));
		FacilityButtonMetadataTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		FacilityButtonMetadataTextBlock->SetJustification(ETextJustify::Left);
		FacilityButtonMetadataTextBlock->SetAutoWrapText(true);
		USRUIThemeLibrary::ApplyTextStyle(FacilityButtonMetadataTextBlock, ESRUITextStyle::Caption);
		FacilityButtonMetadataTextBlocks.Add(FacilityButtonMetadataTextBlock);
		if (UVerticalBoxSlot* MetadataSlot = FacilityCard->AddChildToVerticalBox(FacilityButtonMetadataTextBlock))
		{
			MetadataSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			MetadataSlot->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
		}

		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(FacilityButton->AddChild(FacilityCard)))
		{
			ButtonSlot->SetPadding(FMargin(9.0f, 7.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
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
		const float ClampedWidthRatio = FMath::Clamp(FMath::Max(CategoryBarWidthViewportRatio, MinimumFamilyCategoryBarWidthRatio), 0.0f, 1.0f);
		const float ClampedHeightRatio = FMath::Clamp(FMath::Max(CategoryBarHeightViewportRatio, MinimumFamilyCategoryBarHeightRatio), 0.0f, 1.0f);
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
		else if (ButtonIndex == 4)
		{
			CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory5Clicked);
			CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory5Hovered);
		}
		else if (ButtonIndex == 5)
		{
			CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory6Clicked);
			CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory6Hovered);
		}
		else if (ButtonIndex == 6)
		{
			CategoryButton->OnClicked.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory7Clicked);
			CategoryButton->OnHovered.AddDynamic(this, &USRStructureSelectionWidget::HandleCategory7Hovered);
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
		LabelTextBlock->SetText(FText::GetEmpty());
		LabelTextBlock->SetJustification(ETextJustify::Center);
		LabelTextBlock->SetAutoWrapText(false);
		FSlateFontInfo LabelFont = LabelTextBlock->GetFont();
		LabelFont.Size = USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle::Caption);
		LabelTextBlock->SetFont(LabelFont);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(
			USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Neutral).PrimaryTextColor));
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
	EntryActions.Reserve(VisibleBuildOptionIds.Num());

	for (const FName StructureId : VisibleBuildOptionIds)
	{
		const FSRStructureBuildOption* BuildOptionPtr = FindBuildOption(StructureId);
		if (!BuildOptionPtr)
		{
			continue;
		}
		const FSRStructureBuildOption& BuildOption = *BuildOptionPtr;

		UButton* BuildOptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		const bool bSelectable = BuildOption.IsSelectable();
		BuildOptionButton->SetIsEnabled(bSelectable);
		BuildOptionButton->SetBackgroundColor(
			bSelectable
				? (bHasSelectedStructureId && SelectedStructureId == BuildOption.StructureId ? SelectedButtonColor : ButtonColor)
				: DisabledButtonColor);
		BuildOptionButton->SetToolTipText(FSRStructureBuildCatalogBuilder::BuildToolTipText(BuildOption));

		UVerticalBox* BuildOptionVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		BuildOptionButton->AddChild(BuildOptionVerticalBox);

		UTextBlock* NameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameTextBlock->SetText(GetBuildOptionButtonLabel(BuildOption));
		const FSRUIStatePalette OptionPalette = USRUIThemeLibrary::ResolveStatePalette(
			bSelectable
				? ESRUIVisualState::Neutral
				: USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(BuildOption.Availability));
		NameTextBlock->SetColorAndOpacity(FSlateColor(OptionPalette.PrimaryTextColor));
		FSlateFontInfo NameFont = NameTextBlock->GetFont();
		NameFont.Size = USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle::Heading);
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
			DescriptionTextBlock->SetColorAndOpacity(FSlateColor(OptionPalette.SecondaryTextColor));
			FSlateFontInfo DescriptionFont = DescriptionTextBlock->GetFont();
			DescriptionFont.Size = USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle::Body);
			DescriptionTextBlock->SetFont(DescriptionFont);
			DescriptionTextBlock->SetAutoWrapText(true);
			if (UVerticalBoxSlot* DescriptionSlot = BuildOptionVerticalBox->AddChildToVerticalBox(DescriptionTextBlock))
			{
				DescriptionSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 7.0f));
			}
		}

		const FText StatusText = FSRStructureBuildCatalogBuilder::BuildStatusText(BuildOption);
		if (!StatusText.IsEmpty())
		{
			UTextBlock* StatusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			StatusTextBlock->SetText(StatusText);
			StatusTextBlock->SetColorAndOpacity(FSlateColor(OptionPalette.AccentColor));
			FSlateFontInfo StatusFont = StatusTextBlock->GetFont();
			StatusFont.Size = USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle::Caption);
			StatusTextBlock->SetFont(StatusFont);
			StatusTextBlock->SetAutoWrapText(true);
			if (UVerticalBoxSlot* StatusSlot = BuildOptionVerticalBox->AddChildToVerticalBox(StatusTextBlock))
			{
				StatusSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 7.0f));
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
	FSRStructureBuildDockModel::BuildFamilyTabs(
		BuildOptions,
		FamilyTabs);

	const int32 FilterTabIndex = FamilyTabs.IndexOfByPredicate(
		[this](const FSRStructureBuildFamilyTab& Tab)
		{
			return Tab.Filter == SelectedFamilyFilter;
		});
	if (!FamilyTabs.IsValidIndex(SelectedCategoryIndex)
		|| FamilyTabs[SelectedCategoryIndex].Filter != SelectedFamilyFilter)
	{
		SelectedCategoryIndex = FilterTabIndex;
	}

	if (!IsStructureCategoryAvailable(SelectedCategoryIndex))
	{
		SelectedCategoryIndex = FamilyTabs.IndexOfByPredicate(
			[](const FSRStructureBuildFamilyTab& Tab)
			{
				return Tab.HasOptions();
			});
	}

	VisibleBuildOptionIds.Reset();
	if (FamilyTabs.IsValidIndex(SelectedCategoryIndex))
	{
		SelectedFamilyFilter = FamilyTabs[SelectedCategoryIndex].Filter;
		FSRStructureBuildDockModel::QueryOptions(
			BuildOptions,
			SelectedFamilyFilter,
			bIncludeSharedWorkflowOptionsInFamilyTabs,
			VisibleBuildOptionIds);
	}

	if (SelectedFacilityButtonIndex != INDEX_NONE
		&& !IsBuildOptionSelectable(GetFacilityButtonStructureId(SelectedFacilityButtonIndex)))
	{
		SelectedFacilityButtonIndex = INDEX_NONE;
	}
	HoveredFacilityButtonIndex = INDEX_NONE;

	const int32 PageCount = GetSelectedFacilityPageCount();
	SelectedStructureTabIndex = PageCount > 0
		? FMath::Clamp(SelectedStructureTabIndex, 0, PageCount - 1)
		: 0;
}

void USRStructureSelectionWidget::RebuildBuildOptionIndex()
{
	BuildOptionIndexByStructureId.Reset();
	BuildOptionIndexByStructureId.Reserve(BuildOptions.Num());

	for (int32 BuildOptionIndex = 0; BuildOptionIndex < BuildOptions.Num(); ++BuildOptionIndex)
	{
		const FName StructureId = BuildOptions[BuildOptionIndex].StructureId;
		if (StructureId.IsNone() || BuildOptionIndexByStructureId.Contains(StructureId))
		{
			continue;
		}

		BuildOptionIndexByStructureId.Add(StructureId, BuildOptionIndex);
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
	const float ButtonWidth = FMath::Max(FamilyTabMinimumWidth, ButtonLength * 1.45f);
	const float ButtonGapLength = FMath::Max(0.0f, BarHeight * (1.0f - ButtonHeightRatio) * 0.5f);
	const float IconLength = FMath::Max(1.0f, ButtonLength * FMath::Clamp(CategoryButtonIconRatio, 0.0f, 0.38f));
	const float LabelLength = FMath::Max(1.0f, ButtonLength * FMath::Clamp(FMath::Max(CategoryButtonLabelRatio, 0.32f), 0.0f, 1.0f));

	for (USizeBox* CategoryButtonSizeBox : CategoryButtonSizeBoxes)
	{
		if (!CategoryButtonSizeBox)
		{
			continue;
		}

		CategoryButtonSizeBox->SetWidthOverride(ButtonWidth);
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
	const float ButtonHeight = FMath::Max(FacilityCardMinimumHeight, BarHeight * ButtonHeightRatio);
	const float ButtonGapLength = FMath::Max(6.0f, BarHeight * (1.0f - ButtonHeightRatio) * 0.5f);
	const float AvailableWidth = BarLocalSize.X > UE_SMALL_NUMBER
		? FMath::Max(0.0f, BarLocalSize.X - ButtonGapLength * static_cast<float>(FacilityButtonCount - 1))
		: FacilityCardMinimumWidth * static_cast<float>(FacilityButtonCount);
	const float ButtonWidth = FMath::Max(
		FacilityCardMinimumWidth,
		AvailableWidth / static_cast<float>(FacilityButtonCount));

	for (USizeBox* FacilityButtonSizeBox : FacilityButtonSizeBoxes)
	{
		if (!FacilityButtonSizeBox)
		{
			continue;
		}

		FacilityButtonSizeBox->SetWidthOverride(ButtonWidth);
		FacilityButtonSizeBox->SetHeightOverride(ButtonHeight);
	}

	for (USizeBox* FacilityButtonGapSizeBox : FacilityButtonGapSizeBoxes)
	{
		if (!FacilityButtonGapSizeBox)
		{
			continue;
		}

		FacilityButtonGapSizeBox->SetWidthOverride(ButtonGapLength);
		FacilityButtonGapSizeBox->SetHeightOverride(ButtonHeight);
	}
}

void USRStructureSelectionWidget::SyncDetailPanelLayout()
{
	if (!StructureDetailBorder)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(StructureDetailBorder->Slot))
	{
		const float WidthRatio = FMath::Clamp(
			FMath::Max(DetailPanelWidthViewportRatio, MinimumDetailPanelWidthRatio),
			0.0f,
			1.0f);
		const float DetailHeight = FMath::Clamp(
			FMath::Max(DetailPanelHeightViewportRatio, MinimumDetailPanelHeightRatio),
			0.0f,
			1.0f);
		const float CategoryHeight = FMath::Clamp(
			FMath::Max(CategoryBarHeightViewportRatio, MinimumFamilyCategoryBarHeightRatio),
			0.0f,
			1.0f);
		const float FacilityHeight = FMath::Clamp(
			FMath::Max(FacilityButtonBarHeightViewportRatio, MinimumFamilyFacilityBarHeightRatio),
			0.0f,
			1.0f);
		const float TabHeight = FMath::Clamp(StructureTabBarHeightViewportRatio, 0.0f, 1.0f);
		const float Left = (1.0f - WidthRatio) * 0.5f;
		const float Bottom = FMath::Clamp(1.0f - CategoryHeight - FacilityHeight - TabHeight, 0.0f, 1.0f);
		const float Top = FMath::Clamp(Bottom - DetailHeight, 0.0f, 1.0f);
		CanvasSlot->SetAnchors(FAnchors(Left, Top, 1.0f - Left, Bottom));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}
}

void USRStructureSelectionWidget::SyncStructureTabBarLayout()
{
	if (!StructureTabBarBorder)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(StructureTabBarBorder->Slot))
	{
		const int32 VisibleIndicatorCount = FMath::Max(1, StructureTabIndicatorImages.Num());
		const float IndicatorScale = FMath::Max(1.0f, static_cast<float>(VisibleIndicatorCount) / 4.0f);
		const float ClampedWidthRatio = FMath::Clamp(
			FMath::Max(StructureTabBarWidthViewportRatio * IndicatorScale, MinimumStructureTabBarWidthRatio),
			MinimumStructureTabBarWidthRatio,
			MaximumStructureTabBarWidthRatio);
		const float ClampedCategoryHeightRatio = FMath::Clamp(FMath::Max(CategoryBarHeightViewportRatio, MinimumFamilyCategoryBarHeightRatio), 0.0f, 1.0f);
		const float ClampedFacilityHeightRatio = FMath::Clamp(FMath::Max(FacilityButtonBarHeightViewportRatio, MinimumFamilyFacilityBarHeightRatio), 0.0f, 1.0f);
		const float ClampedTabHeightRatio = FMath::Clamp(StructureTabBarHeightViewportRatio, 0.0f, 1.0f);
		const float LeftAnchor = (1.0f - ClampedWidthRatio) * 0.5f;
		const float RightAnchor = 1.0f - LeftAnchor;
		const float BottomAnchor = 1.0f - ClampedCategoryHeightRatio - ClampedFacilityHeightRatio;
		const float TopAnchor = FMath::Clamp(BottomAnchor - ClampedTabHeightRatio, 0.0f, 1.0f);
		CanvasSlot->SetAnchors(FAnchors(LeftAnchor, TopAnchor, RightAnchor, FMath::Clamp(BottomAnchor, 0.0f, 1.0f)));
		CanvasSlot->SetOffsets(FMargin(0.0f));
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

void USRStructureSelectionWidget::RebuildStructureTabIndicators()
{
	if (!WidgetTree || !StructureTabIndicatorRowBox)
	{
		return;
	}

	StructureTabIndicatorRowBox->ClearChildren();
	StructureTabIndicatorSizeBoxes.Reset();
	StructureTabIndicatorImages.Reset();
	StructurePageTextBlock = nullptr;
	StructureTabIndicatorFirstPageIndex = 0;

	const int32 PageCount = GetSelectedFacilityPageCount();
	if (PageCount <= 0)
	{
		SelectedStructureTabIndex = 0;
		return;
	}
	SelectedStructureTabIndex = FMath::Clamp(SelectedStructureTabIndex, 0, PageCount - 1);
	const FSRUIPageWindow PageWindow = FSRUILayoutPolicy::ResolvePageWindow(
		PageCount,
		SelectedStructureTabIndex,
		FMath::Max(1, MaximumVisiblePageIndicators));
	StructureTabIndicatorFirstPageIndex = PageWindow.FirstPageIndex;

	auto AddTabEqualSpacer = [this]()
	{
		if (!WidgetTree || !StructureTabIndicatorRowBox)
		{
			return;
		}

		USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		if (UHorizontalBoxSlot* SpacerSlot = StructureTabIndicatorRowBox->AddChildToHorizontalBox(Spacer))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};

	AddTabEqualSpacer();
	for (int32 LocalTabIndex = 0; LocalTabIndex < PageWindow.IndicatorCount; ++LocalTabIndex)
	{
		USizeBox* TabIndicatorSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("StructurePageIndicatorSizeBox%d"), LocalTabIndex + 1)));
		TabIndicatorSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		StructureTabIndicatorSizeBoxes.Add(TabIndicatorSizeBox);

		UImage* TabIndicatorImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			FName(*FString::Printf(TEXT("StructurePageIndicatorImage%d"), LocalTabIndex + 1)));
		TabIndicatorImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		StructureTabIndicatorImages.Add(TabIndicatorImage);
		TabIndicatorSizeBox->AddChild(TabIndicatorImage);

		if (UHorizontalBoxSlot* IndicatorSlot = StructureTabIndicatorRowBox->AddChildToHorizontalBox(TabIndicatorSizeBox))
		{
			IndicatorSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			IndicatorSlot->SetVerticalAlignment(VAlign_Center);
		}

		AddTabEqualSpacer();
	}

	StructurePageTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("StructurePageTextBlock"));
	StructurePageTextBlock->SetText(FText::FromString(PageWindow.PageLabel));
	StructurePageTextBlock->SetJustification(ETextJustify::Center);
	StructurePageTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	USRUIThemeLibrary::ApplyTextStyle(
		StructurePageTextBlock,
		ESRUITextStyle::Caption,
		ESRUIVisualState::Info,
		true);
	if (UHorizontalBoxSlot* PageTextSlot = StructureTabIndicatorRowBox->AddChildToHorizontalBox(StructurePageTextBlock))
	{
		PageTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		PageTextSlot->SetVerticalAlignment(VAlign_Center);
		PageTextSlot->SetPadding(FMargin(10.0f, 0.0f));
	}
	AddTabEqualSpacer();

	RefreshStructureTabIndicatorBrushes();
	RefreshStructureTabIndicatorStyles();
	SyncStructureTabBarLayout();
}

void USRStructureSelectionWidget::RefreshFacilityButtonBarVisibility()
{
	if (!FacilityButtonBarBorder)
	{
		return;
	}

	const int32 PageCount = GetSelectedFacilityPageCount();
	const bool bShouldShowFacilityButtons = IsStructureCategoryAvailable(SelectedCategoryIndex)
		&& PageCount > 0;
	if (bShouldShowFacilityButtons && PageCount > 0)
	{
		SelectedStructureTabIndex = FMath::Clamp(SelectedStructureTabIndex, 0, PageCount - 1);
	}
	else
	{
		SelectedStructureTabIndex = 0;
	}
	RebuildStructureTabIndicators();

	FacilityButtonBarBorder->SetVisibility(bShouldShowFacilityButtons ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (StructureTabBarBorder)
	{
		StructureTabBarBorder->SetVisibility(bShouldShowFacilityButtons && PageCount > 1
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (!bShouldShowFacilityButtons)
	{
		SelectedFacilityButtonIndex = INDEX_NONE;
		HoveredFacilityButtonIndex = INDEX_NONE;
	}

	RefreshFacilityButtonLabels();
	RefreshFacilityButtonStyles();
	RefreshBuildOptionDetail();
}

void USRStructureSelectionWidget::RefreshFacilityButtonLabels()
{
	for (int32 ButtonIndex = 0; ButtonIndex < FacilityButtonTextBlocks.Num(); ++ButtonIndex)
	{
		const FName StructureId = GetFacilityButtonStructureId(ButtonIndex);
		const FSRStructureBuildOption* BuildOption = FindBuildOption(StructureId);
		const FSRStructureBuildCardPresentation Presentation = BuildOption
			? FSRStructureBuildPresentationBuilder::BuildCard(*BuildOption)
			: FSRStructureBuildCardPresentation();
		const FSRStructureBuildRecommendationPresentation Recommendation = BuildOption
			? FSRStructureBuildPresentationBuilder::BuildRecommendation(
				*BuildOption,
				BuildRecommendationContext)
			: FSRStructureBuildRecommendationPresentation();

		if (UTextBlock* FacilityButtonTextBlock = FacilityButtonTextBlocks[ButtonIndex])
		{
			FacilityButtonTextBlock->SetText(Presentation.DisplayName);
		}
		if (FacilityButtonRoleTextBlocks.IsValidIndex(ButtonIndex)
			&& FacilityButtonRoleTextBlocks[ButtonIndex])
		{
			FacilityButtonRoleTextBlocks[ButtonIndex]->SetText(Presentation.RoleText);
		}
		if (FacilityButtonMetadataTextBlocks.IsValidIndex(ButtonIndex)
			&& FacilityButtonMetadataTextBlocks[ButtonIndex])
		{
			FacilityButtonMetadataTextBlocks[ButtonIndex]->SetText(Presentation.MetadataText);
		}
		if (FacilityButtonStatusBadges.IsValidIndex(ButtonIndex)
			&& FacilityButtonStatusBadges[ButtonIndex])
		{
			FacilityButtonStatusBadges[ButtonIndex]->SetBadge(
				Recommendation.bVisible ? Recommendation.BadgeText : Presentation.AvailabilityText,
				Recommendation.bVisible
					? Recommendation.VisualState
					: BuildOption
					? USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(Presentation.Availability)
					: ESRUIVisualState::Disabled);
		}
		if (FacilityButtonFamilyGlyphs.IsValidIndex(ButtonIndex)
			&& FacilityButtonFamilyGlyphs[ButtonIndex])
		{
			FacilityButtonFamilyGlyphs[ButtonIndex]->SetFamily(
				BuildOption ? BuildOption->ResourceFamily : ESRResourceFamily::None);
			FacilityButtonFamilyGlyphs[ButtonIndex]->SetVisibility(
				BuildOption ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (FacilityButtonSizeBoxes.IsValidIndex(ButtonIndex)
			&& FacilityButtonSizeBoxes[ButtonIndex])
		{
			FacilityButtonSizeBoxes[ButtonIndex]->SetVisibility(
				BuildOption ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}

		if (FacilityButtons.IsValidIndex(ButtonIndex))
		{
			if (UButton* FacilityButton = FacilityButtons[ButtonIndex])
			{
				FacilityButton->SetToolTipText(BuildOption
					? FSRStructureBuildCatalogBuilder::BuildToolTipText(*BuildOption)
					: FText::GetEmpty());
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
			const FSRStructureBuildOption* BuildOption = FindBuildOption(StructureId);
			const bool bEnabled = IsBuildOptionSelectable(StructureId);
			const bool bSelected = bEnabled
				&& bHasSelectedStructureId
				&& SelectedStructureId == StructureId;
			const bool bHovered = BuildOption && ButtonIndex == HoveredFacilityButtonIndex;
			const bool bRecommended = BuildOption
				&& FSRStructureBuildPresentationBuilder::BuildRecommendation(
					*BuildOption,
					BuildRecommendationContext).bVisible;
			const ESRUIVisualState AvailabilityState = BuildOption
				? USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(BuildOption->Availability)
				: ESRUIVisualState::Disabled;
			const ESRUIVisualState CardState = bSelected
				? ESRUIVisualState::Selected
				: bHovered
					? (bEnabled ? ESRUIVisualState::Hovered : AvailabilityState)
					: bRecommended
						? ESRUIVisualState::Warning
						: (bEnabled ? ESRUIVisualState::Neutral : AvailabilityState);
			const FLinearColor FamilyAccent = BuildOption
				? USRUIThemeLibrary::ResolveFamilyAccentColor(BuildOption->ResourceFamily)
				: USRUIThemeLibrary::ResolveFamilyAccentColor(ESRResourceFamily::None);
			const FSRUIStatePalette CardPalette = USRUIThemeLibrary::ResolveStatePalette(CardState);
			FacilityButton->SetIsEnabled(bEnabled);
			FacilityButton->SetBackgroundColor(FLinearColor::LerpUsingHSV(
				CardPalette.SurfaceColor,
				FamilyAccent,
				bSelected ? 0.34f : bHovered ? 0.22f : 0.10f));

			if (FacilityButtonTextBlocks.IsValidIndex(ButtonIndex))
			{
				if (UTextBlock* FacilityButtonTextBlock = FacilityButtonTextBlocks[ButtonIndex])
				{
					FacilityButtonTextBlock->SetColorAndOpacity(FSlateColor(
						CardPalette.PrimaryTextColor));
				}
			}
			if (FacilityButtonRoleTextBlocks.IsValidIndex(ButtonIndex)
				&& FacilityButtonRoleTextBlocks[ButtonIndex])
			{
				FacilityButtonRoleTextBlocks[ButtonIndex]->SetColorAndOpacity(FSlateColor(
					bEnabled ? FamilyAccent : CardPalette.SecondaryTextColor));
			}
			if (FacilityButtonMetadataTextBlocks.IsValidIndex(ButtonIndex)
				&& FacilityButtonMetadataTextBlocks[ButtonIndex])
			{
				FacilityButtonMetadataTextBlocks[ButtonIndex]->SetColorAndOpacity(FSlateColor(
					CardPalette.SecondaryTextColor));
			}
			if (FacilityButtonStatusBadges.IsValidIndex(ButtonIndex)
				&& FacilityButtonStatusBadges[ButtonIndex])
			{
				if (bSelected)
				{
					FacilityButtonStatusBadges[ButtonIndex]->SetBadge(
						NSLOCTEXT("StarRoversBuildDock", "SelectedCardBadge", "SELECTED"),
						ESRUIVisualState::Selected);
				}
				else if (BuildOption)
				{
					const FSRStructureBuildRecommendationPresentation Recommendation =
						FSRStructureBuildPresentationBuilder::BuildRecommendation(
							*BuildOption,
							BuildRecommendationContext);
					FacilityButtonStatusBadges[ButtonIndex]->SetBadge(
						Recommendation.bVisible
							? Recommendation.BadgeText
							: FSRStructureBuildPresentationBuilder::GetAvailabilityLabel(BuildOption->Availability),
						Recommendation.bVisible ? Recommendation.VisualState : AvailabilityState);
				}
			}
		}
	}
}

void USRStructureSelectionWidget::RefreshBuildRecommendation(bool bRefreshWorldState)
{
	FSRStructureBuildRecommendationContext NewContext;
	if (UWorld* World = GetWorld())
	{
		if (USRRunMilestoneSubsystem* MilestoneSubsystem =
			World->GetSubsystem<USRRunMilestoneSubsystem>())
		{
			if (bRefreshWorldState)
			{
				MilestoneSubsystem->RefreshFromWorld();
			}

			const FSRFirstFuelMilestoneSnapshot Snapshot =
				MilestoneSubsystem->GetFirstFuelMilestoneSnapshot();
			ESRStructureBuildRole RecommendedRole = ESRStructureBuildRole::General;
			ESRResourceFamily PreferredFamily = ESRResourceFamily::None;
			bool bHasBuildObjective = Snapshot.bIsTracking;
			switch (Snapshot.CurrentMilestone)
			{
			case ESRFirstFuelMilestone::PlaceExtractor:
				RecommendedRole = ESRStructureBuildRole::Extraction;
				NewContext.ObjectiveText = NSLOCTEXT(
					"StarRoversBuildDock",
					"RecommendExtractor",
					"PLACE ON THE RECOMMENDED DEPOSIT");
				break;
			case ESRFirstFuelMilestone::PlaceFamilyProcessor:
				RecommendedRole = ESRStructureBuildRole::FamilyProcessing;
				PreferredFamily = Snapshot.FirstResourceFamily;
				bHasBuildObjective = PreferredFamily != ESRResourceFamily::None;
				NewContext.ObjectiveText = FText::Format(
					NSLOCTEXT("StarRoversBuildDock", "RecommendFamilyProcessor", "PROCESS THE FIRST {0} CARD"),
					FSRStructureBuildPresentationBuilder::GetFamilyLabel(PreferredFamily));
				break;
			case ESRFirstFuelMilestone::PlaceStellarFuelFabricator:
				RecommendedRole = ESRStructureBuildRole::StellarFuelFabrication;
				NewContext.ObjectiveText = NSLOCTEXT(
					"StarRoversBuildDock",
					"RecommendFuelFabricator",
					"COMBINE THE FIRST FIVE-CARD HAND");
				break;
			case ESRFirstFuelMilestone::PlaceHub:
				RecommendedRole = ESRStructureBuildRole::Hub;
				NewContext.ObjectiveText = NSLOCTEXT(
					"StarRoversBuildDock",
					"RecommendHub",
					"ROUTE STELLAR FUEL OFF-WORLD");
				break;
			case ESRFirstFuelMilestone::ExtractFirstCard:
			case ESRFirstFuelMilestone::ProcessFirstCard:
			case ESRFirstFuelMilestone::FabricateFirstStellarFuel:
			case ESRFirstFuelMilestone::LaunchFirstStellarFuel:
			case ESRFirstFuelMilestone::DeliverFirstStellarFuel:
			case ESRFirstFuelMilestone::Complete:
			default:
				bHasBuildObjective = false;
				break;
			}

			if (bHasBuildObjective)
			{
				NewContext.RecommendedStructureId =
					FSRStructureBuildDockModel::FindRecommendedOptionId(
						BuildOptions,
						RecommendedRole,
						PreferredFamily);
				NewContext.bActive = !NewContext.RecommendedStructureId.IsNone();
				NewContext.CurrentStep = FMath::Clamp(
					Snapshot.CompletedMilestoneCount + 1,
					1,
					FMath::Max(1, Snapshot.TotalMilestoneCount));
				NewContext.TotalSteps = Snapshot.TotalMilestoneCount;
			}
		}
	}

	const bool bChanged = BuildRecommendationContext.bActive != NewContext.bActive
		|| BuildRecommendationContext.RecommendedStructureId != NewContext.RecommendedStructureId
		|| BuildRecommendationContext.CurrentStep != NewContext.CurrentStep
		|| BuildRecommendationContext.TotalSteps != NewContext.TotalSteps
		|| !BuildRecommendationContext.ObjectiveText.EqualTo(NewContext.ObjectiveText);
	BuildRecommendationContext = MoveTemp(NewContext);
	if (bChanged)
	{
		RefreshFacilityButtonLabels();
		RefreshFacilityButtonStyles();
		RefreshBuildOptionDetail();
	}
}

void USRStructureSelectionWidget::RefreshBuildOptionDetail()
{
	if (!StructureDetailBorder
		|| !DetailTitleTextBlock
		|| !DetailClassificationTextBlock
		|| !DetailSpecificationTextBlock
		|| !DetailDescriptionTextBlock
		|| !DetailAvailabilityBadge
		|| !DetailFamilyGlyph
		|| !DetailRecommendationRow
		|| !DetailRecommendationBadge
		|| !DetailRecommendationTextBlock
		|| !DetailFlowPanel
		|| !DetailFlowInputBadge
		|| !DetailFlowProcessBadge
		|| !DetailFlowOutputBadge
		|| !DetailFlowEffectTextBlock
		|| !DetailPlacementMetricRow
		|| !DetailPlacementTargetBadge
		|| !DetailPlacementFootprintBadge
		|| !DetailPlacementCapacityBadge
		|| !DetailPlacementStatusTextBlock
		|| !DetailPlacementTextBlock)
	{
		return;
	}

	auto HideDecisionRows = [this]()
	{
		DetailRecommendationRow->SetVisibility(ESlateVisibility::Collapsed);
		DetailFlowPanel->SetVisibility(ESlateVisibility::Collapsed);
		DetailFlowEffectTextBlock->SetVisibility(ESlateVisibility::Collapsed);
		DetailPlacementMetricRow->SetVisibility(ESlateVisibility::Collapsed);
		DetailPlacementStatusTextBlock->SetVisibility(ESlateVisibility::Collapsed);
		DetailPlacementTextBlock->SetVisibility(ESlateVisibility::Collapsed);
		DetailPlacementStatusTextBlock->SetText(FText::GetEmpty());
		DetailPlacementTextBlock->SetText(FText::GetEmpty());
	};

	StructureDetailBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const FSRStructureBuildOption* BuildOption = GetDetailBuildOption();
	int32 SelectableVisibleOptionCount = 0;
	for (const FName StructureId : VisibleBuildOptionIds)
	{
		const FSRStructureBuildOption* VisibleOption = FindBuildOption(StructureId);
		SelectableVisibleOptionCount += VisibleOption && VisibleOption->IsSelectable() ? 1 : 0;
	}
	const FSRStructureBuildEmptyStatePresentation EmptyState =
		FSRStructureBuildPresentationBuilder::BuildEmptyState(
			BuildOptions.Num(),
			VisibleBuildOptionIds.Num(),
			SelectableVisibleOptionCount);
	if (!BuildOption && EmptyState.bVisible)
	{
		HideDecisionRows();
		DetailDescriptionTextBlock->SetAutoWrapText(true);
		DetailFamilyGlyph->SetVisibility(ESlateVisibility::Collapsed);
		DetailTitleTextBlock->SetText(EmptyState.Title);
		DetailClassificationTextBlock->SetText(EmptyState.ClassificationText);
		DetailSpecificationTextBlock->SetText(EmptyState.DetailText);
		DetailDescriptionTextBlock->SetText(EmptyState.ActionText);
		DetailAvailabilityBadge->SetBadge(EmptyState.BadgeText, EmptyState.VisualState);
		StructureDetailBorder->SetToolTipText(EmptyState.DetailText);
		USRUIThemeLibrary::ApplyCardStyle(
			StructureDetailBorder,
			EmptyState.VisualState,
			FMargin(14.0f, 10.0f));
		DetailClassificationTextBlock->SetColorAndOpacity(FSlateColor(
			USRUIThemeLibrary::ResolveStatePalette(EmptyState.VisualState).AccentColor));
		return;
	}
	if (!BuildOption)
	{
		HideDecisionRows();
		DetailDescriptionTextBlock->SetAutoWrapText(true);
		DetailFamilyGlyph->SetVisibility(ESlateVisibility::Collapsed);
		DetailTitleTextBlock->SetText(NSLOCTEXT("StarRoversBuildDock", "DetailPromptTitle", "Build Dock"));
		DetailClassificationTextBlock->SetText(NSLOCTEXT("StarRoversBuildDock", "DetailPromptClass", "FAMILY WORKSPACE"));
		DetailSpecificationTextBlock->SetText(FText::GetEmpty());
		DetailDescriptionTextBlock->SetText(
			NSLOCTEXT("StarRoversBuildDock", "DetailPrompt", "Hover a structure card for specifications, or select one to inspect live placement."));
		DetailAvailabilityBadge->SetBadge(
			NSLOCTEXT("StarRoversBuildDock", "DetailBrowseBadge", "BROWSE"),
			ESRUIVisualState::Info);
		StructureDetailBorder->SetToolTipText(FText::GetEmpty());
		USRUIThemeLibrary::ApplyCardStyle(
			StructureDetailBorder,
			ESRUIVisualState::Neutral,
			FMargin(14.0f, 10.0f));
		return;
	}

	const FSRStructureBuildDetailPresentation Presentation =
		FSRStructureBuildPresentationBuilder::BuildDetail(*BuildOption);
	DetailFamilyGlyph->SetFamily(BuildOption->ResourceFamily);
	DetailFamilyGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
	DetailTitleTextBlock->SetText(Presentation.Title);
	DetailClassificationTextBlock->SetText(Presentation.ClassificationText);
	DetailSpecificationTextBlock->SetText(Presentation.SpecificationText);
	DetailDescriptionTextBlock->SetAutoWrapText(false);
	DetailDescriptionTextBlock->SetText(Presentation.Description.IsEmpty()
		? NSLOCTEXT("StarRoversBuildDock", "NoDescription", "No authored description.")
		: Presentation.Description);
	DetailAvailabilityBadge->SetBadge(
		Presentation.AvailabilityText,
		USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(Presentation.Availability));

	const FSRStructureBuildRecommendationPresentation Recommendation =
		FSRStructureBuildPresentationBuilder::BuildRecommendation(
			*BuildOption,
			BuildRecommendationContext);
	DetailRecommendationRow->SetVisibility(Recommendation.bVisible
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (Recommendation.bVisible)
	{
		DetailRecommendationBadge->SetBadge(Recommendation.BadgeText, Recommendation.VisualState);
		DetailRecommendationTextBlock->SetText(Recommendation.ReasonText);
		DetailRecommendationRow->SetToolTipText(Recommendation.ReasonText);
	}

	const FSRStructureBuildFlowPresentation Flow =
		FSRStructureBuildPresentationBuilder::BuildFlow(*BuildOption);
	DetailFlowPanel->SetVisibility(Flow.bVisible
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	DetailFlowEffectTextBlock->SetVisibility(Flow.bVisible && !Flow.EffectText.IsEmpty()
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (Flow.bVisible)
	{
		DetailFlowInputBadge->SetBadge(Flow.InputText, ESRUIVisualState::Neutral);
		DetailFlowProcessBadge->SetBadge(Flow.ProcessText, ESRUIVisualState::Info);
		DetailFlowOutputBadge->SetBadge(Flow.OutputText, ESRUIVisualState::Positive);
		DetailFlowEffectTextBlock->SetText(Flow.EffectText);
		DetailFlowPanel->SetToolTipText(Flow.ToolTipText);
		DetailFlowInputBadge->SetToolTipText(Flow.ToolTipText);
		DetailFlowProcessBadge->SetToolTipText(Flow.ToolTipText);
		DetailFlowOutputBadge->SetToolTipText(Flow.ToolTipText);
	}
	TArray<FText> DetailToolTipLines;
	DetailToolTipLines.Add(Presentation.Title);
	if (!Presentation.Description.IsEmpty())
	{
		DetailToolTipLines.Add(Presentation.Description);
	}
	if (!Flow.ToolTipText.IsEmpty())
	{
		DetailToolTipLines.Add(Flow.ToolTipText);
	}
	StructureDetailBorder->SetToolTipText(FText::Join(
		NSLOCTEXT("StarRoversBuildDock", "DetailTooltipSeparator", "\n"),
		DetailToolTipLines));

	const bool bShowingSelectedPlacement = bHasSelectedStructureId
		&& SelectedStructureId == BuildOption->StructureId;
	FSRStructurePlacementPreview LivePreview;
	const FSRStructurePlacementPreview* LivePreviewPtr = nullptr;
	if (bShowingSelectedPlacement)
	{
		const ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer());
		LivePreview = IsValid(PlayerController)
			? PlayerController->GetSelectedStructurePlacementPreview()
			: FSRStructurePlacementPreview();
		LivePreviewPtr = &LivePreview;
	}
	const FSRStructureBuildPlacementPresentation Placement =
		FSRStructureBuildPresentationBuilder::BuildPlacement(*BuildOption, LivePreviewPtr);
	DetailPlacementMetricRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	DetailPlacementTargetBadge->SetBadge(Placement.TargetText, Placement.TargetVisualState);
	DetailPlacementFootprintBadge->SetBadge(Placement.FootprintText, Placement.FootprintVisualState);
	DetailPlacementCapacityBadge->SetBadge(Placement.CapacityText, Placement.CapacityVisualState);
	DetailPlacementStatusTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	DetailPlacementTextBlock->SetVisibility(Placement.DetailText.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible);
	DetailPlacementStatusTextBlock->SetText(bShowingSelectedPlacement
		? NSLOCTEXT("StarRoversBuildDock", "LivePlacementLabel", "LIVE PLACEMENT")
		: NSLOCTEXT("StarRoversBuildDock", "SelectPlacementLabel", "SELECT TO PREVIEW"));
	DetailPlacementTextBlock->SetText(Placement.DetailText);
	DetailPlacementMetricRow->SetToolTipText(Placement.DetailText);
	DetailPlacementTargetBadge->SetToolTipText(Placement.DetailText);
	DetailPlacementFootprintBadge->SetToolTipText(Placement.DetailText);
	DetailPlacementCapacityBadge->SetToolTipText(Placement.DetailText);

	ESRUIVisualState DetailState = USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(
		BuildOption->Availability);
	if (bShowingSelectedPlacement)
	{
		DetailState = Placement.CapacityVisualState == ESRUIVisualState::Warning
			? ESRUIVisualState::Warning
			: Placement.TargetVisualState;
	}
	else if (Recommendation.bVisible)
	{
		DetailState = Recommendation.VisualState;
	}

	USRUIThemeLibrary::ApplyCardStyle(
		StructureDetailBorder,
		DetailState,
		FMargin(14.0f, 10.0f));
	const FLinearColor FamilyAccent = USRUIThemeLibrary::ResolveFamilyAccentColor(BuildOption->ResourceFamily);
	DetailClassificationTextBlock->SetColorAndOpacity(FSlateColor(FamilyAccent));
	DetailFlowEffectTextBlock->SetColorAndOpacity(FSlateColor(FamilyAccent));
	DetailPlacementStatusTextBlock->SetColorAndOpacity(FSlateColor(
		USRUIThemeLibrary::ResolveStatePalette(DetailState).AccentColor));
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
	const int32 PageCount = GetSelectedFacilityPageCount();
	if (PageCount <= 0)
	{
		SelectedStructureTabIndex = 0;
		return;
	}

	SelectedStructureTabIndex = FMath::Clamp(SelectedStructureTabIndex, 0, PageCount - 1);

	for (int32 LocalTabIndex = 0; LocalTabIndex < StructureTabIndicatorImages.Num(); ++LocalTabIndex)
	{
		if (UImage* IndicatorImage = StructureTabIndicatorImages[LocalTabIndex])
		{
			const int32 PageIndex = StructureTabIndicatorFirstPageIndex + LocalTabIndex;
			IndicatorImage->SetVisibility(PageIndex < PageCount ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			IndicatorImage->SetColorAndOpacity(PageIndex == SelectedStructureTabIndex
				? SelectedStructureTabIndicatorColor
				: InactiveStructureTabIndicatorColor);
		}
	}
	if (StructurePageTextBlock)
	{
		StructurePageTextBlock->SetText(FText::FromString(
			FSRUILayoutPolicy::ResolvePageWindow(
				PageCount,
				SelectedStructureTabIndex,
				FMath::Max(1, MaximumVisiblePageIndicators)).PageLabel));
	}
}

void USRStructureSelectionWidget::SetStructureSelectionTabIndex(int32 NewTabIndex)
{
	const int32 PageCount = GetSelectedFacilityPageCount();
	if (PageCount <= 0)
	{
		SelectedStructureTabIndex = 0;
		return;
	}

	SelectedStructureTabIndex = (NewTabIndex % PageCount + PageCount) % PageCount;
	const int32 SelectedVisibleOptionIndex = bHasSelectedStructureId
		? VisibleBuildOptionIds.IndexOfByKey(SelectedStructureId)
		: INDEX_NONE;
	SelectedFacilityButtonIndex = SelectedVisibleOptionIndex != INDEX_NONE
		&& SelectedVisibleOptionIndex / FacilityButtonCount == SelectedStructureTabIndex
			? SelectedVisibleOptionIndex % FacilityButtonCount
			: INDEX_NONE;
	const FSRUIPageWindow PageWindow = FSRUILayoutPolicy::ResolvePageWindow(
		PageCount,
		SelectedStructureTabIndex,
		FMath::Max(1, MaximumVisiblePageIndicators));
	if (StructureTabIndicatorImages.Num() != PageWindow.IndicatorCount
		|| StructureTabIndicatorFirstPageIndex != PageWindow.FirstPageIndex
		|| !StructurePageTextBlock)
	{
		RebuildStructureTabIndicators();
	}
	else
	{
		RefreshStructureTabIndicatorStyles();
	}
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
		case 4:
			ConfiguredBrush = &Category5IconBrush;
			break;
		case 5:
			ConfiguredBrush = &Category6IconBrush;
			break;
		case 6:
			ConfiguredBrush = &Category7IconBrush;
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

void USRStructureSelectionWidget::RefreshCategoryButtonLabels()
{
	for (int32 ButtonIndex = 0; ButtonIndex < CategoryButtonLabelTextBlocks.Num(); ++ButtonIndex)
	{
		UTextBlock* LabelTextBlock = CategoryButtonLabelTextBlocks[ButtonIndex];
		if (!LabelTextBlock)
		{
			continue;
		}

		if (!FamilyTabs.IsValidIndex(ButtonIndex))
		{
			LabelTextBlock->SetText(FText::GetEmpty());
			continue;
		}

		const FSRStructureBuildFamilyTab& Tab = FamilyTabs[ButtonIndex];
		LabelTextBlock->SetText(FText::Format(
			NSLOCTEXT("StarRoversBuildDock", "FamilyTabLabelFormat", "{0}\n{1}/{2}"),
			Tab.Label,
			FText::AsNumber(Tab.SelectableOptionCount),
			FText::AsNumber(Tab.TotalOptionCount)));
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
			const ESRResourceFamily ResourceFamily = FamilyTabs.IsValidIndex(ButtonIndex)
				? FamilyTabs[ButtonIndex].ResourceFamily
				: ESRResourceFamily::None;
			FLinearColor FamilyAccent = USRUIThemeLibrary::ResolveFamilyAccentColor(ResourceFamily);
			if (FamilyTabs.IsValidIndex(ButtonIndex)
				&& FamilyTabs[ButtonIndex].Filter == ESRStructureBuildFamilyFilter::All)
			{
				FamilyAccent = USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Info).AccentColor;
			}

			CategoryButton->SetIsEnabled(bEnabled);
			const FLinearColor BaseColor = bSelected
				? SelectedCategoryButtonColor
				: bHovered
					? HoveredCategoryButtonColor
					: CategoryButtonColor;
			CategoryButton->SetBackgroundColor(bEnabled
				? FLinearColor::LerpUsingHSV(BaseColor, FamilyAccent, bSelected ? 0.55f : bHovered ? 0.35f : 0.14f)
				: DisabledButtonColor);

			if (CategoryButtonImages.IsValidIndex(ButtonIndex) && CategoryButtonImages[ButtonIndex])
			{
				CategoryButtonImages[ButtonIndex]->SetColorAndOpacity(bEnabled
					? FamilyAccent
					: USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Disabled).SecondaryTextColor);
			}
			if (CategoryButtonLabelTextBlocks.IsValidIndex(ButtonIndex)
				&& CategoryButtonLabelTextBlocks[ButtonIndex])
			{
				CategoryButtonLabelTextBlocks[ButtonIndex]->SetColorAndOpacity(FSlateColor(
					USRUIThemeLibrary::ResolveStatePalette(
						bEnabled ? ESRUIVisualState::Neutral : ESRUIVisualState::Disabled).PrimaryTextColor));
			}
			if (FamilyTabs.IsValidIndex(ButtonIndex))
			{
				const FSRStructureBuildFamilyTab& Tab = FamilyTabs[ButtonIndex];
				CategoryButton->SetToolTipText(FText::Format(
					NSLOCTEXT("StarRoversBuildDock", "FamilyTabTooltipFormat", "{0}\nAvailable: {1} / {2}"),
					Tab.Label,
					FText::AsNumber(Tab.SelectableOptionCount),
					FText::AsNumber(Tab.TotalOptionCount)));
			}
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
	SelectedFamilyFilter = FamilyTabs[CategoryIndex].Filter;
	SelectedFacilityButtonIndex = INDEX_NONE;
	SelectedStructureTabIndex = 0;
	FSRStructureBuildDockModel::QueryOptions(
		BuildOptions,
		SelectedFamilyFilter,
		bIncludeSharedWorkflowOptionsInFamilyTabs,
		VisibleBuildOptionIds);
	RebuildBuildOptions();
	RefreshCategoryButtonStyles();
	RefreshFacilityButtonBarVisibility();
	SetStructureSelectionTabIndex(0);
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

void USRStructureSelectionWidget::RevealBuildOptionInDock(FName StructureId)
{
	const FSRStructureBuildOption* BuildOption = FindBuildOption(StructureId);
	if (!BuildOption)
	{
		return;
	}

	int32 VisibleOptionIndex = VisibleBuildOptionIds.IndexOfByKey(StructureId);
	if (VisibleOptionIndex == INDEX_NONE)
	{
		const ESRStructureBuildFamilyFilter PreferredFilter =
			FSRStructureBuildDockModel::ResolvePreferredFilter(*BuildOption);
		const int32 PreferredTabIndex = FamilyTabs.IndexOfByPredicate(
			[PreferredFilter](const FSRStructureBuildFamilyTab& Tab)
			{
				return Tab.Filter == PreferredFilter;
			});
		if (PreferredTabIndex != INDEX_NONE && IsStructureCategoryAvailable(PreferredTabIndex))
		{
			SelectStructureCategory(PreferredTabIndex);
			VisibleOptionIndex = VisibleBuildOptionIds.IndexOfByKey(StructureId);
		}
	}

	if (VisibleOptionIndex != INDEX_NONE)
	{
		SelectedStructureTabIndex = VisibleOptionIndex / FacilityButtonCount;
		SelectedFacilityButtonIndex = VisibleOptionIndex % FacilityButtonCount;
		RebuildStructureTabIndicators();
		RefreshFacilityButtonLabels();
		RefreshFacilityButtonStyles();
	}
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
	return IsStructureCategoryAvailable(SelectedCategoryIndex)
		? &VisibleBuildOptionIds
		: nullptr;
}

int32 USRStructureSelectionWidget::GetSelectedFacilityPageCount() const
{
	const TArray<FName>* FacilityBuildOptionIds = GetSelectedFacilityBuildOptionIds();
	if (!FacilityBuildOptionIds || FacilityBuildOptionIds->IsEmpty() || FacilityButtonCount <= 0)
	{
		return 0;
	}

	return FMath::DivideAndRoundUp(FacilityBuildOptionIds->Num(), FacilityButtonCount);
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
	return FamilyTabs.IsValidIndex(CategoryIndex)
		&& FamilyTabs[CategoryIndex].HasOptions();
}

bool USRStructureSelectionWidget::IsBuildOptionSelectable(FName StructureId) const
{
	const FSRStructureBuildOption* BuildOption = FindBuildOption(StructureId);
	return BuildOption && BuildOption->IsSelectable();
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
			&& FacilityButton->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return ButtonIndex;
		}
	}

	return INDEX_NONE;
}

void USRStructureSelectionWidget::HandleCategory1Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection CategoryButton OnClicked Index=0"));
	SelectStructureCategory(0);
}

void USRStructureSelectionWidget::HandleCategory1Hovered()
{
	SetHoveredStructureCategory(0);
}

void USRStructureSelectionWidget::HandleCategory2Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection CategoryButton OnClicked Index=1"));
	SelectStructureCategory(1);
}

void USRStructureSelectionWidget::HandleCategory2Hovered()
{
	SetHoveredStructureCategory(1);
}

void USRStructureSelectionWidget::HandleCategory3Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection CategoryButton OnClicked Index=2"));
	SelectStructureCategory(2);
}

void USRStructureSelectionWidget::HandleCategory3Hovered()
{
	SetHoveredStructureCategory(2);
}

void USRStructureSelectionWidget::HandleCategory4Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection CategoryButton OnClicked Index=3"));
	SelectStructureCategory(3);
}

void USRStructureSelectionWidget::HandleCategory4Hovered()
{
	SetHoveredStructureCategory(3);
}

void USRStructureSelectionWidget::HandleCategory5Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection CategoryButton OnClicked Index=4"));
	SelectStructureCategory(4);
}

void USRStructureSelectionWidget::HandleCategory5Hovered()
{
	SetHoveredStructureCategory(4);
}

void USRStructureSelectionWidget::HandleCategory6Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection CategoryButton OnClicked Index=5"));
	SelectStructureCategory(5);
}

void USRStructureSelectionWidget::HandleCategory6Hovered()
{
	SetHoveredStructureCategory(5);
}

void USRStructureSelectionWidget::HandleCategory7Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection CategoryButton OnClicked Index=6"));
	SelectStructureCategory(6);
}

void USRStructureSelectionWidget::HandleCategory7Hovered()
{
	SetHoveredStructureCategory(6);
}

void USRStructureSelectionWidget::HandleCategoryUnhovered()
{
	ClearHoveredStructureCategory();
}

void USRStructureSelectionWidget::HandleFacilityButton1Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection FacilityButton OnClicked Index=0"));
	SelectFacilityButton(0);
}

void USRStructureSelectionWidget::HandleFacilityButton1Hovered()
{
	SetHoveredFacilityButton(0);
}

void USRStructureSelectionWidget::HandleFacilityButton2Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection FacilityButton OnClicked Index=1"));
	SelectFacilityButton(1);
}

void USRStructureSelectionWidget::HandleFacilityButton2Hovered()
{
	SetHoveredFacilityButton(1);
}

void USRStructureSelectionWidget::HandleFacilityButton3Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection FacilityButton OnClicked Index=2"));
	SelectFacilityButton(2);
}

void USRStructureSelectionWidget::HandleFacilityButton3Hovered()
{
	SetHoveredFacilityButton(2);
}

void USRStructureSelectionWidget::HandleFacilityButton4Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection FacilityButton OnClicked Index=3"));
	SelectFacilityButton(3);
}

void USRStructureSelectionWidget::HandleFacilityButton4Hovered()
{
	SetHoveredFacilityButton(3);
}

void USRStructureSelectionWidget::HandleFacilityButton5Clicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: StructureSelection FacilityButton OnClicked Index=4"));
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

	const int32* BuildOptionIndex = BuildOptionIndexByStructureId.Find(StructureId);
	return BuildOptionIndex && BuildOptions.IsValidIndex(*BuildOptionIndex)
		? &BuildOptions[*BuildOptionIndex]
		: nullptr;
}

const FSRStructureBuildOption* USRStructureSelectionWidget::GetDetailBuildOption() const
{
	if (HoveredFacilityButtonIndex != INDEX_NONE)
	{
		if (const FSRStructureBuildOption* HoveredOption = FindBuildOption(
			GetFacilityButtonStructureId(HoveredFacilityButtonIndex)))
		{
			return HoveredOption;
		}
	}

	return bHasSelectedStructureId ? FindBuildOption(SelectedStructureId) : nullptr;
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
	const bool bOverDetailPanel = StructureDetailBorder
		&& StructureDetailBorder->IsVisible()
		&& StructureDetailBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);

	return bOverCategoryBar || bOverFacilityButtonBar || bOverStructureTabBar || bOverDetailPanel;
}
