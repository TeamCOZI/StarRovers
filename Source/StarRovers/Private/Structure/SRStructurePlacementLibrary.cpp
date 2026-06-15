#include "Structure/SRStructurePlacementLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	void RestoreStaticMeshDefaultMaterials(AActor* StructureActor, UStaticMesh* ExpectedStaticMesh)
	{
		if (!IsValid(StructureActor) || !IsValid(ExpectedStaticMesh))
		{
			return;
		}

		TArray<UStaticMeshComponent*> StaticMeshComponents;
		StructureActor->GetComponents(StaticMeshComponents);
		for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
		{
			if (!IsValid(StaticMeshComponent) || StaticMeshComponent->GetStaticMesh() != ExpectedStaticMesh)
			{
				continue;
			}

			const int32 MaterialSlotCount = ExpectedStaticMesh->GetStaticMaterials().Num();
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
			{
				StaticMeshComponent->SetMaterial(MaterialIndex, ExpectedStaticMesh->GetMaterial(MaterialIndex));
			}
		}
	}
}

bool USRStructurePlacementLibrary::BuildStructurePlacementTransform(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	USRStructureDataAsset* StructureDataAsset,
	FTransform& OutTransform)
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

bool USRStructurePlacementLibrary::TryPlaceStructureOnSurfaceGrid(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	USRStructureDataAsset* StructureDataAsset,
	AActor*& OutPlacedStructureActor,
	bool bUseStaticMeshMaterials)
{
	OutPlacedStructureActor = nullptr;
	if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset))
	{
		return false;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	UWorld* World = SurfaceOwner ? SurfaceOwner->GetWorld() : nullptr;
	if (!IsValid(SurfaceOwner) || !World)
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo TargetCellInfo;
	if (!SurfaceGrid->GetCellInfoById(TargetCellId, TargetCellInfo))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (StructureData.BuildKind != ESRStructureBuildKind::Structure)
	{
		return false;
	}

	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot place structure from '%s': StructureActorClass is not set."), *GetNameSafe(StructureDataAsset));
		return false;
	}

	if (!StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot place structure from '%s': StructureActorClass does not implement ISRBuildableStructureInterface."), *GetNameSafe(StructureDataAsset));
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	if (!SurfaceGrid->GetFootprintCellIds(TargetCellId, StructureData.FootprintCellsX, StructureData.FootprintCellsY, FootprintCellIds)
		|| !SurfaceGrid->CanOccupyCells(FootprintCellIds))
	{
		return false;
	}

	FTransform StructureTransform;
	if (!BuildStructurePlacementTransform(SurfaceGrid, TargetCellId, StructureDataAsset, StructureTransform))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = SurfaceOwner;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* PlacedStructureActor = World->SpawnActor<AActor>(StructureActorClass, StructureTransform, SpawnParameters);
	if (!IsValid(PlacedStructureActor))
	{
		return false;
	}

	auto RollbackPlacedStructureActor = [SurfaceGrid, &FootprintCellIds](AActor* Actor)
	{
		if (IsValid(SurfaceGrid) && !FootprintCellIds.IsEmpty())
		{
			SurfaceGrid->SetCellsOccupied(FootprintCellIds, false, NAME_None);
		}
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	};

	ISRBuildableStructureInterface::Execute_ApplyStructureDataAsset(PlacedStructureActor, StructureDataAsset);
	ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PlacedStructureActor, false);
	if (bUseStaticMeshMaterials)
	{
		RestoreStaticMeshDefaultMaterials(PlacedStructureActor, StructureData.StaticMesh.Get());
	}
	if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(PlacedStructureActor, TargetCellInfo))
	{
		RollbackPlacedStructureActor(PlacedStructureActor);
		return false;
	}

	PlacedStructureActor->SetOwner(SurfaceOwner);
	PlacedStructureActor->SetActorTransform(StructureTransform);
	PlacedStructureActor->SetActorHiddenInGame(false);
	PlacedStructureActor->SetActorEnableCollision(false);
	if (!PlacedStructureActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform))
	{
		RollbackPlacedStructureActor(PlacedStructureActor);
		return false;
	}

	const FName OccupantId = FName(*PlacedStructureActor->GetName());
	if (!SurfaceGrid->SetCellsOccupied(FootprintCellIds, true, OccupantId))
	{
		RollbackPlacedStructureActor(PlacedStructureActor);
		return false;
	}

	OutPlacedStructureActor = PlacedStructureActor;
	return true;
}
