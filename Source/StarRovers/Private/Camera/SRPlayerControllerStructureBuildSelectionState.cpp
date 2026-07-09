#include "SRPlayerControllerStructureBuildSelectionState.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRStructureSelectionWidget.h"

void FSRPlayerControllerStructureBuildSelectionState::ResetSelection(
	FName& SelectedStructureBuildId,
	bool& bHasSelectedStructureBuildId,
	TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset,
	AActor* SelectedActor)
{
	ResetSelectionFields(SelectedStructureBuildId, bHasSelectedStructureBuildId, SelectedStructureDataAsset);
	SetHoveredInteractionGridPatchVisible(SelectedActor, false);
}

void FSRPlayerControllerStructureBuildSelectionState::ApplySelection(
	FName StructureId,
	USRStructureDataAsset* StructureDataAsset,
	FName& SelectedStructureBuildId,
	bool& bHasSelectedStructureBuildId,
	TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset,
	AActor* SelectedActor)
{
	SelectedStructureBuildId = StructureId;
	bHasSelectedStructureBuildId = true;
	SelectedStructureDataAsset = StructureDataAsset;
	SetHoveredInteractionGridPatchVisible(SelectedActor, true);
}

bool FSRPlayerControllerStructureBuildSelectionState::ClearSelection(
	FName& SelectedStructureBuildId,
	bool& bHasSelectedStructureBuildId,
	TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset,
	AActor* SelectedActor,
	USRStructureSelectionWidget* StructureSelectionWidget,
	USRAssemblyComponent* AssemblyComponent)
{
	if (!bHasSelectedStructureBuildId && !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	if (StructureSelectionWidget)
	{
		StructureSelectionWidget->ClearSelectedStructureId();
	}

	ResetSelectionFields(SelectedStructureBuildId, bHasSelectedStructureBuildId, SelectedStructureDataAsset);

	if (AssemblyComponent)
	{
		AssemblyComponent->CancelSelectedStructurePlacement();
	}

	SetHoveredInteractionGridPatchVisible(SelectedActor, false);
	return true;
}

void FSRPlayerControllerStructureBuildSelectionState::SyncSelectionFromWidget(
	FName SelectedStructureBuildId,
	bool bHasSelectedStructureBuildId,
	TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset,
	AActor* SelectedActor,
	USRStructureSelectionWidget* StructureSelectionWidget)
{
	if (!bHasSelectedStructureBuildId || !StructureSelectionWidget)
	{
		return;
	}

	StructureSelectionWidget->SetSelectedStructureId(SelectedStructureBuildId);
	SelectedStructureDataAsset = StructureSelectionWidget->GetSelectedStructureDataAsset();
	SetHoveredInteractionGridPatchVisible(SelectedActor, IsValid(SelectedStructureDataAsset));
}

void FSRPlayerControllerStructureBuildSelectionState::ResetSelectionFields(
	FName& SelectedStructureBuildId,
	bool& bHasSelectedStructureBuildId,
	TObjectPtr<USRStructureDataAsset>& SelectedStructureDataAsset)
{
	SelectedStructureBuildId = NAME_None;
	bHasSelectedStructureBuildId = false;
	SelectedStructureDataAsset = nullptr;
}

void FSRPlayerControllerStructureBuildSelectionState::SetHoveredInteractionGridPatchVisible(AActor* SelectedActor, bool bVisible)
{
	if (USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
	{
		SurfaceGrid->SetHoveredInteractionGridPatchVisible(bVisible);
	}
}
