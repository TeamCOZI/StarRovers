#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/SRPlayerGuidancePresentation.h"
#include "UI/SRUILayoutPolicy.h"
#include "SRPlayerGuidanceWidget.generated.h"

class SWidget;
class UButton;
class UScaleBox;
class USizeBox;
class UTextBlock;
class USRResourceGlyphWidget;
class USRStatusBadgeWidget;
class USRThemedCardWidget;

/** Top-center, non-blocking banner for one current problem or next action. */
UCLASS(Blueprintable)
class STARROVERS_API USRPlayerGuidanceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetGuidanceMessage(const FSRPlayerGuidanceMessage& NewMessage);
	void ClearGuidanceMessage();
	void PushTransientNotification(
		FName MessageId,
		FText Title,
		FText Detail,
		FText Action,
		ESRUIVisualState VisualState = ESRUIVisualState::Positive,
		float DurationSeconds = 5.0f);

	void SetAutomaticContextEvaluationEnabled(bool bEnabled);
	FSRPlayerGuidanceMessage GetDisplayedGuidanceMessage() const;
	FSRPlayerGuidanceSnapshot BuildCurrentSnapshot() const;
	void EvaluateCurrentContext();
	bool ExecuteDisplayedAction();
	bool IsPointerOverGuidanceUI() const;
	FSRUITopCenterLaneLayout GetResolvedCommandLaneLayout() const;
	bool IsCompactCommandLane() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Guidance", meta = (ClampMin = "0.05"))
	float ContextRefreshIntervalSeconds = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Guidance", meta = (ClampMin = "320.0"))
	float BannerWidth = 820.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Guidance", meta = (ClampMin = "72.0"))
	float BannerHeight = 148.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Guidance", meta = (ClampMin = "320.0"))
	float CompactDetailVisibilityWidth = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Guidance")
	bool bAutomaticContextEvaluation = true;

private:
	void BuildGuidanceWidgetTree();
	void CacheGuidanceWidgetTree();
	void RefreshPresentation();
	void RefreshResponsiveLayout(const FVector2D& ViewportSize, bool bForceRefresh = false);
	FSRPlayerGuidanceMessage ResolveMessageToDisplay() const;
	bool ExecuteBuildAction(
		ESRStructureBuildRole Role,
		ESRResourceFamily PreferredFamily,
		const FSRFirstFuelMilestoneSnapshot& MilestoneSnapshot);
	bool ExecuteFacilityFocusAction(const FSRFirstFuelMilestoneSnapshot& MilestoneSnapshot);

	UFUNCTION()
	void HandleActionButtonClicked();

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> BannerScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BannerDesignSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<USRThemedCardWidget> BannerCard;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> CategoryBadge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRResourceGlyphWidget> ResourceGlyphWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ActionButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionButtonTextBlock;

	FSRPlayerGuidanceMessage ContextMessage;
	FSRPlayerGuidanceMessage TransientMessage;
	FSRPlayerGuidanceMessage DisplayedMessage;
	double TransientExpirySeconds = 0.0;
	float ContextRefreshAccumulator = 0.0f;
	FVector2D LastResponsiveViewportSize = FVector2D::ZeroVector;
	FSRUITopCenterLaneLayout ResolvedCommandLaneLayout;
};
