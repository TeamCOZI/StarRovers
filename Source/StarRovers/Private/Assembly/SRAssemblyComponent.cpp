#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRCameraPawn.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr uint64 StructureGhostPlacementDebugMessageKey = 0x535247686F73744FULL;
}

USRAssemblyComponent::USRAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bAssemblyModeActive = false;
	ActiveAssemblySurfaceGrid = nullptr;
	LastHoveredSampleSurfaceGrid = nullptr;
	LastPublishedHoveredSurfaceGrid = nullptr;
	StructureGhostActor = nullptr;
	StructureGhostDataAsset = nullptr;
	LastLoggedInvalidGhostDataAsset = nullptr;
	LastHoveredSampleMousePosition = FVector2D::ZeroVector;
	bHasLastHoveredSampleMousePosition = false;
	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	StructureGhostCellId = FSRPlanetSurfaceGridCellId();
	bHasStructureGhostCellId = false;
}

void USRAssemblyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSurfaceHover();
	UpdateStructureGhostPreview();
}

void USRAssemblyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyStructureGhostPreview();

	Super::EndPlay(EndPlayReason);
}

bool USRAssemblyComponent::IsAssemblyModeActive() const
{
	return bAssemblyModeActive;
}

void USRAssemblyComponent::SetAssemblyModeActive(bool bNewAssemblyModeActive)
{
	if (bNewAssemblyModeActive)
	{
		AActor* FocusedActor = nullptr;
		USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
		if (!TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid))
		{
			bNewAssemblyModeActive = false;
		}
	}

	if (bAssemblyModeActive == bNewAssemblyModeActive)
	{
		ApplyAssemblyModeToFocusedSurfaceGrid();
		return;
	}

	bAssemblyModeActive = bNewAssemblyModeActive;
	ResetHoverSampleCache();
	ApplyAssemblyModeToFocusedSurfaceGrid();
	if (!bAssemblyModeActive)
	{
		DestroyStructureGhostPreview();
	}
}

void USRAssemblyComponent::ToggleAssemblyMode()
{
	SetAssemblyModeActive(!bAssemblyModeActive);
}

bool USRAssemblyComponent::TryHandleAssemblyClick(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !bAssemblyModeActive)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;

	if (TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid)
		&& TryProjectCursorToSurfaceCell(FocusedSurfaceGrid, HoveredCell, HoverHitLocation))
	{
		OutSelectedActor = FocusedActor;
		if (PlayerController->GetSelectedStructureDataAsset())
		{
			FocusedSurfaceGrid->ClearSelectedCell();
			TryPlaceSelectedStructure(FocusedSurfaceGrid, HoveredCell);
			return true;
		}

		FocusedSurfaceGrid->SetSelectedCell(HoveredCell.CellId);
		return true;
	}

	return false;
}

void USRAssemblyComponent::ClearSurfaceGridInteraction(AActor* SurfaceActor)
{
	USRPlanetSurfaceGrid* CurrentSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SurfaceActor);
	if (CurrentSurfaceGrid)
	{
		CurrentSurfaceGrid->ClearHoveredCell();
		CurrentSurfaceGrid->ClearSelectedCell();
		CurrentSurfaceGrid->SetGridVisible(false);
	}
	if (CurrentSurfaceGrid == ActiveAssemblySurfaceGrid)
	{
		ActiveAssemblySurfaceGrid = nullptr;
	}

	if (!IsValid(SurfaceActor) || CurrentSurfaceGrid == HoveredSurfaceGrid)
	{
		HoveredSurfaceGrid = nullptr;
	}
	ClearPublishedHoveredCellInfo();
	ResetHoverSampleCache();
	DestroyStructureGhostPreview();
}

void USRAssemblyComponent::ClearSurfaceHover()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = nullptr;
	ClearPublishedHoveredCellInfo();
	ResetHoverSampleCache();
	DestroyStructureGhostPreview();
}

ASRPlayerController* USRAssemblyComponent::GetOwnerController() const
{
	return Cast<ASRPlayerController>(GetOwner());
}

bool USRAssemblyComponent::GetCursorRay(FVector& OutRayOrigin, FVector& OutRayDirection) const
{
	OutRayOrigin = FVector::ZeroVector;
	OutRayDirection = FVector::ZeroVector;

	const ASRPlayerController* PlayerController = GetOwnerController();
	return PlayerController
		&& PlayerController->DeprojectMousePositionToWorld(OutRayOrigin, OutRayDirection)
		&& !OutRayDirection.IsNearlyZero();
}

bool USRAssemblyComponent::TryGetFocusedSurfaceGrid(AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid) const
{
	OutFocusedActor = nullptr;
	OutSurfaceGrid = nullptr;

	const ASRPlayerController* PlayerController = GetOwnerController();
	const ASRCameraPawn* CameraPawn = PlayerController ? Cast<ASRCameraPawn>(PlayerController->GetPawn()) : nullptr;
	if (!CameraPawn)
	{
		return false;
	}

	AActor* FocusedActor = CameraPawn->GetFocusedActor();
	if (!IsValid(FocusedActor))
	{
		return false;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor);
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	OutFocusedActor = FocusedActor;
	OutSurfaceGrid = SurfaceGrid;
	return true;
}

bool USRAssemblyComponent::TryProjectCursorToSurfaceCell(USRPlanetSurfaceGrid* SurfaceGrid, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const
{
	OutCell = FSRPlanetSurfaceGridCell();
	OutHitLocation = FVector::ZeroVector;

	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	if (AActor* OwnerActor = SurfaceGrid->GetOwner())
	{
		OwnerActor->UpdateComponentTransforms();
	}
	SurfaceGrid->UpdateComponentToWorld();

	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (!GetCursorRay(RayOrigin, RayDirection))
	{
		return false;
	}

	return SurfaceGrid->RaycastCell(RayOrigin, RayDirection, OutCell, OutHitLocation);
}

void USRAssemblyComponent::UpdateSurfaceHover()
{
	if (!bAssemblyModeActive)
	{
		ClearSurfaceHover();
		return;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController)
	{
		ClearSurfaceHover();
		return;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid))
	{
		ClearSurfaceHover();
		return;
	}

	FVector2D CurrentMousePosition = FVector2D::ZeroVector;
	const bool bHasMousePosition = PlayerController->GetMousePosition(CurrentMousePosition.X, CurrentMousePosition.Y);
	if (bHasMousePosition
		&& bHasLastHoveredSampleMousePosition
		&& LastHoveredSampleSurfaceGrid == SurfaceGrid
		&& HoveredSurfaceGrid == SurfaceGrid
		&& SurfaceGrid->HasHoveredCell()
		&& FVector2D::Distance(CurrentMousePosition, LastHoveredSampleMousePosition) <= 0.5f)
	{
		return;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;
	if (!TryProjectCursorToSurfaceCell(SurfaceGrid, HoveredCell, HoverHitLocation))
	{
		ClearSurfaceHover();
		return;
	}

	if (HoveredSurfaceGrid && HoveredSurfaceGrid != SurfaceGrid)
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = SurfaceGrid;
	HoveredSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, HoveredCell);
	if (bHasMousePosition)
	{
		LastHoveredSampleSurfaceGrid = SurfaceGrid;
		LastHoveredSampleMousePosition = CurrentMousePosition;
		bHasLastHoveredSampleMousePosition = true;
	}
}

void USRAssemblyComponent::ApplyAssemblyModeToFocusedSurfaceGrid()
{
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	const bool bHasFocusedSurfaceGrid = TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid);
	USRPlanetSurfaceGrid* DesiredSurfaceGrid = bAssemblyModeActive && bHasFocusedSurfaceGrid ? FocusedSurfaceGrid : nullptr;

	if (ActiveAssemblySurfaceGrid && ActiveAssemblySurfaceGrid != DesiredSurfaceGrid)
	{
		ActiveAssemblySurfaceGrid->SetGridVisible(false);
	}

	ActiveAssemblySurfaceGrid = DesiredSurfaceGrid;
	if (ActiveAssemblySurfaceGrid)
	{
		ActiveAssemblySurfaceGrid->SetGridVisible(true);
	}

	if (!bAssemblyModeActive)
	{
		ClearSurfaceHover();
		DestroyStructureGhostPreview();
	}
}

void USRAssemblyComponent::ResetHoverSampleCache()
{
	LastHoveredSampleSurfaceGrid = nullptr;
	LastHoveredSampleMousePosition = FVector2D::ZeroVector;
	bHasLastHoveredSampleMousePosition = false;
}

void USRAssemblyComponent::PublishHoveredCellInfo(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !IsValid(SurfaceGrid))
	{
		return;
	}

	if (bHasLastPublishedHoveredCellInfo
		&& LastPublishedHoveredSurfaceGrid == SurfaceGrid
		&& LastPublishedHoveredCellId == HoveredCell.CellId)
	{
		return;
	}

	FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
	if (!SurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo))
	{
		ClearPublishedHoveredCellInfo();
		return;
	}

	bHasLastPublishedHoveredCellInfo = true;
	LastPublishedHoveredSurfaceGrid = SurfaceGrid;
	LastPublishedHoveredCellId = HoveredCell.CellId;
	PlayerController->SetHoveredSurfaceCellInfo(true, HoveredCellInfo);
}

void USRAssemblyComponent::ClearPublishedHoveredCellInfo()
{
	if (!bHasLastPublishedHoveredCellInfo)
	{
		return;
	}

	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	if (ASRPlayerController* PlayerController = GetOwnerController())
	{
		PlayerController->SetHoveredSurfaceCellInfo(false, FSRPlanetSurfaceGridCellInfo());
	}
}

void USRAssemblyComponent::UpdateStructureGhostPreview()
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!bAssemblyModeActive || !IsValid(PlayerController) || !IsValid(SelectedStructureDataAsset) || !IsValid(HoveredSurfaceGrid))
	{
		DestroyStructureGhostPreview();
		return;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	if (!HoveredSurfaceGrid->GetHoveredCell(HoveredCell))
	{
		DestroyStructureGhostPreview();
		return;
	}

	FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
	if (!HoveredSurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo)
		|| !HoveredCellInfo.bCanConstruct
		|| HoveredCellInfo.bOccupied)
	{
		DestroyStructureGhostPreview();
		return;
	}

	const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		LogInvalidGhostDataAssetOnce(SelectedStructureDataAsset, TEXT("StructureActorClass is not set"));
		DestroyStructureGhostPreview();
		return;
	}

	if (!StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		LogInvalidGhostDataAssetOnce(SelectedStructureDataAsset, TEXT("StructureActorClass does not implement ISRBuildableStructureInterface"));
		DestroyStructureGhostPreview();
		return;
	}

	FTransform GhostTransform;
	if (!BuildStructureGhostTransform(HoveredSurfaceGrid, HoveredCell.CellId, SelectedStructureDataAsset, GhostTransform))
	{
		DestroyStructureGhostPreview();
		return;
	}

	const bool bNeedsNewGhostActor = !IsValid(StructureGhostActor)
		|| StructureGhostDataAsset != SelectedStructureDataAsset
		|| StructureGhostActor->GetClass() != StructureActorClass;
	if (bNeedsNewGhostActor)
	{
		DestroyStructureGhostPreview();

		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = PlayerController;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		StructureGhostActor = World->SpawnActor<AActor>(StructureActorClass, GhostTransform, SpawnParameters);
		if (!IsValid(StructureGhostActor))
		{
			return;
		}

		StructureGhostDataAsset = SelectedStructureDataAsset;
		ISRBuildableStructureInterface::Execute_ApplyStructureDataAsset(StructureGhostActor, SelectedStructureDataAsset);
		ISRBuildableStructureInterface::Execute_SetStructureGhostMode(StructureGhostActor, true);
		if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(StructureGhostActor, HoveredCellInfo))
		{
			DestroyStructureGhostPreview();
			return;
		}
		StructureGhostActor->SetActorHiddenInGame(false);
		LastLoggedInvalidGhostDataAsset = nullptr;
	}
	else if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(StructureGhostActor, HoveredCellInfo))
	{
		DestroyStructureGhostPreview();
		return;
	}

	StructureGhostActor->SetActorTransform(GhostTransform);
	StructureGhostActor->SetActorHiddenInGame(false);
	PublishStructureGhostPlacementDebug(
		HoveredSurfaceGrid,
		HoveredCell,
		GhostTransform,
		StructureData.ConstructionHeightOffset,
		!bHasStructureGhostCellId || !(StructureGhostCellId == HoveredCell.CellId));
	StructureGhostCellId = HoveredCell.CellId;
	bHasStructureGhostCellId = true;
}

void USRAssemblyComponent::DestroyStructureGhostPreview()
{
	if (IsValid(StructureGhostActor))
	{
		StructureGhostActor->Destroy();
	}

	StructureGhostActor = nullptr;
	StructureGhostDataAsset = nullptr;
	StructureGhostCellId = FSRPlanetSurfaceGridCellId();
	bHasStructureGhostCellId = false;
}

bool USRAssemblyComponent::BuildStructureGhostTransform(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	USRStructureDataAsset* StructureDataAsset,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (!SurfaceGrid->GetCellWorldTransform(CellId, StructureData.ConstructionHeightOffset, OutTransform))
	{
		return false;
	}

	if (StructureData.bAlignToSurfaceNormal)
	{
		const FQuat BaseRotation = OutTransform.GetRotation();
		const FVector SurfaceNormal = BaseRotation.GetAxisZ().GetSafeNormal();
		const FQuat YawRotation = SurfaceNormal.IsNearlyZero()
			? FQuat::Identity
			: FQuat(SurfaceNormal, FMath::DegreesToRadians(StructureData.PlacementYawDegrees));
		OutTransform.SetRotation(YawRotation * BaseRotation);
	}
	else
	{
		OutTransform.SetRotation(FRotator(0.0f, StructureData.PlacementYawDegrees, 0.0f).Quaternion());
	}

	return true;
}

void USRAssemblyComponent::PublishStructureGhostPlacementDebug(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& HoveredCell,
	const FTransform& GhostTransform,
	float StructureHeightOffset,
	bool bLogDebug) const
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	const AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	if (!IsValid(SurfaceOwner))
	{
		return;
	}

	const FTransform& OwnerTransform = SurfaceOwner->GetActorTransform();
	const FTransform& GridTransform = SurfaceGrid->GetComponentTransform();
	const FVector CellWorldPosition = GridTransform.TransformPosition(HoveredCell.LocalCenter);
	const FVector GhostWorldPosition = GhostTransform.GetLocation();
	const FVector CellPlanetLocalPosition = OwnerTransform.InverseTransformPosition(CellWorldPosition);
	const FVector GhostPlanetLocalPosition = OwnerTransform.InverseTransformPosition(GhostWorldPosition);
	const FVector PlanetLocalDelta = GhostPlanetLocalPosition - CellPlanetLocalPosition;
	const FVector CellPlanetLocalNormal = OwnerTransform.InverseTransformVectorNoScale(
		GridTransform.TransformVectorNoScale(HoveredCell.LocalNormal)).GetSafeNormal();
	const float NormalDelta = CellPlanetLocalNormal.IsNearlyZero()
		? 0.0f
		: FVector::DotProduct(PlanetLocalDelta, CellPlanetLocalNormal);
	const float GridHeightOffset = SurfaceGrid->GetConstructionHeightOffset();

	const FString DebugText = FString::Printf(
		TEXT("Structure Ghost Debug | Cell=(%d,%d,%d)\nCell Local=%s\nGhost Local=%s\nDelta=%s\nGridOffset=%.3f | StructureOffset=%.3f | NormalDelta=%.3f"),
		static_cast<int32>(HoveredCell.CellId.Face),
		HoveredCell.CellId.CellX,
		HoveredCell.CellId.CellY,
		*CellPlanetLocalPosition.ToCompactString(),
		*GhostPlanetLocalPosition.ToCompactString(),
		*PlanetLocalDelta.ToCompactString(),
		GridHeightOffset,
		StructureHeightOffset,
		NormalDelta);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			StructureGhostPlacementDebugMessageKey,
			0.1f,
			FColor::Cyan,
			DebugText);
	}

	if (bLogDebug)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *DebugText);
	}
}

bool USRAssemblyComponent::TryPlaceSelectedStructure(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(PlayerController) || !IsValid(SurfaceGrid) || !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo TargetCellInfo;
	if (!SurfaceGrid->GetCellInfoById(TargetCell.CellId, TargetCellInfo))
	{
		return false;
	}

	if (!TargetCellInfo.bCanConstruct || TargetCellInfo.bOccupied)
	{
		DestroyStructureGhostPreview();
		return false;
	}

	const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		LogInvalidGhostDataAssetOnce(SelectedStructureDataAsset, TEXT("StructureActorClass is not set"));
		return false;
	}

	if (!StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		LogInvalidGhostDataAssetOnce(SelectedStructureDataAsset, TEXT("StructureActorClass does not implement ISRBuildableStructureInterface"));
		return false;
	}

	FTransform StructureTransform;
	if (!BuildStructureGhostTransform(SurfaceGrid, TargetCell.CellId, SelectedStructureDataAsset, StructureTransform))
	{
		return false;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	if (!IsValid(SurfaceOwner))
	{
		return false;
	}

	const bool bPlacedFromGhost = IsValid(StructureGhostActor)
		&& StructureGhostDataAsset == SelectedStructureDataAsset
		&& StructureGhostActor->GetClass() == StructureActorClass
		&& bHasStructureGhostCellId
		&& StructureGhostCellId == TargetCell.CellId;

	AActor* PlacedStructureActor = bPlacedFromGhost ? StructureGhostActor.Get() : nullptr;
	if (!bPlacedFromGhost)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return false;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = SurfaceOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PlacedStructureActor = World->SpawnActor<AActor>(StructureActorClass, StructureTransform, SpawnParameters);
		if (!IsValid(PlacedStructureActor))
		{
			return false;
		}
	}

	auto RollbackPlacedStructureActor = [bPlacedFromGhost, PlayerController](AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		if (bPlacedFromGhost)
		{
			Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Actor->SetOwner(PlayerController);
			ISRBuildableStructureInterface::Execute_SetStructureGhostMode(Actor, true);
			return;
		}

		Actor->Destroy();
	};

	ISRBuildableStructureInterface::Execute_ApplyStructureDataAsset(PlacedStructureActor, SelectedStructureDataAsset);
	ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PlacedStructureActor, false);
	if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(PlacedStructureActor, TargetCellInfo))
	{
		RollbackPlacedStructureActor(PlacedStructureActor);
		return false;
	}

	PlacedStructureActor->SetOwner(SurfaceOwner);
	PlacedStructureActor->SetActorTransform(StructureTransform);
	PlacedStructureActor->SetActorHiddenInGame(false);
	if (!PlacedStructureActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform))
	{
		RollbackPlacedStructureActor(PlacedStructureActor);
		return false;
	}

	const FName OccupantId = FName(*PlacedStructureActor->GetName());
	if (!SurfaceGrid->SetCellOccupied(TargetCell.CellId, true, OccupantId))
	{
		RollbackPlacedStructureActor(PlacedStructureActor);
		return false;
	}

	if (bPlacedFromGhost)
	{
		StructureGhostActor = nullptr;
		StructureGhostDataAsset = nullptr;
		StructureGhostCellId = FSRPlanetSurfaceGridCellId();
		bHasStructureGhostCellId = false;
	}
	else
	{
		DestroyStructureGhostPreview();
	}

	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	return true;
}

void USRAssemblyComponent::LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason)
{
	if (!IsValid(StructureDataAsset) || LastLoggedInvalidGhostDataAsset == StructureDataAsset)
	{
		return;
	}

	LastLoggedInvalidGhostDataAsset = StructureDataAsset;
	UE_LOG(
		LogTemp,
		Error,
		TEXT("Structure ghost preview cannot use StructureDataAsset '%s': %s."),
		*GetNameSafe(StructureDataAsset),
		Reason ? Reason : TEXT("Invalid structure data"));
}
