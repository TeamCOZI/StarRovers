#pragma once

#include "CoreMinimal.h"

class AActor;
class USRAssemblyComponent;
class USRStructureDataAsset;
class USRStructureSelectionWidget;

class FSRPlayerControllerStructureBuildSelectionState
{
public:
	static void ResetSelection(
		FName& SelectedStructureBuildId,
		bool& bHasSelectedStructureBuildId,
		TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset,
		AActor* SelectedActor);

	static void ApplySelection(
		FName StructureId,
		USRStructureDataAsset* StructureDataAsset,
		FName& SelectedStructureBuildId,
		bool& bHasSelectedStructureBuildId,
		TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset,
		AActor* SelectedActor);

	static bool ClearSelection(
		FName& SelectedStructureBuildId,
		bool& bHasSelectedStructureBuildId,
		TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset,
		AActor* SelectedActor,
		USRStructureSelectionWidget* StructureSelectionWidget,
		USRAssemblyComponent* AssemblyComponent);

	static void SyncSelectionFromWidget(
		FName SelectedStructureBuildId,
		bool bHasSelectedStructureBuildId,
		TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset,
		AActor* SelectedActor,
		USRStructureSelectionWidget* StructureSelectionWidget);

private:
	static void ResetSelectionFields(
		FName& SelectedStructureBuildId,
		bool& bHasSelectedStructureBuildId,
		TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset);

	static void SetHoveredInteractionGridPatchVisible(AActor* SelectedActor, bool bVisible);
};
