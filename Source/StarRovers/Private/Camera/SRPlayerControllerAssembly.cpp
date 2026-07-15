#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "SRPlayerControllerHoveredBuildOptionPicker.h"
#include "SRPlayerControllerStructureBuildSelectionState.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRStructureSelectionWidget.h"
#include "Utility/SRLog.h"

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

void ASRPlayerController::UpdateAssemblyModeFromFocusedActorScreenSize()
{
	const bool bShouldActivateAssemblyMode = ShouldActivateAssemblyModeForFocusedActorScreenSize();
	if (IsAssemblyModeActive() != bShouldActivateAssemblyMode)
	{
		SetAssemblyModeActive(bShouldActivateAssemblyMode);
	}
}

bool ASRPlayerController::ShouldActivateAssemblyModeForFocusedActorScreenSize() const
{
	const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (!CameraPawn || !PlayerCameraManager)
	{
		return false;
	}

	const AActor* FocusedActor = CameraPawn->GetFocusedActor();
	if (!IsValid(FocusedActor) || !IsValid(USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor)))
	{
		return false;
	}

	const float SafeThreshold = FMath::Max(0.0f, AssemblyModeScreenSizeThreshold);
	const float FocusedActorScreenScale = USRCelestialBodyRuntimeLibrary::GetScreenScale(
		FocusedActor,
		PlayerCameraManager->GetCameraLocation(),
		PlayerCameraManager->GetCameraRotation().Vector(),
		PlayerCameraManager->GetFOVAngle());
	return FocusedActorScreenScale >= SafeThreshold;
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

bool ASRPlayerController::ShouldHandleAssemblyAreaSelectionDrag() const
{
	return AssemblyComponent && AssemblyComponent->ShouldHandleAreaSelectionDrag();
}

bool ASRPlayerController::ShouldHandleAssemblyAreaDeletionDrag() const
{
	return AssemblyComponent && AssemblyComponent->ShouldHandleAreaDeletionDrag();
}

bool ASRPlayerController::ShouldBlockAssemblyCameraDrag() const
{
	return IsAssemblyModeActive() && (IsValid(SelectedStructureDataAsset) || ShouldHandleAssemblyAreaSelectionDrag());
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

bool ASRPlayerController::BeginAssemblyAreaSelectionDrag()
{
	AActor* AssemblySelectedActor = nullptr;
	if (!AssemblyComponent || !AssemblyComponent->BeginAreaSelectionDrag(AssemblySelectedActor))
	{
		return false;
	}

	if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
	{
		UpdateSelection(AssemblySelectedActor);
	}
	return true;
}

bool ASRPlayerController::ContinueAssemblyAreaSelectionDrag()
{
	AActor* AssemblySelectedActor = nullptr;
	if (!AssemblyComponent || !AssemblyComponent->ContinueAreaSelectionDrag(AssemblySelectedActor))
	{
		return false;
	}

	if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
	{
		UpdateSelection(AssemblySelectedActor);
	}
	return true;
}

void ASRPlayerController::EndAssemblyAreaSelectionDrag()
{
	if (AssemblyComponent)
	{
		AssemblyComponent->EndAreaSelectionDrag();
	}
}

bool ASRPlayerController::BeginAssemblyAreaDeletionDrag()
{
	AActor* AssemblySelectedActor = nullptr;
	if (!AssemblyComponent || !AssemblyComponent->BeginAreaDeletionDrag(AssemblySelectedActor))
	{
		return false;
	}

	if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
	{
		UpdateSelection(AssemblySelectedActor);
	}
	return true;
}

bool ASRPlayerController::ContinueAssemblyAreaDeletionDrag()
{
	if (RuntimeState.bAssemblyAreaDeletionDragHoldActive
		&& AssemblyComponent
		&& !AssemblyComponent->IsAreaSelectionDragActive()
		&& !AssemblyComponent->IsAreaDeletionDragActive())
	{
		return BeginAssemblyAreaDeletionDrag();
	}

	AActor* AssemblySelectedActor = nullptr;
	if (!AssemblyComponent || !AssemblyComponent->ContinueAreaDeletionDrag(AssemblySelectedActor))
	{
		return false;
	}

	if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
	{
		UpdateSelection(AssemblySelectedActor);
	}
	return true;
}

void ASRPlayerController::EndAssemblyAreaDeletionDrag()
{
	if (AssemblyComponent)
	{
		AssemblyComponent->EndAreaDeletionDrag();
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
			SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController received structure build option '%s' without a valid StructureDataAsset."), *StructureId.ToString());
		}
		FSRPlayerControllerStructureBuildSelectionState::ResetSelection(
			SelectedStructureBuildId,
			bHasSelectedStructureBuildId,
			SelectedStructureDataAsset,
			SelectedActor);
		return;
	}

	if (const USRAugmentSubsystem* AugmentSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRAugmentSubsystem>() : nullptr)
	{
		if (!AugmentSubsystem->IsStructureUnlocked(StructureDataAsset))
		{
			FSRPlayerControllerStructureBuildSelectionState::ResetSelection(
				SelectedStructureBuildId,
				bHasSelectedStructureBuildId,
				SelectedStructureDataAsset,
				SelectedActor);
			return;
		}
	}

	FSRPlayerControllerStructureBuildSelectionState::ApplySelection(
		StructureId,
		StructureDataAsset,
		SelectedStructureBuildId,
		bHasSelectedStructureBuildId,
		SelectedStructureDataAsset,
		SelectedActor);
}

bool ASRPlayerController::ClearSelectedStructureBuildOption()
{
	return FSRPlayerControllerStructureBuildSelectionState::ClearSelection(
		SelectedStructureBuildId,
		bHasSelectedStructureBuildId,
		SelectedStructureDataAsset,
		SelectedActor,
		StructureSelectionWidget,
		AssemblyComponent);
}

bool ASRPlayerController::TrySelectBuildOptionFromHoveredCell()
{
	const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (!CameraPawn)
	{
		return false;
	}

	FName PickedStructureId = NAME_None;
	USRStructureDataAsset* PickedStructureDataAsset = nullptr;
	const USRAugmentSubsystem* AugmentSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRAugmentSubsystem>() : nullptr;
	TArray<USRStructureDataAsset*> StructureDataAssets;
	GetAvailableStructureDataAssets(StructureDataAssets);
	if (!FSRPlayerControllerHoveredBuildOptionPicker::TryPickBuildOptionFromFocusedActor(
		CameraPawn->GetFocusedActor(),
		StructureDataAssets,
		AugmentSubsystem,
		StructureSelectionWidget,
		PickedStructureId,
		PickedStructureDataAsset))
	{
		return false;
	}

	HandleStructureBuildOptionSelected(PickedStructureId, PickedStructureDataAsset);
	return IsValid(SelectedStructureDataAsset);
}
