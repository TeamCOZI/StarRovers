#pragma once

#include "Assembly/SRStructureBuildCatalog.h"
#include "Assembly/SRStructureBuildDockModel.h"
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/SRStructureBuildPresentation.h"
#include "SRStructureSelectionWidget.generated.h"

class SWidget;
class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class USRResourceGlyphWidget;
class USRStatusBadgeWidget;
class USRStructureSelectionWidget;
class USRStructureDataAsset;

DECLARE_MULTICAST_DELEGATE_TwoParams(FSRStarRoversStructureBuildOptionSelectedSignature, FName, USRStructureDataAsset*);

UCLASS()
class STARROVERS_API USRStructureSelectionEntryAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRStructureSelectionWidget* InOwnerWidget, FName InStructureId);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<USRStructureSelectionWidget> OwnerWidget;

	UPROPERTY(Transient)
	FName StructureId = NAME_None;
};

UCLASS(Blueprintable)
class STARROVERS_API USRStructureSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetBuildOptions(const TArray<FSRStructureBuildOption>& NewBuildOptions);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetBuildCatalog(const FSRStructureBuildCatalog& NewBuildCatalog);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetBuildOptionsFromDataAssets(const TArray<USRStructureDataAsset*>& StructureDataAssets);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
	TArray<FSRStructureBuildOption> GetBuildOptions() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetSelectedStructureId(FName NewSelectedStructureId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void ClearSelectedStructureId();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
	bool HasSelectedStructureId() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
	FName GetSelectedStructureId() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
	USRStructureDataAsset* GetSelectedStructureDataAsset() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Input")
	bool IsPointerOverStructureSelectionPanel() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetStructureCategoryButtonIconBrush(int32 CategoryIndex, const FSlateBrush& IconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly|Tabs")
	bool AdvanceStructureSelectionTab();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly|Tabs")
	bool RetreatStructureSelectionTab();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Tabs")
	int32 GetBuildDockPageCount() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Tabs")
	int32 GetBuildDockPageIndex() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Tabs")
	int32 GetVisibleBuildDockPageIndicatorCount() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	bool SelectStructureCategoryByShortcut(int32 CategoryIndex);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly|Build Dock")
	bool SelectBuildDockFamily(ESRStructureBuildFamilyFilter FamilyFilter);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Build Dock")
	ESRStructureBuildFamilyFilter GetSelectedBuildDockFamily() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Build Dock")
	TArray<FSRStructureBuildOption> GetVisibleBuildOptions() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Build Dock")
	TArray<FSRStructureBuildFamilyTab> GetBuildDockFamilyTabs() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Build Dock")
	FName GetRecommendedBuildOptionId() const;

	/** Selects the first deterministic, currently available option for a workflow role. */
	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly|Build Dock")
	bool SelectRecommendedBuildOption(
		ESRStructureBuildRole Role,
		ESRResourceFamily PreferredFamily = ESRResourceFamily::None,
		bool bDispatchSelection = true);

	bool TryHandleStructureSelectionPointerClick();

	void DispatchBuildOptionSelected(FName StructureId);
	FSRStarRoversStructureBuildOptionSelectedSignature& OnBuildOptionSelected();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Theme", meta = (
		DisplayName = "Use Shared UI Theme",
		ToolTip = "When enabled, shared Star Rovers theme tokens replace the legacy per-widget color defaults."))
	bool bUseSharedUITheme = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Build Dock", meta = (
		DisplayName = "Include Shared Workflow Options In Family Tabs",
		ToolTip = "Shows shared extraction, logistics, generic processing, Tag, and Fuel Imprint facilities beside Family-specific processors."))
	bool bIncludeSharedWorkflowOptionsInFamilyTabs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "TitleText"))
	FText TitleText = NSLOCTEXT("StarRoversStructureSelection", "TitleText", "Build Dock");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "PanelColor"))
	FLinearColor PanelColor = FLinearColor(0.015f, 0.025f, 0.04f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "ButtonColor"))
	FLinearColor ButtonColor = FLinearColor(0.08f, 0.11f, 0.15f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "SelectedButtonColor"))
	FLinearColor SelectedButtonColor = FLinearColor(0.18f, 0.36f, 0.42f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "DisabledButtonColor"))
	FLinearColor DisabledButtonColor = FLinearColor(0.06f, 0.07f, 0.08f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "CategoryBarWidthViewportRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float CategoryBarWidthViewportRatio = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "CategoryBarHeightViewportRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float CategoryBarHeightViewportRatio = 0.065f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "CategoryButtonHeightRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float CategoryButtonHeightRatio = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "CategoryButtonIconRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float CategoryButtonIconRatio = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "CategoryButtonLabelRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float CategoryButtonLabelRatio = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "FamilyTabMinimumWidth", ClampMin = "48.0"))
	float FamilyTabMinimumWidth = 86.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "CategoryBarColor"))
	FLinearColor CategoryBarColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "CategoryButtonColor"))
	FLinearColor CategoryButtonColor = FLinearColor(0.12f, 0.24f, 0.12f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "SelectedCategoryButtonColor"))
	FLinearColor SelectedCategoryButtonColor = FLinearColor(0.06f, 0.55f, 0.78f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "HoveredCategoryButtonColor"))
	FLinearColor HoveredCategoryButtonColor = FLinearColor(0.18f, 0.44f, 0.20f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "Category1IconBrush"))
	FSlateBrush Category1IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "Category2IconBrush"))
	FSlateBrush Category2IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "Category3IconBrush"))
	FSlateBrush Category3IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "Category4IconBrush"))
	FSlateBrush Category4IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "Category5IconBrush"))
	FSlateBrush Category5IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "Category6IconBrush"))
	FSlateBrush Category6IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "Category7IconBrush"))
	FSlateBrush Category7IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "FacilityButtonBarWidthViewportRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float FacilityButtonBarWidthViewportRatio = 0.58f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "FacilityButtonBarHeightViewportRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float FacilityButtonBarHeightViewportRatio = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "FacilityButtonHeightRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float FacilityButtonHeightRatio = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "FacilityCardMinimumWidth", ClampMin = "96.0"))
	float FacilityCardMinimumWidth = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "FacilityCardMinimumHeight", ClampMin = "64.0"))
	float FacilityCardMinimumHeight = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "FacilityButtonBarColor"))
	FLinearColor FacilityButtonBarColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "FacilityButtonColor"))
	FLinearColor FacilityButtonColor = FLinearColor(0.12f, 0.24f, 0.12f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "SelectedFacilityButtonColor"))
	FLinearColor SelectedFacilityButtonColor = FLinearColor(0.06f, 0.55f, 0.78f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "HoveredFacilityButtonColor"))
	FLinearColor HoveredFacilityButtonColor = FLinearColor(0.18f, 0.44f, 0.20f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Tabs", meta = (DisplayName = "StructureTabBarWidthViewportRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float StructureTabBarWidthViewportRatio = 0.0625f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Tabs", meta = (DisplayName = "StructureTabBarHeightViewportRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float StructureTabBarHeightViewportRatio = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Tabs", meta = (DisplayName = "StructureTabIndicatorHeightRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float StructureTabIndicatorHeightRatio = 2.0f / 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Tabs", meta = (
		DisplayName = "Maximum Visible Page Indicators",
		ClampMin = "1",
		ClampMax = "11",
		ToolTip = "Bounds the number of page dots so very large facility catalogs never expand the Build Dock."))
	int32 MaximumVisiblePageIndicators = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Tabs", meta = (DisplayName = "StructureTabBarColor"))
	FLinearColor StructureTabBarColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Tabs", meta = (DisplayName = "SelectedStructureTabIndicatorColor"))
	FLinearColor SelectedStructureTabIndicatorColor = FLinearColor(0.88f, 0.95f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Tabs", meta = (DisplayName = "InactiveStructureTabIndicatorColor"))
	FLinearColor InactiveStructureTabIndicatorColor = FLinearColor(0.88f, 0.95f, 1.0f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Detail", meta = (DisplayName = "DetailPanelWidthViewportRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float DetailPanelWidthViewportRatio = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Detail", meta = (DisplayName = "DetailPanelHeightViewportRatio", ClampMin = "0.0", ClampMax = "1.0"))
	float DetailPanelHeightViewportRatio = 0.23f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "BuildOptions"))
	TArray<FSRStructureBuildOption> BuildOptions;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "SelectedStructureId"))
	FName SelectedStructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "bHasSelectedStructureId"))
	bool bHasSelectedStructureId = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "SelectedCategoryIndex"))
	int32 SelectedCategoryIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Build Dock", meta = (DisplayName = "SelectedFamilyFilter"))
	ESRStructureBuildFamilyFilter SelectedFamilyFilter = ESRStructureBuildFamilyFilter::All;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Build Dock", meta = (DisplayName = "FamilyTabs"))
	TArray<FSRStructureBuildFamilyTab> FamilyTabs;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Build Dock")
	FSRStructureBuildRecommendationContext BuildRecommendationContext;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|CategoryBar", meta = (DisplayName = "HoveredCategoryIndex"))
	int32 HoveredCategoryIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "SelectedFacilityButtonIndex"))
	int32 SelectedFacilityButtonIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|FacilityButtonBar", meta = (DisplayName = "HoveredFacilityButtonIndex"))
	int32 HoveredFacilityButtonIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Tabs", meta = (DisplayName = "SelectedStructureTabIndex"))
	int32 SelectedStructureTabIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> StructureSelectionBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StructureSelectionVerticalBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedStructureTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> BuildOptionsScrollBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRStructureSelectionEntryAction>> EntryActions;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CategoryBarBorder;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> CategoryButtonRowBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> CategoryButtonSizeBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> CategoryButtonGapSizeBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CategoryButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> CategoryButtonIconSizeBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CategoryButtonImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> CategoryButtonLabelSizeBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CategoryButtonLabelTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> DefaultCategoryIconTextures;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> FacilityButtonBarBorder;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> FacilityButtonRowBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> FacilityButtonSizeBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> FacilityButtonGapSizeBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> FacilityButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FacilityButtonTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FacilityButtonRoleTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FacilityButtonMetadataTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRStatusBadgeWidget>> FacilityButtonStatusBadges;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRResourceGlyphWidget>> FacilityButtonFamilyGlyphs;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> StructureDetailBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailTitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailClassificationTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailSpecificationTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailDescriptionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> DetailAvailabilityBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRResourceGlyphWidget> DetailFamilyGlyph;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> DetailRecommendationRow;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> DetailRecommendationBadge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailRecommendationTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> DetailFlowPanel;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> DetailFlowInputBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> DetailFlowProcessBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> DetailFlowOutputBadge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailFlowEffectTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> DetailPlacementMetricRow;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> DetailPlacementTargetBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> DetailPlacementFootprintBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> DetailPlacementCapacityBadge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailPlacementStatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailPlacementTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> StructureTabBarBorder;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> StructureTabIndicatorRowBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> StructureTabIndicatorSizeBoxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> StructureTabIndicatorImages;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StructurePageTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultStructureTabIndicatorTexture;

	int32 StructureTabIndicatorFirstPageIndex = 0;

	UFUNCTION(BlueprintImplementableEvent, Category = "StarRovers|Assembly")
	void OnSelectedStructureChanged(FName NewSelectedStructureId, bool bNewHasSelectedStructureId);

private:
	void ApplySharedUITheme();
	void BuildStructureSelectionWidgetTree();
	void RebuildBuildOptions();
	void RebuildCategorizedBuildOptions();
	void RefreshSelectedStructureText();
	void SyncCategoryBarLayout();
	void EnsureDefaultCategoryIconTextures();
	void ApplyCategoryButtonIconBrushes();
	void RefreshCategoryButtonStyles();
	void RefreshCategoryButtonLabels();
	void SyncFacilityButtonBarLayout();
	void SyncDetailPanelLayout();
	void RefreshFacilityButtonBarVisibility();
	void RefreshFacilityButtonLabels();
	void RefreshFacilityButtonStyles();
	void RefreshBuildRecommendation(bool bRefreshWorldState);
	void RefreshBuildOptionDetail();
	void RefreshPointerHoverState();
	void SyncStructureTabBarLayout();
	void RebuildStructureTabIndicators();
	void EnsureDefaultStructureTabIndicatorTexture();
	void RefreshStructureTabIndicatorBrushes();
	void RefreshStructureTabIndicatorStyles();
	void RebuildBuildOptionIndex();
	void SetStructureSelectionTabIndex(int32 NewTabIndex);
	void SelectStructureCategory(int32 CategoryIndex);
	void SelectFacilityButton(int32 FacilityButtonIndex);
	void RevealBuildOptionInDock(FName StructureId);
	bool SelectBuildOptionIfAvailable(FName StructureId);
	void SetHoveredStructureCategory(int32 CategoryIndex);
	void ClearHoveredStructureCategory();
	void SetHoveredFacilityButton(int32 FacilityButtonIndex);
	void ClearHoveredFacilityButton();
	const TArray<FName>* GetSelectedFacilityBuildOptionIds() const;
	int32 GetSelectedFacilityPageCount() const;
	FName GetFacilityButtonStructureId(int32 FacilityButtonIndex) const;
	bool IsStructureCategoryAvailable(int32 CategoryIndex) const;
	bool IsBuildOptionSelectable(FName StructureId) const;
	int32 FindCategoryButtonIndexAtScreenPosition(const FVector2D& ScreenPosition) const;
	int32 FindFacilityButtonIndexAtScreenPosition(const FVector2D& ScreenPosition) const;
	const FSRStructureBuildOption* FindBuildOption(FName StructureId) const;
	const FSRStructureBuildOption* GetDetailBuildOption() const;
	bool IsScreenPositionOverStructureSelectionPanel(const FVector2D& ScreenPosition) const;

	UFUNCTION()
	void HandleCategory1Clicked();

	UFUNCTION()
	void HandleCategory1Hovered();

	UFUNCTION()
	void HandleCategory2Clicked();

	UFUNCTION()
	void HandleCategory2Hovered();

	UFUNCTION()
	void HandleCategory3Clicked();

	UFUNCTION()
	void HandleCategory3Hovered();

	UFUNCTION()
	void HandleCategory4Clicked();

	UFUNCTION()
	void HandleCategory4Hovered();

	UFUNCTION()
	void HandleCategory5Clicked();

	UFUNCTION()
	void HandleCategory5Hovered();

	UFUNCTION()
	void HandleCategory6Clicked();

	UFUNCTION()
	void HandleCategory6Hovered();

	UFUNCTION()
	void HandleCategory7Clicked();

	UFUNCTION()
	void HandleCategory7Hovered();

	UFUNCTION()
	void HandleCategoryUnhovered();

	UFUNCTION()
	void HandleFacilityButton1Clicked();

	UFUNCTION()
	void HandleFacilityButton1Hovered();

	UFUNCTION()
	void HandleFacilityButton2Clicked();

	UFUNCTION()
	void HandleFacilityButton2Hovered();

	UFUNCTION()
	void HandleFacilityButton3Clicked();

	UFUNCTION()
	void HandleFacilityButton3Hovered();

	UFUNCTION()
	void HandleFacilityButton4Clicked();

	UFUNCTION()
	void HandleFacilityButton4Hovered();

	UFUNCTION()
	void HandleFacilityButton5Clicked();

	UFUNCTION()
	void HandleFacilityButton5Hovered();

	UFUNCTION()
	void HandleFacilityButtonUnhovered();

	FSRStarRoversStructureBuildOptionSelectedSignature BuildOptionSelectedEvent;

	TMap<FName, int32> BuildOptionIndexByStructureId;
	TArray<FName> VisibleBuildOptionIds;
	float BuildRecommendationRefreshAccumulator = 0.0f;
};
