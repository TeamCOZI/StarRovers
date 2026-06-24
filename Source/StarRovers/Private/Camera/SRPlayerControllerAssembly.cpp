#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Structure/SRStructureDataAsset.h"
bool ASRPlayerController::IsAssemblyModeActive() const
{
	return AssemblyComponent && AssemblyComponent->IsAssemblyModeActive();
}

void ASRPlayerController::SetAssemblyModeActive(bool bNewAssemblyModeActive)
{
	if (AssemblyComponent)
	{
		AssemblyComponent->SetAssemblyModeActive(bNewAssemblyModeActive);
	}
	RefreshFocusInfoWidget();
	RefreshStructureSelectionWidget();
}

void ASRPlayerController::ToggleAssemblyMode()
{
	SetAssemblyModeActive(!IsAssemblyModeActive());
}

bool ASRPlayerController::HasSelectedStructureBuildId() const
{
	return bHasSelectedStructureBuildId;
}

FName ASRPlayerController::GetSelectedStructureBuildId() const
{
	return SelectedStructureBuildId;
}

USRStructureDataAsset* ASRPlayerController::GetSelectedStructureDataAsset() const
{
	return SelectedStructureDataAsset;
}

bool ASRPlayerController::ShouldHandleAssemblyPlacementDrag() const
{
	return AssemblyComponent && AssemblyComponent->ShouldHandleStructurePlacementDrag();
}

bool ASRPlayerController::ShouldBlockAssemblyCameraDrag() const
{
	return IsAssemblyModeActive() && IsValid(SelectedStructureDataAsset);
}

bool ASRPlayerController::BeginAssemblyPlacementDrag()
{
	AActor* AssemblySelectedActor = nullptr;
	if (!AssemblyComponent || !AssemblyComponent->BeginStructurePlacementDrag(AssemblySelectedActor))
	{
		return false;
	}

	UpdateSelection(AssemblySelectedActor);
	return true;
}

bool ASRPlayerController::ContinueAssemblyPlacementDrag()
{
	AActor* AssemblySelectedActor = nullptr;
	if (!AssemblyComponent || !AssemblyComponent->ContinueStructurePlacementDrag(AssemblySelectedActor))
	{
		return false;
	}

	if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
	{
		UpdateSelection(AssemblySelectedActor);
	}
	return true;
}

void ASRPlayerController::EndAssemblyPlacementDrag()
{
	if (AssemblyComponent)
	{
		AssemblyComponent->EndStructurePlacementDrag(true);
	}
}

bool ASRPlayerController::RotateStructurePlacement(int32 StepDelta)
{
	return AssemblyComponent && AssemblyComponent->RotateStructurePlacement(StepDelta);
}

void ASRPlayerController::HandleStructureBuildOptionSelected(FName StructureId, USRStructureDataAsset* StructureDataAsset)
{
	if (StructureId.IsNone() || !IsValid(StructureDataAsset))
	{
		if (!StructureId.IsNone())
		{
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController received structure build option '%s' without a valid StructureDataAsset."), *StructureId.ToString());
		}
		SelectedStructureBuildId = NAME_None;
		bHasSelectedStructureBuildId = false;
		SelectedStructureDataAsset = nullptr;
		return;
	}

	SelectedStructureBuildId = StructureId;
	bHasSelectedStructureBuildId = true;
	SelectedStructureDataAsset = StructureDataAsset;
}
