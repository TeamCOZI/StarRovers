#include "Structure/SRStructurePlacementLibrary.h"

#include "Utility/SRLog.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Components/PrimitiveComponent.h"
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

	void EnableRenderCustomDepth(AActor* StructureActor)
	{
		if (!IsValid(StructureActor))
		{
			return;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		StructureActor->GetComponents(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (IsValid(PrimitiveComponent))
			{
				PrimitiveComponent->SetRenderCustomDepth(true);
			}
		}
	}
}

bool USRStructurePlacementLibrary::BuildStructurePlacementTransform(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	USRStructureDataAsset* StructureDataAsset,
	FTransform& OutTransform,
	float AdditionalYawDegrees)
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

	const float FinalYawDegrees = StructureData.PlacementYawDegrees + AdditionalYawDegrees;
	if (StructureData.bAlignToSurfaceNormal)
	{
		const FQuat BaseRotation = OutTransform.GetRotation();
		const FVector SurfaceNormal = BaseRotation.GetAxisZ().GetSafeNormal();
		const FQuat YawRotation = SurfaceNormal.IsNearlyZero()
			? FQuat::Identity
			: FQuat(SurfaceNormal, FMath::DegreesToRadians(FinalYawDegrees));
		OutTransform.SetRotation(YawRotation * BaseRotation);
	}
	else
	{
		OutTransform.SetRotation(FRotator(0.0f, FinalYawDegrees, 0.0f).Quaternion());
	}

	return true;
}

bool USRStructurePlacementLibrary::TryPlaceStructureOnSurfaceGrid(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	USRStructureDataAsset* StructureDataAsset,
	AActor*& OutPlacedStructureActor,
	bool bUseStaticMeshMaterials,
	int32 PlacementRotationSteps)
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
	const int32 NormalizedRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(PlacementRotationSteps);

	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		SR_LOG(Structure, LogTemp, Error, TEXT("Cannot place structure from '%s': StructureActorClass is not set."), *GetNameSafe(StructureDataAsset));
		return false;
	}

	if (!StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		SR_LOG(Structure, LogTemp, Error, TEXT("Cannot place structure from '%s': StructureActorClass does not implement ISRBuildableStructureInterface."), *GetNameSafe(StructureDataAsset));
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	if (!SurfaceGrid->GetFootprintCellIds(
		TargetCellId,
		StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, NormalizedRotationSteps),
		StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, NormalizedRotationSteps),
		FootprintCellIds)
		|| !SurfaceGrid->CanOccupyCells(FootprintCellIds))
	{
		return false;
	}

	FTransform StructureTransform;
	if (!BuildStructurePlacementTransform(
		SurfaceGrid,
		TargetCellId,
		StructureDataAsset,
		StructureTransform,
		StarRovers::Structure::PlacementRotationStepsToYawDegrees(NormalizedRotationSteps)))
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
	EnableRenderCustomDepth(PlacedStructureActor);
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
	if (USRFacilityNetworkComponent* FacilityNetwork = SurfaceOwner->FindComponentByClass<USRFacilityNetworkComponent>())
	{
		FacilityNetwork->RegisterFacility(OccupantId, StructureDataAsset, TargetCellId, FootprintCellIds, NormalizedRotationSteps);
	}

	OutPlacedStructureActor = PlacedStructureActor;
	return true;
}
