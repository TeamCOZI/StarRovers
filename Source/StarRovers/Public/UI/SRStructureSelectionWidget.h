#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SRStructureSelectionWidget.generated.h"

class SWidget;
class UBorder;
class UButton;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class USRStructureSelectionWidget;
class USRStructureDataAsset;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "bEnabled"))
	bool bEnabled = true;
};

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

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetBuildOptions(const TArray<FSRStructureBuildOption>& NewBuildOptions);

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

	void DispatchBuildOptionSelected(FName StructureId);
	FSRStarRoversStructureBuildOptionSelectedSignature& OnBuildOptionSelected();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "TitleText"))
	FText TitleText = NSLOCTEXT("StarRoversStructureSelection", "TitleText", "Structures");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "PanelColor"))
	FLinearColor PanelColor = FLinearColor(0.015f, 0.025f, 0.04f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "ButtonColor"))
	FLinearColor ButtonColor = FLinearColor(0.08f, 0.11f, 0.15f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "SelectedButtonColor"))
	FLinearColor SelectedButtonColor = FLinearColor(0.18f, 0.36f, 0.42f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "DisabledButtonColor"))
	FLinearColor DisabledButtonColor = FLinearColor(0.06f, 0.07f, 0.08f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "BuildOptions"))
	TArray<FSRStructureBuildOption> BuildOptions;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "SelectedStructureId"))
	FName SelectedStructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "bHasSelectedStructureId"))
	bool bHasSelectedStructureId = false;

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

	UFUNCTION(BlueprintImplementableEvent, Category = "StarRovers|Assembly")
	void OnSelectedStructureChanged(FName NewSelectedStructureId, bool bNewHasSelectedStructureId);

private:
	void BuildStructureSelectionWidgetTree();
	void RebuildBuildOptions();
	void RefreshSelectedStructureText();
	const FSRStructureBuildOption* FindBuildOption(FName StructureId) const;

	FSRStarRoversStructureBuildOptionSelectedSignature BuildOptionSelectedEvent;
};
