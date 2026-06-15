#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Utility/SRMemoryDiagnostics.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSRMemoryDiagnosticsStructureMutation(
		TEXT("sr.MemoryDiagnostics.StructureMutation"),
		0,
		TEXT("Logs memory diagnostics after user structure placement and structure deletion. 0=disabled, 1=enabled."));

	TAutoConsoleVariable<int32> CVarSRMemoryDiagnosticsForceGCOnStructureDelete(
		TEXT("sr.MemoryDiagnostics.ForceGCOnStructureDelete"),
		0,
		TEXT("Requests garbage collection after structure deletion diagnostics. 0=log next tick only, 1=force GC and log AfterGCTick."));
}

USRStructureInstanceManagerComponent::USRStructureInstanceManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	NextStructureInstanceSequence = 1;
}

bool USRStructureInstanceManagerComponent::TryPlaceStructureOnSurfaceGrid(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	USRStructureDataAsset* StructureDataAsset,
	FName& OutOccupantId,
	bool bNaturalStructure,
	bool bUseStaticMeshMaterials)
{
	OutOccupantId = NAME_None;
	if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (StructureData.BuildKind != ESRStructureBuildKind::Structure
		|| StructureData.StructureId.IsNone()
		|| !IsValid(StructureData.StaticMesh.Get()))
	{
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	if (!SurfaceGrid->GetFootprintCellIds(TargetCellId, StructureData.FootprintCellsX, StructureData.FootprintCellsY, FootprintCellIds)
		|| !SurfaceGrid->CanOccupyCells(FootprintCellIds))
	{
		return false;
	}

	FTransform PlacementTransform;
	if (!USRStructurePlacementLibrary::BuildStructurePlacementTransform(SurfaceGrid, TargetCellId, StructureDataAsset, PlacementTransform))
	{
		return false;
	}

	const FName VisualKey = MakeVisualKey(StructureDataAsset, bUseStaticMeshMaterials);
	FSRStructureVisualGroup& VisualGroup = FindOrCreateVisualGroup(StructureDataAsset, VisualKey, bUseStaticMeshMaterials);
	if (!IsValid(VisualGroup.Component))
	{
		return false;
	}

	const FTransform InstanceWorldTransform = BuildInstanceWorldTransform(PlacementTransform, StructureData);
	const int32 InstanceIndex = VisualGroup.Component->AddInstance(InstanceWorldTransform, true);
	if (InstanceIndex == INDEX_NONE)
	{
		return false;
	}

	const FName OccupantId = MakeOccupantId(TargetCellId, StructureData.StructureId, NextStructureInstanceSequence++);
	if (!SurfaceGrid->SetCellsOccupied(FootprintCellIds, true, OccupantId))
	{
		VisualGroup.Component->RemoveInstance(InstanceIndex);
		return false;
	}

	FSRPlacedStructureInstance PlacedStructure;
	PlacedStructure.OccupantId = OccupantId;
	PlacedStructure.StructureId = StructureData.StructureId;
	PlacedStructure.OriginCellId = TargetCellId;
	PlacedStructure.FootprintCellIds = FootprintCellIds;
	PlacedStructure.StructureDataAsset = StructureDataAsset;
	PlacedStructure.VisualKey = VisualKey;
	PlacedStructure.InstanceIndex = InstanceIndex;
	PlacedStructure.bNaturalStructure = bNaturalStructure;

	PlacedStructuresByOccupantId.Add(OccupantId, PlacedStructure);
	VisualGroup.OccupantIds.Add(OccupantId);
	OutOccupantId = OccupantId;
	if (!bNaturalStructure)
	{
		LogStructureMemoryDiagnostics(TEXT("StructurePlace.User"), false, 1, FootprintCellIds.Num());
	}
	return true;
}

bool USRStructureInstanceManagerComponent::TryRemoveStructureAtCell(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo CellInfo;
	if (!SurfaceGrid->GetCellInfoById(TargetCellId, CellInfo) || !CellInfo.bOccupied || CellInfo.OccupantId.IsNone())
	{
		return false;
	}

	if (!PlacedStructuresByOccupantId.Contains(CellInfo.OccupantId))
	{
		return false;
	}

	RemoveStructureByOccupantId(SurfaceGrid, CellInfo.OccupantId);
	return true;
}

void USRStructureInstanceManagerComponent::ClearNaturalStructures(USRPlanetSurfaceGrid* SurfaceGrid)
{
	TArray<FName> NaturalOccupantIds;
	for (const TPair<FName, FSRPlacedStructureInstance>& PlacedStructurePair : PlacedStructuresByOccupantId)
	{
		if (PlacedStructurePair.Value.bNaturalStructure)
		{
			NaturalOccupantIds.Add(PlacedStructurePair.Key);
		}
	}

	RemoveStructuresByOccupantIds(SurfaceGrid, NaturalOccupantIds);
}

void USRStructureInstanceManagerComponent::ClearAllStructures(USRPlanetSurfaceGrid* SurfaceGrid)
{
	TArray<FName> OccupantIds;
	PlacedStructuresByOccupantId.GetKeys(OccupantIds);
	RemoveStructuresByOccupantIds(SurfaceGrid, OccupantIds);
}

bool USRStructureInstanceManagerComponent::GetPlacedStructure(FName OccupantId, FSRPlacedStructureInstance& OutPlacedStructure) const
{
	if (const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId))
	{
		OutPlacedStructure = *PlacedStructure;
		return true;
	}

	OutPlacedStructure = FSRPlacedStructureInstance();
	return false;
}

FName USRStructureInstanceManagerComponent::MakeVisualKey(USRStructureDataAsset* StructureDataAsset, bool bUseStaticMeshMaterials)
{
	if (!IsValid(StructureDataAsset))
	{
		return NAME_None;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	const FString MeshPath = IsValid(StructureData.StaticMesh.Get()) ? StructureData.StaticMesh->GetPathName() : FString(TEXT("None"));
	const FString MaterialPath = bUseStaticMeshMaterials
		? FString(TEXT("StaticMeshMaterials"))
		: (IsValid(StructureData.Material.Get()) ? StructureData.Material->GetPathName() : FString(TEXT("None")));
	return FName(*FString::Printf(TEXT("%s|%s"), *MeshPath, *MaterialPath));
}

FName USRStructureInstanceManagerComponent::MakeOccupantId(const FSRPlanetSurfaceGridCellId& CellId, FName StructureId, int32 SequenceNumber)
{
	return FName(*FString::Printf(
		TEXT("Structure_%s_%d_%d_%d_%d"),
		*StructureId.ToString(),
		static_cast<int32>(CellId.Face),
		CellId.CellX,
		CellId.CellY,
		SequenceNumber));
}

FTransform USRStructureInstanceManagerComponent::BuildInstanceWorldTransform(const FTransform& PlacementTransform, const FSRStructureData& StructureData)
{
	FVector MeshRelativeLocation = StructureData.MeshRelativeLocation;
	if (UStaticMesh* StaticMesh = StructureData.StaticMesh.Get())
	{
		const FBox MeshBounds = StaticMesh->GetBoundingBox();
		if (MeshBounds.IsValid)
		{
			const FQuat MeshRelativeRotation = StructureData.MeshRelativeRotation.Quaternion();
			float MinLocalZ = TNumericLimits<float>::Max();
			for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
			{
				const FVector BoundsCorner(
					(CornerIndex & 1) ? MeshBounds.Max.X : MeshBounds.Min.X,
					(CornerIndex & 2) ? MeshBounds.Max.Y : MeshBounds.Min.Y,
					(CornerIndex & 4) ? MeshBounds.Max.Z : MeshBounds.Min.Z);
				const FVector TransformedCorner = MeshRelativeRotation.RotateVector(BoundsCorner * StructureData.MeshRelativeScale);
				MinLocalZ = FMath::Min(MinLocalZ, TransformedCorner.Z);
			}

			if (MinLocalZ != TNumericLimits<float>::Max())
			{
				MeshRelativeLocation -= FVector(0.0f, 0.0f, MinLocalZ);
			}
		}
	}

	const FTransform MeshRelativeTransform(
		StructureData.MeshRelativeRotation,
		MeshRelativeLocation,
		StructureData.MeshRelativeScale);
	return MeshRelativeTransform * PlacementTransform;
}

USRStructureInstanceManagerComponent::FSRStructureVisualGroup& USRStructureInstanceManagerComponent::FindOrCreateVisualGroup(
	USRStructureDataAsset* StructureDataAsset,
	FName VisualKey,
	bool bUseStaticMeshMaterials)
{
	FSRStructureVisualGroup& VisualGroup = VisualGroupsByKey.FindOrAdd(VisualKey);
	if (IsValid(VisualGroup.Component) || !IsValid(StructureDataAsset))
	{
		return VisualGroup;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return VisualGroup;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (!IsValid(StructureData.StaticMesh.Get()))
	{
		return VisualGroup;
	}

	const FName ComponentName = MakeUniqueObjectName(OwnerActor, UHierarchicalInstancedStaticMeshComponent::StaticClass(), FName(TEXT("StructureInstances")));
	UHierarchicalInstancedStaticMeshComponent* HISMComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(OwnerActor, ComponentName);
	if (!IsValid(HISMComponent))
	{
		return VisualGroup;
	}

	HISMComponent->SetupAttachment(this);
	HISMComponent->SetMobility(EComponentMobility::Movable);
	HISMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HISMComponent->SetGenerateOverlapEvents(false);
	HISMComponent->SetCastShadow(true);
	HISMComponent->SetStaticMesh(StructureData.StaticMesh);
	if (!bUseStaticMeshMaterials && IsValid(StructureData.Material.Get()))
	{
		HISMComponent->SetMaterial(0, StructureData.Material);
	}
	HISMComponent->ComponentTags.AddUnique(TEXT("StarRovers.StructureInstances"));
	OwnerActor->AddInstanceComponent(HISMComponent);
	HISMComponent->RegisterComponent();

	VisualGroup.Component = HISMComponent;
	return VisualGroup;
}

void USRStructureInstanceManagerComponent::RemoveStructureByOccupantId(USRPlanetSurfaceGrid* SurfaceGrid, FName OccupantId)
{
	TArray<FName> OccupantIds;
	OccupantIds.Add(OccupantId);
	RemoveStructuresByOccupantIds(SurfaceGrid, OccupantIds);
}

void USRStructureInstanceManagerComponent::RemoveStructuresByOccupantIds(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FName>& OccupantIds)
{
	if (OccupantIds.IsEmpty())
	{
		return;
	}

	TArray<FSRPlanetSurfaceGridCellId> ClearedCellIds;
	TMap<FName, TArray<int32>> RemovedInstanceIndicesByVisualKey;
	int32 RemovedUserStructureCount = 0;
	int32 RemovedNaturalStructureCount = 0;
	for (const FName OccupantId : OccupantIds)
	{
		FSRPlacedStructureInstance RemovedStructure;
		if (!PlacedStructuresByOccupantId.RemoveAndCopyValue(OccupantId, RemovedStructure))
		{
			continue;
		}

		ClearedCellIds.Append(RemovedStructure.FootprintCellIds);
		RemovedInstanceIndicesByVisualKey.FindOrAdd(RemovedStructure.VisualKey).Add(RemovedStructure.InstanceIndex);
		if (RemovedStructure.bNaturalStructure)
		{
			++RemovedNaturalStructureCount;
		}
		else
		{
			++RemovedUserStructureCount;
		}
	}

	if (RemovedInstanceIndicesByVisualKey.IsEmpty())
	{
		return;
	}

	if (IsValid(SurfaceGrid) && !ClearedCellIds.IsEmpty())
	{
		SurfaceGrid->BeginInteractionHighlightBatch();
		SurfaceGrid->SetCellsOccupied(ClearedCellIds, false, NAME_None);
		SurfaceGrid->EndInteractionHighlightBatch();
	}

	for (const TPair<FName, TArray<int32>>& RemovedGroupPair : RemovedInstanceIndicesByVisualKey)
	{
		RemoveVisualInstances(RemovedGroupPair.Key, RemovedGroupPair.Value);
	}

	const bool bForceGC = CVarSRMemoryDiagnosticsForceGCOnStructureDelete.GetValueOnGameThread() != 0;
	if (RemovedUserStructureCount > 0)
	{
		LogStructureMemoryDiagnostics(TEXT("StructureDelete.User"), bForceGC, RemovedUserStructureCount, ClearedCellIds.Num());
	}
	if (RemovedNaturalStructureCount > 0)
	{
		LogStructureMemoryDiagnostics(TEXT("StructureDelete.Natural"), bForceGC, RemovedNaturalStructureCount, ClearedCellIds.Num());
	}
}

void USRStructureInstanceManagerComponent::RemoveVisualInstances(FName VisualKey, const TArray<int32>& RemovedInstanceIndices)
{
	FSRStructureVisualGroup* VisualGroup = VisualGroupsByKey.Find(VisualKey);
	if (!VisualGroup || !IsValid(VisualGroup->Component) || RemovedInstanceIndices.IsEmpty())
	{
		return;
	}

	if (RemovedInstanceIndices.Num() >= VisualGroup->OccupantIds.Num())
	{
		VisualGroup->Component->ClearInstances();
		VisualGroup->OccupantIds.Reset();
		return;
	}

	TArray<int32> SortedRemovedInstanceIndices = RemovedInstanceIndices;
	SortedRemovedInstanceIndices.Sort([](int32 LeftIndex, int32 RightIndex)
	{
		return LeftIndex > RightIndex;
	});
	for (const int32 RemovedInstanceIndex : SortedRemovedInstanceIndices)
	{
		if (!VisualGroup->OccupantIds.IsValidIndex(RemovedInstanceIndex)
			|| !VisualGroup->Component->RemoveInstance(RemovedInstanceIndex))
		{
			RebuildVisualGroup(VisualKey);
			return;
		}

		VisualGroup->OccupantIds.RemoveAt(RemovedInstanceIndex, 1, EAllowShrinking::No);
		for (int32 InstanceIndex = RemovedInstanceIndex; InstanceIndex < VisualGroup->OccupantIds.Num(); ++InstanceIndex)
		{
			if (FSRPlacedStructureInstance* ShiftedStructure = PlacedStructuresByOccupantId.Find(VisualGroup->OccupantIds[InstanceIndex]))
			{
				ShiftedStructure->InstanceIndex = InstanceIndex;
			}
		}
	}
}

void USRStructureInstanceManagerComponent::RebuildVisualGroup(FName VisualKey)
{
	FSRStructureVisualGroup* VisualGroup = VisualGroupsByKey.Find(VisualKey);
	if (!VisualGroup || !IsValid(VisualGroup->Component))
	{
		return;
	}

	VisualGroup->Component->ClearInstances();
	TArray<FName> RebuiltOccupantIds;
	RebuiltOccupantIds.Reserve(VisualGroup->OccupantIds.Num());
	for (int32 OccupantIndex = 0; OccupantIndex < VisualGroup->OccupantIds.Num(); ++OccupantIndex)
	{
		const FName OccupantId = VisualGroup->OccupantIds[OccupantIndex];
		FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId);
		if (!PlacedStructure || !IsValid(PlacedStructure->StructureDataAsset))
		{
			continue;
		}

		USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
		if (AActor* OwnerActor = GetOwner())
		{
			SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
		}

		FTransform PlacementTransform;
		if (!IsValid(SurfaceGrid)
			|| !USRStructurePlacementLibrary::BuildStructurePlacementTransform(
				SurfaceGrid,
				PlacedStructure->OriginCellId,
				PlacedStructure->StructureDataAsset,
				PlacementTransform))
		{
			continue;
		}

		const FSRStructureData StructureData = PlacedStructure->StructureDataAsset->BuildData();
		PlacedStructure->InstanceIndex = VisualGroup->Component->AddInstance(BuildInstanceWorldTransform(PlacementTransform, StructureData), true);
		if (PlacedStructure->InstanceIndex != INDEX_NONE)
		{
			RebuiltOccupantIds.Add(OccupantId);
		}
	}
	VisualGroup->OccupantIds = MoveTemp(RebuiltOccupantIds);
}

void USRStructureInstanceManagerComponent::LogStructureMemoryDiagnostics(const TCHAR* Label, bool bRequestGarbageCollection, int32 AffectedStructures, int32 AffectedCells) const
{
	if (CVarSRMemoryDiagnosticsStructureMutation.GetValueOnGameThread() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	int32 VisualComponentCount = 0;
	int32 ValidVisualComponentCount = 0;
	int32 TotalVisualInstances = 0;
	for (const TPair<FName, FSRStructureVisualGroup>& VisualGroupPair : VisualGroupsByKey)
	{
		++VisualComponentCount;
		if (IsValid(VisualGroupPair.Value.Component))
		{
			++ValidVisualComponentCount;
			TotalVisualInstances += VisualGroupPair.Value.Component->GetInstanceCount();
		}
	}

	int32 UserStructureCount = 0;
	int32 NaturalStructureCount = 0;
	for (const TPair<FName, FSRPlacedStructureInstance>& PlacedStructurePair : PlacedStructuresByOccupantId)
	{
		if (PlacedStructurePair.Value.bNaturalStructure)
		{
			++NaturalStructureCount;
		}
		else
		{
			++UserStructureCount;
		}
	}

	TArray<FString> ExtraLines;
	ExtraLines.Add(FString::Printf(
		TEXT("StructureInstanceManager Owner=%s AffectedStructures=%d AffectedCells=%d Placed=%d User=%d Natural=%d VisualGroups=%d VisualComponents=%d ValidVisualComponents=%d TotalVisualInstances=%d ForceGC=%s"),
		*GetNameSafe(GetOwner()),
		AffectedStructures,
		AffectedCells,
		PlacedStructuresByOccupantId.Num(),
		UserStructureCount,
		NaturalStructureCount,
		VisualGroupsByKey.Num(),
		VisualComponentCount,
		ValidVisualComponentCount,
		TotalVisualInstances,
		bRequestGarbageCollection ? TEXT("true") : TEXT("false")));

	if (bRequestGarbageCollection)
	{
		FSRMemoryDiagnostics::RequestGarbageCollectionAndLogNextTick(World, Label, ExtraLines);
		return;
	}

	FSRMemoryDiagnostics::LogSnapshot(World, FString::Printf(TEXT("%s.AfterRefresh"), Label), ExtraLines);
	FSRMemoryDiagnostics::LogSnapshotNextTick(World, FString::Printf(TEXT("%s.AfterTick"), Label), ExtraLines);
}
