#include "Assembly/SRAssemblyPreviewState.h"

#include "Assembly/SRAssemblyPreviewMaterial.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructure.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructureSurfacePortConnection.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool GetGhostPortNeighborCellId(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRStructurePortDirection Direction,
		FSRPlanetSurfaceGridCellId& OutNeighborCellId)
	{
		return StarRovers::Structure::SurfacePorts::TryGetPortConnectionCellId(
			SurfaceGrid,
			CellId,
			Direction,
			OutNeighborCellId);
	}

	bool ResolveGhostPortFootprintCellId(
		const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
		int32 FootprintCellsX,
		int32 FootprintCellsY,
		const FSRStructurePortSpec& PortSpec,
		FSRPlanetSurfaceGridCellId& OutFootprintCellId)
	{
		OutFootprintCellId = FSRPlanetSurfaceGridCellId();

		const int32 SafeFootprintCellsX = FMath::Max(1, FootprintCellsX);
		const int32 SafeFootprintCellsY = FMath::Max(1, FootprintCellsY);
		if (PortSpec.CellOffsetX < 0
			|| PortSpec.CellOffsetY < 0
			|| PortSpec.CellOffsetX >= SafeFootprintCellsX
			|| PortSpec.CellOffsetY >= SafeFootprintCellsY)
		{
			return false;
		}

		const int32 FootprintIndex = PortSpec.CellOffsetY * SafeFootprintCellsX + PortSpec.CellOffsetX;
		if (!FootprintCellIds.IsValidIndex(FootprintIndex))
		{
			return false;
		}

		OutFootprintCellId = FootprintCellIds[FootprintIndex];
		return true;
	}

	void AppendGhostPortPreviewCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
		int32 FootprintCellsX,
		int32 FootprintCellsY,
		const FSRStructurePortSpec& PortSpec,
		TArray<FSRPlanetSurfaceGridCellId>& OutConnectionCellIds)
	{
		FSRPlanetSurfaceGridCellId PortFootprintCellId;
		if (!ResolveGhostPortFootprintCellId(FootprintCellIds, FootprintCellsX, FootprintCellsY, PortSpec, PortFootprintCellId))
		{
			return;
		}

		FSRPlanetSurfaceGridCellId ConnectionCellId;
		if (!GetGhostPortNeighborCellId(SurfaceGrid, PortFootprintCellId, PortSpec.Direction, ConnectionCellId)
			|| FootprintCellIds.Contains(ConnectionCellId))
		{
			return;
		}

		OutConnectionCellIds.AddUnique(ConnectionCellId);
	}
}

void FSRAssemblyStructurePreviewState::ClearGhostPortPreview()
{
	if (bHasStructureGhostPortPreview && IsValid(StructureGhostPortPreviewSurfaceGrid))
	{
		StructureGhostPortPreviewSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	StructureGhostPortPreviewSurfaceGrid = nullptr;
	bHasStructureGhostPortPreview = false;
}

void FSRAssemblyStructurePreviewState::UpdateGhostPortPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRStructureData& StructureData,
	const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
	int32 PlacementRotationSteps)
{
	if (!IsValid(SurfaceGrid)
		|| StructureData.BuildKind != ESRStructureBuildKind::Structure
		|| FootprintCellIds.IsEmpty())
	{
		ClearGhostPortPreview();
		return;
	}

	TArray<FSRPlanetSurfaceGridCellId> InputConnectionCellIds;
	TArray<FSRPlanetSurfaceGridCellId> OutputConnectionCellIds;
	const int32 SafeFootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacementRotationSteps);
	const int32 SafeFootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacementRotationSteps);

	for (const FSRStructurePortSpec& InputPort : StructureData.InputPorts)
	{
		const FSRStructurePortSpec RotatedInputPort = StarRovers::Structure::RotateStructurePortSpec(InputPort, StructureData, PlacementRotationSteps);
		AppendGhostPortPreviewCell(
			SurfaceGrid,
			FootprintCellIds,
			SafeFootprintCellsX,
			SafeFootprintCellsY,
			RotatedInputPort,
			InputConnectionCellIds);
	}

	for (const FSRStructurePortSpec& OutputPort : StructureData.OutputPorts)
	{
		const FSRStructurePortSpec RotatedOutputPort = StarRovers::Structure::RotateStructurePortSpec(OutputPort, StructureData, PlacementRotationSteps);
		AppendGhostPortPreviewCell(
			SurfaceGrid,
			FootprintCellIds,
			SafeFootprintCellsX,
			SafeFootprintCellsY,
			RotatedOutputPort,
			OutputConnectionCellIds);
	}

	if (InputConnectionCellIds.IsEmpty() && OutputConnectionCellIds.IsEmpty())
	{
		ClearGhostPortPreview();
		return;
	}

	if (IsValid(StructureGhostPortPreviewSurfaceGrid) && StructureGhostPortPreviewSurfaceGrid != SurfaceGrid)
	{
		StructureGhostPortPreviewSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	SurfaceGrid->SetFacilityPortPreviewCells(InputConnectionCellIds, OutputConnectionCellIds);
	StructureGhostPortPreviewSurfaceGrid = SurfaceGrid;
	bHasStructureGhostPortPreview = true;
}

bool FSRAssemblyStructurePreviewState::UpdateGhostActor(
	UWorld* World,
	AActor* Owner,
	USRPlanetSurfaceGrid* HoveredSurfaceGrid,
	USRStructureDataAsset* StructureDataAsset,
	const FSRStructureData& StructureData,
	const FTransform& GhostTransform,
	const FSRPlanetSurfaceGridCellInfo& PreviewCellInfo,
	UMaterialInterface* PreviewMaterial)
{
	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		StructureActorClass = ASRStructure::StaticClass();
	}
	if (!IsValid(StructureActorClass))
	{
		LogInvalidGhostDataAssetOnce(StructureDataAsset, TEXT("StructureActorClass is not set"));
		DestroyGhostActor(HoveredSurfaceGrid);
		return false;
	}

	if (!StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		LogInvalidGhostDataAssetOnce(StructureDataAsset, TEXT("StructureActorClass does not implement ISRBuildableStructureInterface"));
		DestroyGhostActor(HoveredSurfaceGrid);
		return false;
	}

	const bool bNeedsNewGhostActor = !IsValid(StructureGhostActor)
		|| StructureGhostDataAsset != StructureDataAsset
		|| StructureGhostActor->GetClass() != StructureActorClass;
	if (bNeedsNewGhostActor)
	{
		DestroyGhostActor(HoveredSurfaceGrid);
		if (!IsValid(World))
		{
			return false;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = Owner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		StructureGhostActor = World->SpawnActor<AActor>(StructureActorClass, GhostTransform, SpawnParameters);
		if (!IsValid(StructureGhostActor))
		{
			return false;
		}

		StructureGhostDataAsset = StructureDataAsset;
		ISRBuildableStructureInterface::Execute_ApplyStructureDataAsset(StructureGhostActor, StructureDataAsset);
		ISRBuildableStructureInterface::Execute_SetStructureGhostMode(StructureGhostActor, true);
		StarRovers::Assembly::PreviewMaterials::ApplyToActor(StructureGhostActor, PreviewMaterial);
		if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(StructureGhostActor, PreviewCellInfo))
		{
			DestroyGhostActor(HoveredSurfaceGrid);
			return false;
		}
		StructureGhostActor->SetActorHiddenInGame(false);
		LastLoggedInvalidGhostDataAsset = nullptr;
	}
	else if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(StructureGhostActor, PreviewCellInfo))
	{
		DestroyGhostActor(HoveredSurfaceGrid);
		return false;
	}

	ISRBuildableStructureInterface::Execute_SetStructureGhostMode(StructureGhostActor, true);
	StarRovers::Assembly::PreviewMaterials::ApplyToActor(StructureGhostActor, PreviewMaterial);
	StructureGhostActor->SetActorTransform(GhostTransform);
	StructureGhostActor->SetActorHiddenInGame(false);
	return true;
}

void FSRAssemblyStructurePreviewState::DestroyGhostActor(USRPlanetSurfaceGrid* HoveredSurfaceGrid)
{
	ClearGhostPortPreview();

	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearConstructionReplacementPreviewCells();
		if (AActor* SurfaceOwner = HoveredSurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearDeletePreviewedStructures();
			}
		}
	}

	if (IsValid(StructureGhostActor))
	{
		StructureGhostActor->Destroy();
	}

	StructureGhostActor = nullptr;
	StructureGhostDataAsset = nullptr;
	StructureGhostCellId = FSRPlanetSurfaceGridCellId();
	bHasStructureGhostCellId = false;
}

void FSRAssemblyStructurePreviewState::DestroyPlacementDragPreviewActors(USRPlanetSurfaceGrid* HoveredSurfaceGrid)
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearConstructionReplacementPreviewCells();
		if (AActor* SurfaceOwner = HoveredSurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearDeletePreviewedStructures();
			}
		}
	}

	for (FSRStructurePlacementDragPreviewActor& PreviewInfo : StructurePlacementDragPreviewActors)
	{
		if (AActor* PreviewActor = PreviewInfo.PreviewActor.Get())
		{
			PreviewActor->Destroy();
		}
	}

	StructurePlacementDragPreviewActors.Reset();
}

AActor* FSRAssemblyStructurePreviewState::SpawnPlacementDragPreviewActor(
	UWorld* World,
	AActor* FallbackOwner,
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRStructureDataAsset* StructureDataAsset) const
{
	if (!IsValid(World) || !IsValid(SurfaceGrid) || !IsValid(StructureDataAsset))
	{
		return nullptr;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		StructureActorClass = ASRStructure::StaticClass();
	}
	if (!IsValid(StructureActorClass)
		|| !StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		return nullptr;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = IsValid(SurfaceOwner) ? SurfaceOwner : FallbackOwner;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;

	AActor* PreviewActor = World->SpawnActor<AActor>(StructureActorClass, FTransform::Identity, SpawnParameters);
	if (!IsValid(PreviewActor))
	{
		return nullptr;
	}

	if (IsValid(SurfaceOwner))
	{
		PreviewActor->SetOwner(SurfaceOwner);
		PreviewActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform);
	}

	ISRBuildableStructureInterface::Execute_ApplyStructureDataAsset(PreviewActor, StructureDataAsset);
	ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PreviewActor, true);
	PreviewActor->SetActorEnableCollision(false);
	PreviewActor->SetActorHiddenInGame(true);
	return PreviewActor;
}

void FSRAssemblyStructurePreviewState::LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason)
{
	(void)Reason;

	if (!IsValid(StructureDataAsset) || LastLoggedInvalidGhostDataAsset == StructureDataAsset)
	{
		return;
	}

	LastLoggedInvalidGhostDataAsset = StructureDataAsset;
}

void FSRAssemblyConveyorPreviewState::ClearPortPreview()
{
	if (bHasConveyorPortPreview && IsValid(ConveyorPortPreviewSurfaceGrid))
	{
		ConveyorPortPreviewSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	ConveyorPortPreviewSurfaceGrid = nullptr;
	bHasConveyorPortPreview = false;
}

void FSRAssemblyConveyorPreviewState::SetInvalidPlacementPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid))
	{
		ClearInvalidPlacementPreview();
		return;
	}

	if (IsValid(ConveyorInvalidPlacementPreviewSurfaceGrid) && ConveyorInvalidPlacementPreviewSurfaceGrid != SurfaceGrid)
	{
		ConveyorInvalidPlacementPreviewSurfaceGrid->ClearInvalidPreviewCells();
	}

	SurfaceGrid->SetInvalidPreviewCells(CellIds);
	ConveyorInvalidPlacementPreviewSurfaceGrid = SurfaceGrid;
	bHasConveyorInvalidPlacementPreview = true;
}

void FSRAssemblyConveyorPreviewState::ClearInvalidPlacementPreview()
{
	if (bHasConveyorInvalidPlacementPreview && IsValid(ConveyorInvalidPlacementPreviewSurfaceGrid))
	{
		ConveyorInvalidPlacementPreviewSurfaceGrid->ClearInvalidPreviewCells();
	}

	ConveyorInvalidPlacementPreviewSurfaceGrid = nullptr;
	bHasConveyorInvalidPlacementPreview = false;
}

void FSRAssemblyConveyorPreviewState::SetBulkDeletionPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid))
	{
		ClearBulkDeletionPreview();
		return;
	}

	if (IsValid(ConveyorBulkDeletionPreviewSurfaceGrid) && ConveyorBulkDeletionPreviewSurfaceGrid != SurfaceGrid)
	{
		ConveyorBulkDeletionPreviewSurfaceGrid->ClearDeletionPreviewCells();
	}

	SurfaceGrid->SetDeletionPreviewCells(CellIds);
	ConveyorBulkDeletionPreviewSurfaceGrid = SurfaceGrid;
	bHasConveyorBulkDeletionPreview = true;
}

void FSRAssemblyConveyorPreviewState::ClearBulkDeletionPreview()
{
	if (bHasConveyorBulkDeletionPreview && IsValid(ConveyorBulkDeletionPreviewSurfaceGrid))
	{
		ConveyorBulkDeletionPreviewSurfaceGrid->ClearDeletionPreviewCells();
	}

	ConveyorBulkDeletionPreviewSurfaceGrid = nullptr;
	bHasConveyorBulkDeletionPreview = false;
	DestroyDeletionGhostActor();
}

bool FSRAssemblyConveyorPreviewState::IsGhostActorCurrent(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRStructureDataAsset* ConveyorDataAsset,
	const FSRPlanetSurfaceGridCellId& TargetCellId) const
{
	return IsValid(ConveyorGhostActor)
		&& ConveyorGhostDataAsset == ConveyorDataAsset
		&& ConveyorGhostSurfaceGrid == SurfaceGrid
		&& bHasConveyorGhostTargetCell
		&& ConveyorGhostTargetCellId == TargetCellId;
}

ESRAssemblyConveyorGhostUpdateResult FSRAssemblyConveyorPreviewState::UpdateGhostActor(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRPlanetSurfaceGrid* CleanupSurfaceGrid,
	USRStructureDataAsset* ConveyorDataAsset,
	const FSRStructureData& ConveyorData,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	FName ConveyorActorSplineComponentTag,
	float ConveyorActorSurfaceOffset,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	UMaterialInterface* PreviewMaterial)
{
	UClass* ConveyorActorClass = ConveyorData.StructureActorClass.Get();
	if (!IsValid(ConveyorActorClass) || !ConveyorActorClass->IsChildOf(ASRConveyorBeltActor::StaticClass()))
	{
		DestroyGhostActor(CleanupSurfaceGrid);
		return ESRAssemblyConveyorGhostUpdateResult::Failed;
	}

	const bool bNeedsNewGhostActor = !IsValid(ConveyorGhostActor)
		|| ConveyorGhostDataAsset != ConveyorDataAsset
		|| ConveyorGhostSurfaceGrid != SurfaceGrid
		|| ConveyorGhostActor->GetClass() != ConveyorActorClass;
	if (bNeedsNewGhostActor)
	{
		DestroyGhostActor(CleanupSurfaceGrid);

		AActor* SurfaceOwner = IsValid(SurfaceGrid) ? SurfaceGrid->GetOwner() : nullptr;
		UWorld* World = IsValid(SurfaceOwner) ? SurfaceOwner->GetWorld() : nullptr;
		if (!World)
		{
			return ESRAssemblyConveyorGhostUpdateResult::Failed;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = SurfaceOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		ConveyorGhostActor = World->SpawnActor<ASRConveyorBeltActor>(
			ConveyorActorClass,
			SurfaceOwner->GetActorTransform(),
			SpawnParameters);
		if (!IsValid(ConveyorGhostActor))
		{
			return ESRAssemblyConveyorGhostUpdateResult::Failed;
		}

		ConveyorGhostActor->SetOwner(SurfaceOwner);
		ConveyorGhostActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform);
		ConveyorGhostActor->SetActorHiddenInGame(false);
		ConveyorGhostActor->SetConveyorGhostMode(true, IsValid(PreviewMaterial) ? PreviewMaterial : ConveyorData.GhostMaterial.Get());
		ConveyorGhostDataAsset = ConveyorDataAsset;
		ConveyorGhostSurfaceGrid = SurfaceGrid;
	}

	if (!ConveyorGhostActor->InitializeConveyorPaths(
		SurfaceGrid,
		BeltPaths,
		ConveyorActorSplineComponentTag,
		ConveyorActorSurfaceOffset))
	{
		DestroyGhostActor(CleanupSurfaceGrid);
		return ESRAssemblyConveyorGhostUpdateResult::PreviewFailed;
	}

	ConveyorGhostActor->SetConveyorGhostMode(true, IsValid(PreviewMaterial) ? PreviewMaterial : ConveyorData.GhostMaterial.Get());
	ConveyorGhostActor->SetActorHiddenInGame(ConveyorGhostActor->IsConveyorGhostGenerationPending());
	ConveyorGhostTargetCellId = TargetCellId;
	bHasConveyorGhostTargetCell = true;
	return ESRAssemblyConveyorGhostUpdateResult::Updated;
}

void FSRAssemblyConveyorPreviewState::DestroyGhostActor(USRPlanetSurfaceGrid* HoveredSurfaceGrid)
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearConstructionReplacementPreviewCells();
		if (AActor* SurfaceOwner = HoveredSurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearDeletePreviewedStructures();
			}
		}
	}

	if (IsValid(ConveyorGhostActor))
	{
		ConveyorGhostActor->Destroy();
	}

	ConveyorGhostActor = nullptr;
	ConveyorGhostDataAsset = nullptr;
	ConveyorGhostSurfaceGrid = nullptr;
	ConveyorGhostTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasConveyorGhostTargetCell = false;
}

bool FSRAssemblyConveyorPreviewState::IsDeletionGhostActorCurrent(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRStructureDataAsset* ConveyorDataAsset,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	int32 Layer) const
{
	return IsValid(ConveyorDeletionGhostActor)
		&& ConveyorDeletionGhostDataAsset == ConveyorDataAsset
		&& ConveyorDeletionGhostSurfaceGrid == SurfaceGrid
		&& bHasConveyorDeletionGhostTargetCell
		&& ConveyorDeletionGhostTargetCellId == TargetCellId
		&& ConveyorDeletionGhostLayer == FMath::Max(0, Layer);
}

bool FSRAssemblyConveyorPreviewState::UpdateDeletionGhostActor(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRStructureDataAsset* ConveyorDataAsset,
	const FSRStructureData& ConveyorData,
	const TArray<FSRConveyorBeltPath>& BeltPaths,
	FName ConveyorActorSplineComponentTag,
	float ConveyorActorSurfaceOffset,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	int32 Layer)
{
	UClass* ConveyorActorClass = ConveyorData.StructureActorClass.Get();
	if (!IsValid(ConveyorActorClass) || !ConveyorActorClass->IsChildOf(ASRConveyorBeltActor::StaticClass()))
	{
		DestroyDeletionGhostActor();
		return false;
	}

	const bool bNeedsNewGhostActor = !IsValid(ConveyorDeletionGhostActor)
		|| ConveyorDeletionGhostDataAsset != ConveyorDataAsset
		|| ConveyorDeletionGhostSurfaceGrid != SurfaceGrid
		|| ConveyorDeletionGhostActor->GetClass() != ConveyorActorClass;
	if (bNeedsNewGhostActor)
	{
		DestroyDeletionGhostActor();

		AActor* SurfaceOwner = IsValid(SurfaceGrid) ? SurfaceGrid->GetOwner() : nullptr;
		UWorld* World = IsValid(SurfaceOwner) ? SurfaceOwner->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = SurfaceOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		ConveyorDeletionGhostActor = World->SpawnActor<ASRConveyorBeltActor>(
			ConveyorActorClass,
			SurfaceOwner->GetActorTransform(),
			SpawnParameters);
		if (!IsValid(ConveyorDeletionGhostActor))
		{
			return false;
		}

		ConveyorDeletionGhostActor->SetOwner(SurfaceOwner);
		ConveyorDeletionGhostActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform);
		ConveyorDeletionGhostActor->SetActorHiddenInGame(false);
		ConveyorDeletionGhostDataAsset = ConveyorDataAsset;
		ConveyorDeletionGhostSurfaceGrid = SurfaceGrid;
	}

	if (!ConveyorDeletionGhostActor->InitializeConveyorPaths(
		SurfaceGrid,
		BeltPaths,
		ConveyorActorSplineComponentTag,
		ConveyorActorSurfaceOffset))
	{
		DestroyDeletionGhostActor();
		return false;
	}

	ConveyorDeletionGhostActor->SetConveyorGhostMode(true, ConveyorData.GhostMaterial);
	ConveyorDeletionGhostActor->SetActorHiddenInGame(false);
	ConveyorDeletionGhostTargetCellId = TargetCellId;
	bHasConveyorDeletionGhostTargetCell = true;
	ConveyorDeletionGhostLayer = FMath::Max(0, Layer);
	return true;
}

void FSRAssemblyConveyorPreviewState::DestroyDeletionGhostActor()
{
	if (IsValid(ConveyorDeletionGhostActor))
	{
		ConveyorDeletionGhostActor->Destroy();
	}

	ConveyorDeletionGhostActor = nullptr;
	ConveyorDeletionGhostDataAsset = nullptr;
	ConveyorDeletionGhostSurfaceGrid = nullptr;
	ConveyorDeletionGhostTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasConveyorDeletionGhostTargetCell = false;
	ConveyorDeletionGhostLayer = 0;
}

void FSRAssemblyPreviewReset::Apply(
	FSRAssemblyStructurePreviewState& StructurePreview,
	FSRAssemblyConveyorPreviewState& ConveyorPreview,
	USRPlanetSurfaceGrid* HoveredSurfaceGrid,
	const FSRAssemblyPreviewResetOptions& Options)
{
	if (Options.bClearConveyorPortPreview)
	{
		ConveyorPreview.ClearPortPreview();
	}
	if (Options.bClearConveyorBulkDeletionPreview)
	{
		ConveyorPreview.ClearBulkDeletionPreview();
	}
	if (Options.bClearConveyorInvalidPlacementPreview)
	{
		ConveyorPreview.ClearInvalidPlacementPreview();
	}
	if (Options.bDestroyStructureGhostActor)
	{
		StructurePreview.DestroyGhostActor(HoveredSurfaceGrid);
	}
	if (Options.bDestroyStructurePlacementDragPreviewActors)
	{
		StructurePreview.DestroyPlacementDragPreviewActors(HoveredSurfaceGrid);
	}
	if (Options.bDestroyConveyorGhostActor)
	{
		ConveyorPreview.DestroyGhostActor(HoveredSurfaceGrid);
	}
	if (Options.bDestroyConveyorDeletionGhostActor)
	{
		ConveyorPreview.DestroyDeletionGhostActor();
	}
}
