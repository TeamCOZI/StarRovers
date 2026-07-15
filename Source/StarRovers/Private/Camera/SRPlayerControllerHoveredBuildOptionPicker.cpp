#include "SRPlayerControllerHoveredBuildOptionPicker.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRStructureSelectionWidget.h"

bool FSRPlayerControllerHoveredBuildOptionPicker::TryPickBuildOptionFromFocusedActor(
	AActor* FocusedActor,
	const TArray<USRStructureDataAsset*>& AvailableStructureDataAssets,
	const USRAugmentSubsystem* AugmentSubsystem,
	USRStructureSelectionWidget* StructureSelectionWidget,
	FName& OutStructureId,
	USRStructureDataAsset*& OutStructureDataAsset)
{
	OutStructureId = NAME_None;
	OutStructureDataAsset = nullptr;

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
				PickedStructureDataAsset = ResolveSelectableStructureDataAsset(
					PlacedStructure.StructureDataAsset.Get(),
					AvailableStructureDataAssets,
					AugmentSubsystem);
			}
		}
	}

	if (!PickedStructureDataAsset)
	{
		if (const USRConveyorNetworkComponent* ConveyorNetwork = SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>())
		{
			TSet<FSRPlanetSurfaceGridCellId> HoveredCellIds;
			HoveredCellIds.Add(HoveredCellInfo.CellId);

			TArray<FSRConveyorBeltPath> HoveredConveyorBeltPaths;
			if (ConveyorNetwork->GetConveyorBeltPathsInCells(HoveredCellIds, HoveredConveyorBeltPaths))
			{
				for (const FSRConveyorBeltPath& BeltPath : HoveredConveyorBeltPaths)
				{
					PickedStructureDataAsset = ResolveSelectableStructureDataAsset(
						BeltPath.StructureDataAsset.Get(),
						AvailableStructureDataAssets,
						AugmentSubsystem);
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

	OutStructureId = PickedStructureData.StructureId;
	OutStructureDataAsset = PickedStructureDataAsset;
	return true;
}

USRStructureDataAsset* FSRPlayerControllerHoveredBuildOptionPicker::ResolveSelectableStructureDataAsset(
	USRStructureDataAsset* CandidateStructureDataAsset,
	const TArray<USRStructureDataAsset*>& AvailableStructureDataAssets,
	const USRAugmentSubsystem* AugmentSubsystem)
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

	if (AugmentSubsystem && !AugmentSubsystem->IsStructureUnlocked(CandidateStructureDataAsset))
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
			&& !AvailableStructureData.bIsResourceDeposit
			&& (!AugmentSubsystem || AugmentSubsystem->IsStructureUnlocked(AvailableStructureDataAsset)))
		{
			return AvailableStructureDataAsset;
		}
	}

	return nullptr;
}
