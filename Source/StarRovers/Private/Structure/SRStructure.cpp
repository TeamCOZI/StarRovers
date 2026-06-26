#include "Structure/SRStructure.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

ASRStructure::ASRStructure()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StructureStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureStaticMesh"));
	StructureStaticMesh->SetupAttachment(SceneRoot);
	StructureStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StructureStaticMesh->SetGenerateOverlapEvents(false);
	StructureStaticMesh->SetRenderCustomDepth(true);
}

void ASRStructure::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (InitialStructureDataAsset)
	{
		ApplyStructureDataAsset_Implementation(InitialStructureDataAsset);
	}
	else
	{
		ApplyStructureVisuals();
	}
}

void ASRStructure::BeginPlay()
{
	Super::BeginPlay();

	if (InitialStructureDataAsset && !CurrentStructureDataAsset)
	{
		ApplyStructureDataAsset_Implementation(InitialStructureDataAsset);
	}
}

void ASRStructure::SetInitialStructureDataAsset(USRStructureDataAsset* NewStructureDataAsset)
{
	InitialStructureDataAsset = NewStructureDataAsset;
	ApplyStructureDataAsset_Implementation(NewStructureDataAsset);
}

USRStructureDataAsset* ASRStructure::GetStructureDataAsset() const
{
	return CurrentStructureDataAsset;
}

FSRStructureData ASRStructure::GetStructureData() const
{
	return AppliedStructureData;
}

bool ASRStructure::IsStructureGhostMode() const
{
	return bStructureGhostMode;
}

void ASRStructure::ApplyStructureDataAsset_Implementation(USRStructureDataAsset* StructureDataAsset)
{
	CurrentStructureDataAsset = StructureDataAsset;
	AppliedStructureData = StructureDataAsset ? StructureDataAsset->BuildData() : FSRStructureData();
	ApplyStructureVisuals();
}

void ASRStructure::SetStructureGhostMode_Implementation(bool bNewGhostMode)
{
	if (bStructureGhostMode == bNewGhostMode)
	{
		return;
	}

	bStructureGhostMode = bNewGhostMode;
	ApplyStructureVisuals();
}

bool ASRStructure::CanPlaceOnSurfaceCell_Implementation(const FSRPlanetSurfaceGridCellInfo& SurfaceCellInfo) const
{
	return HasValidStructureData()
		&& AppliedStructureData.bAvailableForConstruction
		&& SurfaceCellInfo.bCanConstruct
		&& !SurfaceCellInfo.bOccupied
		&& SurfaceCellInfo.FaceResolution > 0;
}

void ASRStructure::ApplyStructureVisuals()
{
	if (!StructureStaticMesh)
	{
		return;
	}

	StructureStaticMesh->SetStaticMesh(AppliedStructureData.StaticMesh);
	StructureStaticMesh->SetRelativeLocation(ResolveSurfaceSnappedMeshRelativeLocation());
	StructureStaticMesh->SetRelativeRotation(AppliedStructureData.MeshRelativeRotation);
	StructureStaticMesh->SetRelativeScale3D(AppliedStructureData.MeshRelativeScale);
	StructureStaticMesh->SetVisibility(IsValid(AppliedStructureData.StaticMesh.Get()));

	if (bStructureGhostMode && IsValid(AppliedStructureData.GhostMaterial.Get()))
	{
		const int32 MaterialSlotCount = FMath::Max(1, StructureStaticMesh->GetNumMaterials());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
		{
			StructureStaticMesh->SetMaterial(MaterialIndex, AppliedStructureData.GhostMaterial);
		}
	}
	else if (UMaterialInterface* ActiveMaterial = ResolveActiveMaterial())
	{
		StructureStaticMesh->SetMaterial(0, ActiveMaterial);
	}

	StructureStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StructureStaticMesh->SetGenerateOverlapEvents(false);
	StructureStaticMesh->SetRenderCustomDepth(true);
	SetActorEnableCollision(false);
}

UMaterialInterface* ASRStructure::ResolveActiveMaterial() const
{
	if (bStructureGhostMode && IsValid(AppliedStructureData.GhostMaterial.Get()))
	{
		return AppliedStructureData.GhostMaterial;
	}

	return AppliedStructureData.Material;
}

FVector ASRStructure::ResolveSurfaceSnappedMeshRelativeLocation() const
{
	UStaticMesh* StaticMesh = AppliedStructureData.StaticMesh.Get();
	if (!IsValid(StaticMesh))
	{
		return AppliedStructureData.MeshRelativeLocation;
	}

	const FBox MeshBounds = StaticMesh->GetBoundingBox();
	if (!MeshBounds.IsValid)
	{
		return AppliedStructureData.MeshRelativeLocation;
	}

	const FQuat MeshRelativeRotation = AppliedStructureData.MeshRelativeRotation.Quaternion();
	const FVector MeshRelativeScale = AppliedStructureData.MeshRelativeScale;
	float MinLocalZ = TNumericLimits<float>::Max();
	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector BoundsCorner(
			(CornerIndex & 1) ? MeshBounds.Max.X : MeshBounds.Min.X,
			(CornerIndex & 2) ? MeshBounds.Max.Y : MeshBounds.Min.Y,
			(CornerIndex & 4) ? MeshBounds.Max.Z : MeshBounds.Min.Z);
		const FVector TransformedCorner = MeshRelativeRotation.RotateVector(BoundsCorner * MeshRelativeScale);
		MinLocalZ = FMath::Min(MinLocalZ, TransformedCorner.Z);
	}

	if (MinLocalZ == TNumericLimits<float>::Max())
	{
		return AppliedStructureData.MeshRelativeLocation;
	}

	return AppliedStructureData.MeshRelativeLocation - FVector(0.0f, 0.0f, MinLocalZ);
}

bool ASRStructure::HasValidStructureData() const
{
	return !AppliedStructureData.StructureId.IsNone()
		&& IsValid(AppliedStructureData.StaticMesh.Get());
}
