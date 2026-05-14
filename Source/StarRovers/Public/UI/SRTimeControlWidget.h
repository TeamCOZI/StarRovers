#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SRTimeControlWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class SWidget;

UCLASS(Blueprintable)
class STARROVERS_API USRTimeControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TimeControlBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeControlTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PauseButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TwoXButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FourXButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EightXButton;

	UFUNCTION(BlueprintImplementableEvent, Category = "StarRovers|Simulation")
	void OnTimeControlChanged(bool bIsPaused, float CurrentTimeScale);

private:
	void BuildTimeControlWidgetTree();
	void BindTimeControlButtonHandlers();
	void RefreshTimeControlState();
	class USRTimeControlSubsystem* GetTimeControlSubsystem() const;
	void UpdateButtonStyle(UButton* Button, bool bIsActive) const;

	UFUNCTION()
	void HandlePauseClicked();

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleTwoXClicked();

	UFUNCTION()
	void HandleFourXClicked();

	UFUNCTION()
	void HandleEightXClicked();
};
