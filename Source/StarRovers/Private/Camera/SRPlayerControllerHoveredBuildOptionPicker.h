#pragma once

#include "CoreMinimal.h"

class AActor;
class USRAugmentSubsystem;
class USRStructureDataAsset;
class USRStructureSelectionWidget;

class FSRPlayerControllerHoveredBuildOptionPicker
{
public:
	static bool TryPickBuildOptionFromFocusedActor(
		AActor* FocusedActor,
		const TArray<TObjectPtr<USRStructureDataAsset>>& AvailableStructureDataAssets,
		const USRAugmentSubsystem* AugmentSubsystem,
		USRStructureSelectionWidget* StructureSelectionWidget,
		FName& OutStructureId,
		USRStructureDataAsset*& OutStructureDataAsset);

private:
	static USRStructureDataAsset* ResolveSelectableStructureDataAsset(
		USRStructureDataAsset* CandidateStructureDataAsset,
		const TArray<TObjectPtr<USRStructureDataAsset>>& AvailableStructureDataAssets,
		const USRAugmentSubsystem* AugmentSubsystem);
};
