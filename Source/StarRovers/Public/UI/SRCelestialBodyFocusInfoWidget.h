#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/SRCelestialBodyFocusInfo.h"
#include "SRCelestialBodyFocusInfoWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UWidget;
class SWidget;

DECLARE_MULTICAST_DELEGATE(FSRStarRoversAssemblyModeRequestedSignature);

UCLASS(Blueprintable)
class STARROVERS_API USRCelestialBodyFocusInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Focus")
	void SetFocusInfo(const FSRCelestialBodyFocusInfo& NewFocusInfo);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Focus")
	void ClearFocusInfo();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Focus")
	bool HasFocusInfo() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Focus")
	FSRCelestialBodyFocusInfo GetFocusInfo() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetAssemblyModeActive(bool bNewAssemblyModeActive);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
	bool IsAssemblyModeActive() const;

	FSRStarRoversAssemblyModeRequestedSignature& OnAssemblyModeRequested();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "FocusInfo"))
	FSRCelestialBodyFocusInfo FocusInfo;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasFocusInfo"))
	bool bHasFocusInfo = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "bAssemblyModeActive"))
	bool bAssemblyModeActive = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "StarRovers|Focus")
	void OnFocusInfoChanged(const FSRCelestialBodyFocusInfo& NewFocusInfo);

	UFUNCTION(BlueprintImplementableEvent, Category = "StarRovers|Focus")
	void OnFocusCleared();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> FocusInfoBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DisplayNameTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AssemblyModeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AssemblyModeButtonTextBlock;

private:
	UFUNCTION()
	void HandleAssemblyModeButtonClicked();

	void BuildFocusInfoWidgetTree();
	void EnsureAssemblyModeButton(UWidget* AssemblyModeButtonParent);
	void BindAssemblyModeButtonHandler();
	void RefreshFocusInfoText();
	void RefreshAssemblyModeButton();

	FSRStarRoversAssemblyModeRequestedSignature AssemblyModeRequestedEvent;
};
