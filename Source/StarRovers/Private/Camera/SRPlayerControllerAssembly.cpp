#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRStructureSelectionWidget.h"

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
	if (bAssemblyAreaDeletionDragHoldActive
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
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController received structure build option '%s' without a valid StructureDataAsset."), *StructureId.ToString());
		}
		SelectedStructureBuildId = NAME_None;
		bHasSelectedStructureBuildId = false;
		SelectedStructureDataAsset = nullptr;
		if (USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
		{
			SurfaceGrid->SetHoveredInteractionGridPatchVisible(false);
		}
		return;
	}

	SelectedStructureBuildId = StructureId;
	bHasSelectedStructureBuildId = true;
	SelectedStructureDataAsset = StructureDataAsset;
	if (USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
	{
		SurfaceGrid->SetHoveredInteractionGridPatchVisible(true);
	}
}

bool ASRPlayerController::ClearSelectedStructureBuildOption()
{
	if (!bHasSelectedStructureBuildId && !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	if (StructureSelectionWidget)
	{
		StructureSelectionWidget->ClearSelectedStructureId();
	}

	SelectedStructureBuildId = NAME_None;
	bHasSelectedStructureBuildId = false;
	SelectedStructureDataAsset = nullptr;

	if (AssemblyComponent)
	{
		AssemblyComponent->CancelSelectedStructurePlacement();
	}

	if (USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
	{
		SurfaceGrid->SetHoveredInteractionGridPatchVisible(false);
	}

	return true;
}

bool ASRPlayerController::TrySelectBuildOptionFromHoveredCell()
{
	const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (!CameraPawn)
	{
		return false;
	}

	AActor* FocusedActor = CameraPawn->GetFocusedActor();
	USRPlanetSurfaceGrid* SurfaceGrid = IsValid(FocusedActor)
		? USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor)
		: nullptr;
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
	if (!SurfaceGrid->GetHoveredCellInfo(HoveredCellInfo))
	{
		return false;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	if (!IsValid(SurfaceOwner))
	{
		return false;
	}

	USRStructureDataAsset* PickedStructureDataAsset = nullptr;
	if (HoveredCellInfo.bOccupied && !HoveredCellInfo.OccupantId.IsNone())
	{
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			FSRPlacedStructureInstance PlacedStructure;
			if (StructureInstanceManager->GetPlacedStructure(HoveredCellInfo.OccupantId, PlacedStructure))
			{
				PickedStructureDataAsset = ResolveSelectableStructureDataAsset(PlacedStructure.StructureDataAsset.Get());
			}
		}
	}

	if (!PickedStructureDataAsset)
	{
		if (const USRConveyorNetworkComponent* ConveyorNetwork = SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>())
		{
			TSet<FSRPlanetSurfaceGridCellId> HoveredCellIds;
			HoveredCellIds.Add(HoveredCellInfo.CellId);

			TArray<FSRConveyorVisualPath> HoveredConveyorVisualPaths;
			if (ConveyorNetwork->GetConveyorVisualPathsInCells(HoveredCellIds, HoveredConveyorVisualPaths))
			{
				for (const FSRConveyorVisualPath& VisualPath : HoveredConveyorVisualPaths)
				{
					PickedStructureDataAsset = ResolveSelectableStructureDataAsset(VisualPath.StructureDataAsset.Get());
					if (PickedStructureDataAsset)
					{
						break;
					}
				}
			}
		}
	}

	if (!PickedStructureDataAsset)
	{
		return false;
	}

	const FSRStructureData PickedStructureData = PickedStructureDataAsset->BuildData();
	if (PickedStructureData.StructureId.IsNone())
	{
		return false;
	}

	if (StructureSelectionWidget)
	{
		StructureSelectionWidget->SetSelectedStructureId(PickedStructureData.StructureId);
		if (!StructureSelectionWidget->HasSelectedStructureId()
			|| StructureSelectionWidget->GetSelectedStructureId() != PickedStructureData.StructureId)
		{
			return false;
		}

		if (USRStructureDataAsset* WidgetStructureDataAsset = StructureSelectionWidget->GetSelectedStructureDataAsset())
		{
			PickedStructureDataAsset = WidgetStructureDataAsset;
		}
	}

	HandleStructureBuildOptionSelected(PickedStructureData.StructureId, PickedStructureDataAsset);
	return IsValid(SelectedStructureDataAsset);
}

USRStructureDataAsset* ASRPlayerController::ResolveSelectableStructureDataAsset(USRStructureDataAsset* CandidateStructureDataAsset) const
{
	if (!IsValid(CandidateStructureDataAsset))
	{
		return nullptr;
	}

	const FSRStructureData CandidateStructureData = CandidateStructureDataAsset->BuildData();
	if (CandidateStructureData.StructureId.IsNone()
		|| !CandidateStructureData.bAvailableForConstruction
		|| CandidateStructureData.bIsResourceDeposit)
	{
		return nullptr;
	}

	for (USRStructureDataAsset* AvailableStructureDataAsset : AvailableStructureDataAssets)
	{
		if (!IsValid(AvailableStructureDataAsset))
		{
			continue;
		}

		const FSRStructureData AvailableStructureData = AvailableStructureDataAsset->BuildData();
		if (AvailableStructureData.StructureId == CandidateStructureData.StructureId
			&& AvailableStructureData.BuildKind == CandidateStructureData.BuildKind
			&& AvailableStructureData.bAvailableForConstruction
			&& !AvailableStructureData.bIsResourceDeposit)
		{
			return AvailableStructureDataAsset;
		}
	}

	return nullptr;
}
