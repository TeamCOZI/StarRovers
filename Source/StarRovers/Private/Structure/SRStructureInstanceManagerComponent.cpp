#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Automation/SRFacilityNetworkComponent.h"
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

	bool AreOccupantIdSetsEqual(const TSet<FName>& Left, const TSet<FName>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (const FName OccupantId : Left)
		{
			if (!Right.Contains(OccupantId))
			{
				return false;
			}
		}

		return true;
	}

	void AppendOccupantIds(TSet<FName>& OutOccupantIds, const TSet<FName>& OccupantIds)
	{
		for (const FName OccupantId : OccupantIds)
		{
			OutOccupantIds.Add(OccupantId);
		}
	}
}

USRStructureInstanceManagerComponent::USRStructureInstanceManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	NextStructureInstanceSequence = 1;
	bShowStructureNameLabels = true;
	bShowNaturalStructureNameLabels = false;
	StructureNameLabelHeightOffset = 260.0f;
	StructureNameLabelWorldSize = 120.0f;
	StructureNameLabelColor = FLinearColor(0.95f, 0.98f, 1.0f, 1.0f);
	StructureNameLabelMaxDrawDistance = 0.0f;
}

bool USRStructureInstanceManagerComponent::TryPlaceStructureOnSurfaceGrid(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	USRStructureDataAsset* StructureDataAsset,
	FName& OutOccupantId,
	bool bNaturalStructure,
	bool bUseStaticMeshMaterials,
	int32 PlacementRotationSteps)
{
	return TryPlaceStructureOnSurfaceGridInternal(
		SurfaceGrid,
		TargetCellId,
		StructureDataAsset,
		OutOccupantId,
		bNaturalStructure,
		bUseStaticMeshMaterials,
		PlacementRotationSteps,
		NAME_None);
}

bool USRStructureInstanceManagerComponent::TryPlaceStructureOnSurfaceGridInternal(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	USRStructureDataAsset* StructureDataAsset,
	FName& OutOccupantId,
	bool bNaturalStructure,
	bool bUseStaticMeshMaterials,
	int32 PlacementRotationSteps,
	FName ForcedOccupantId)
{
	OutOccupantId = NAME_None;
	if (!IsValid(SurfaceGrid)
		|| !IsValid(StructureDataAsset)
		|| (!ForcedOccupantId.IsNone() && PlacedStructuresByOccupantId.Contains(ForcedOccupantId)))
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
	const int32 NormalizedRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(PlacementRotationSteps);

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

	FTransform PlacementTransform;
	if (!USRStructurePlacementLibrary::BuildStructurePlacementTransform(
		SurfaceGrid,
		TargetCellId,
		StructureDataAsset,
		PlacementTransform,
		StarRovers::Structure::PlacementRotationStepsToYawDegrees(NormalizedRotationSteps)))
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

	const FName OccupantId = ForcedOccupantId.IsNone()
		? MakeOccupantId(TargetCellId, StructureData.StructureId, NextStructureInstanceSequence++)
		: ForcedOccupantId;
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
	PlacedStructure.PlacementRotationSteps = NormalizedRotationSteps;
	PlacedStructure.StructureDataAsset = StructureDataAsset;
	PlacedStructure.VisualKey = VisualKey;
	PlacedStructure.InstanceIndex = InstanceIndex;
	PlacedStructure.bNaturalStructure = bNaturalStructure;
	PlacedStructure.bUseStaticMeshMaterials = bUseStaticMeshMaterials;

	PlacedStructuresByOccupantId.Add(OccupantId, PlacedStructure);
	VisualGroup.OccupantIds.Add(OccupantId);
	RegisterResourceDeposit(PlacedStructure, StructureData);
	OutOccupantId = OccupantId;
	if (USRFacilityNetworkComponent* FacilityNetwork = GetOwner() ? GetOwner()->FindComponentByClass<USRFacilityNetworkComponent>() : nullptr)
	{
		FacilityNetwork->RegisterFacility(OccupantId, StructureDataAsset, TargetCellId, FootprintCellIds, NormalizedRotationSteps);
	}
	if (!bNaturalStructure)
	{
		LogStructureMemoryDiagnostics(TEXT("StructurePlace.User"), false, 1, FootprintCellIds.Num());
	}
	RefreshStructureNameLabel(SurfaceGrid, PlacedStructure);
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

	const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(CellInfo.OccupantId);
	if (!PlacedStructure || PlacedStructure->bNaturalStructure)
	{
		return false;
	}

	RemoveStructureByOccupantId(SurfaceGrid, CellInfo.OccupantId);
	return true;
}

bool USRStructureInstanceManagerComponent::TryRemoveStructureByOccupantId(
	USRPlanetSurfaceGrid* SurfaceGrid,
	FName OccupantId,
	FSRPlacedStructureInstance& OutRemovedStructure)
{
	OutRemovedStructure = FSRPlacedStructureInstance();
	if (!IsValid(SurfaceGrid) || OccupantId.IsNone())
	{
		return false;
	}

	const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId);
	if (!PlacedStructure || PlacedStructure->bNaturalStructure)
	{
		return false;
	}

	OutRemovedStructure = *PlacedStructure;
	RemoveStructureByOccupantId(SurfaceGrid, OccupantId);
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

void USRStructureInstanceManagerComponent::GetPlacedStructures(TArray<FSRPlacedStructureInstance>& OutPlacedStructures) const
{
	OutPlacedStructures.Reset();
	OutPlacedStructures.Reserve(PlacedStructuresByOccupantId.Num());
	for (const TPair<FName, FSRPlacedStructureInstance>& PlacedStructurePair : PlacedStructuresByOccupantId)
	{
		OutPlacedStructures.Add(PlacedStructurePair.Value);
	}
}

bool USRStructureInstanceManagerComponent::CanDestroyNaturalStructureForConstruction(FName OccupantId) const
{
	if (OccupantId.IsNone())
	{
		return false;
	}

	const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId);
	if (!PlacedStructure || !PlacedStructure->bNaturalStructure || !IsValid(PlacedStructure->StructureDataAsset.Get()))
	{
		return false;
	}

	return PlacedStructure->StructureDataAsset->BuildData().bDestroyableByConstruction;
}

bool USRStructureInstanceManagerComponent::CanDestroyStructureForConstruction(FName OccupantId) const
{
	if (OccupantId.IsNone())
	{
		return false;
	}

	const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId);
	if (!PlacedStructure || !IsValid(PlacedStructure->StructureDataAsset.Get()))
	{
		return false;
	}

	return PlacedStructure->StructureDataAsset->BuildData().bDestroyableByConstruction;
}

bool USRStructureInstanceManagerComponent::CanBuildOverCellsForConstruction(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	TSet<FName>& OutDestructibleOccupantIds) const
{
	OutDestructibleOccupantIds.Reset();
	if (!IsValid(SurfaceGrid) || CellIds.IsEmpty())
	{
		return false;
	}

	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			return false;
		}

		if (!CellInfo.bOccupied)
		{
			if (!CellInfo.bCanConstruct)
			{
				return false;
			}
			continue;
		}

		if (CellInfo.OccupantId.IsNone() || !CanDestroyStructureForConstruction(CellInfo.OccupantId))
		{
			return false;
		}

		OutDestructibleOccupantIds.Add(CellInfo.OccupantId);
	}

	return true;
}

void USRStructureInstanceManagerComponent::SetGhostedStructures(const TSet<FName>& OccupantIds)
{
	TSet<FName> NewGhostedStructureOccupantIds;
	NewGhostedStructureOccupantIds.Reserve(OccupantIds.Num());
	for (const FName OccupantId : OccupantIds)
	{
		if (PlacedStructuresByOccupantId.Contains(OccupantId))
		{
			NewGhostedStructureOccupantIds.Add(OccupantId);
		}
	}

	if (AreOccupantIdSetsEqual(GhostedStructureOccupantIds, NewGhostedStructureOccupantIds))
	{
		return;
	}

	TSet<FName> AffectedOccupantIds;
	AppendOccupantIds(AffectedOccupantIds, GhostedStructureOccupantIds);
	AppendOccupantIds(AffectedOccupantIds, NewGhostedStructureOccupantIds);
	GhostedStructureOccupantIds = MoveTemp(NewGhostedStructureOccupantIds);
	RefreshVisualInstancesForOccupants(AffectedOccupantIds);
}

void USRStructureInstanceManagerComponent::ClearGhostedStructures()
{
	if (GhostedStructureOccupantIds.IsEmpty())
	{
		return;
	}

	TSet<FName> AffectedOccupantIds;
	AppendOccupantIds(AffectedOccupantIds, GhostedStructureOccupantIds);
	GhostedStructureOccupantIds.Reset();
	RefreshVisualInstancesForOccupants(AffectedOccupantIds);
}

void USRStructureInstanceManagerComponent::SetDeletePreviewedStructures(const TSet<FName>& OccupantIds)
{
	TSet<FName> NewDeletePreviewedStructureOccupantIds;
	NewDeletePreviewedStructureOccupantIds.Reserve(OccupantIds.Num());
	for (const FName OccupantId : OccupantIds)
	{
		if (IsDeletePreviewTarget(OccupantId))
		{
			NewDeletePreviewedStructureOccupantIds.Add(OccupantId);
		}
	}

	if (AreOccupantIdSetsEqual(DeletePreviewedStructureOccupantIds, NewDeletePreviewedStructureOccupantIds)
		&& ConstructionReplacementPreviewedStructureOccupantIds.IsEmpty())
	{
		return;
	}

	TSet<FName> AffectedOccupantIds;
	AppendOccupantIds(AffectedOccupantIds, DeletePreviewedStructureOccupantIds);
	AppendOccupantIds(AffectedOccupantIds, NewDeletePreviewedStructureOccupantIds);
	AppendOccupantIds(AffectedOccupantIds, ConstructionReplacementPreviewedStructureOccupantIds);
	DeletePreviewedStructureOccupantIds = MoveTemp(NewDeletePreviewedStructureOccupantIds);
	ConstructionReplacementPreviewedStructureOccupantIds.Reset();
	RefreshVisualInstancesForOccupants(AffectedOccupantIds);
}

void USRStructureInstanceManagerComponent::SetConstructionReplacementPreviewedStructures(const TSet<FName>& OccupantIds)
{
	TSet<FName> NewConstructionReplacementPreviewedStructureOccupantIds;
	NewConstructionReplacementPreviewedStructureOccupantIds.Reserve(OccupantIds.Num());
	for (const FName OccupantId : OccupantIds)
	{
		if (CanDestroyStructureForConstruction(OccupantId))
		{
			NewConstructionReplacementPreviewedStructureOccupantIds.Add(OccupantId);
		}
	}

	if (AreOccupantIdSetsEqual(ConstructionReplacementPreviewedStructureOccupantIds, NewConstructionReplacementPreviewedStructureOccupantIds)
		&& DeletePreviewedStructureOccupantIds.IsEmpty())
	{
		return;
	}

	TSet<FName> AffectedOccupantIds;
	AppendOccupantIds(AffectedOccupantIds, ConstructionReplacementPreviewedStructureOccupantIds);
	AppendOccupantIds(AffectedOccupantIds, NewConstructionReplacementPreviewedStructureOccupantIds);
	AppendOccupantIds(AffectedOccupantIds, DeletePreviewedStructureOccupantIds);
	DeletePreviewedStructureOccupantIds.Reset();
	ConstructionReplacementPreviewedStructureOccupantIds = MoveTemp(NewConstructionReplacementPreviewedStructureOccupantIds);
	RefreshVisualInstancesForOccupants(AffectedOccupantIds);
}

void USRStructureInstanceManagerComponent::ClearDeletePreviewedStructures()
{
	if (DeletePreviewedStructureOccupantIds.IsEmpty()
		&& ConstructionReplacementPreviewedStructureOccupantIds.IsEmpty())
	{
		return;
	}

	TSet<FName> AffectedOccupantIds;
	AppendOccupantIds(AffectedOccupantIds, DeletePreviewedStructureOccupantIds);
	AppendOccupantIds(AffectedOccupantIds, ConstructionReplacementPreviewedStructureOccupantIds);
	DeletePreviewedStructureOccupantIds.Reset();
	ConstructionReplacementPreviewedStructureOccupantIds.Reset();
	RefreshVisualInstancesForOccupants(AffectedOccupantIds);
}

bool USRStructureInstanceManagerComponent::RemoveNonResourceStructuresByOccupantIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TSet<FName>& OccupantIds)
{
	if (!IsValid(SurfaceGrid) || OccupantIds.IsEmpty())
	{
		return false;
	}

	TArray<FName> OccupantIdsToRemove;
	OccupantIdsToRemove.Reserve(OccupantIds.Num());
	for (const FName OccupantId : OccupantIds)
	{
		if (IsDeletePreviewTarget(OccupantId))
		{
			OccupantIdsToRemove.Add(OccupantId);
		}
	}

	if (OccupantIdsToRemove.IsEmpty())
	{
		return false;
	}

	RemoveStructuresByOccupantIds(SurfaceGrid, OccupantIdsToRemove);
	return true;
}

bool USRStructureInstanceManagerComponent::RemoveConstructionDestructibleStructuresByOccupantIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TSet<FName>& OccupantIds,
	TArray<FSRPlacedStructureInstance>* OutRemovedStructures)
{
	if (OutRemovedStructures)
	{
		OutRemovedStructures->Reset();
	}
	if (!IsValid(SurfaceGrid) || OccupantIds.IsEmpty())
	{
		return false;
	}

	TArray<FName> OccupantIdsToRemove;
	OccupantIdsToRemove.Reserve(OccupantIds.Num());
	if (OutRemovedStructures)
	{
		OutRemovedStructures->Reserve(OccupantIds.Num());
	}

	for (const FName OccupantId : OccupantIds)
	{
		if (!CanDestroyStructureForConstruction(OccupantId))
		{
			continue;
		}

		OccupantIdsToRemove.Add(OccupantId);
		if (OutRemovedStructures)
		{
			FSRPlacedStructureInstance PlacedStructure;
			if (GetPlacedStructure(OccupantId, PlacedStructure))
			{
				OutRemovedStructures->Add(PlacedStructure);
			}
		}
	}

	if (OccupantIdsToRemove.IsEmpty())
	{
		return false;
	}

	RemoveStructuresByOccupantIds(SurfaceGrid, OccupantIdsToRemove);
	return true;
}

bool USRStructureInstanceManagerComponent::TryRemoveConstructionDestructibleNaturalStructuresAtCells(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	TArray<FName> OccupantIdsToRemove;
	TSet<FName> OccupantIdSet;
	OccupantIdSet.Reserve(CellIds.Num());
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			return false;
		}

		if (!CellInfo.bOccupied || CellInfo.OccupantId.IsNone())
		{
			continue;
		}

		if (!CanDestroyNaturalStructureForConstruction(CellInfo.OccupantId))
		{
			return false;
		}

		if (!OccupantIdSet.Contains(CellInfo.OccupantId))
		{
			OccupantIdSet.Add(CellInfo.OccupantId);
			OccupantIdsToRemove.Add(CellInfo.OccupantId);
		}
	}

	if (OccupantIdsToRemove.IsEmpty())
	{
		return true;
	}

	RemoveStructuresByOccupantIds(SurfaceGrid, OccupantIdsToRemove);
	return true;
}

bool USRStructureInstanceManagerComponent::TryRemoveConstructionDestructibleStructuresAtCells(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	TArray<FSRPlacedStructureInstance>* OutRemovedStructures)
{
	if (OutRemovedStructures)
	{
		OutRemovedStructures->Reset();
	}
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	TSet<FName> OccupantIdsToRemove;
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			return false;
		}

		if (!CellInfo.bOccupied || CellInfo.OccupantId.IsNone())
		{
			continue;
		}

		if (!CanDestroyStructureForConstruction(CellInfo.OccupantId))
		{
			return false;
		}

		OccupantIdsToRemove.Add(CellInfo.OccupantId);
	}

	if (OccupantIdsToRemove.IsEmpty())
	{
		return true;
	}

	return RemoveConstructionDestructibleStructuresByOccupantIds(
		SurfaceGrid,
		OccupantIdsToRemove,
		OutRemovedStructures);
}

FName USRStructureInstanceManagerComponent::MakeVisualKey(USRStructureDataAsset* StructureDataAsset, bool bUseStaticMeshMaterials)
{
	return MakeVisualKey(StructureDataAsset, bUseStaticMeshMaterials, ESRStructureVisualOverride::None);
}

FName USRStructureInstanceManagerComponent::MakeVisualKey(
	USRStructureDataAsset* StructureDataAsset,
	bool bUseStaticMeshMaterials,
	ESRStructureVisualOverride VisualOverride)
{
	if (!IsValid(StructureDataAsset))
	{
		return NAME_None;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	const FString MeshPath = IsValid(StructureData.StaticMesh.Get()) ? StructureData.StaticMesh->GetPathName() : FString(TEXT("None"));
	FString MaterialPath;
	if (VisualOverride == ESRStructureVisualOverride::Ghost && IsValid(StructureData.GhostMaterial.Get()))
	{
		MaterialPath = StructureData.GhostMaterial->GetPathName();
	}
	else if (VisualOverride == ESRStructureVisualOverride::Delete && IsValid(StructureData.DeleteMaterial.Get()))
	{
		MaterialPath = StructureData.DeleteMaterial->GetPathName();
	}
	else if (bUseStaticMeshMaterials)
	{
		MaterialPath = FString(TEXT("StaticMeshMaterials"));
	}
	else
	{
		MaterialPath = IsValid(StructureData.Material.Get()) ? StructureData.Material->GetPathName() : FString(TEXT("None"));
	}

	return FName(*FString::Printf(TEXT("%s|%s|Override_%d"), *MeshPath, *MaterialPath, static_cast<int32>(VisualOverride)));
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
	bool bUseStaticMeshMaterials,
	ESRStructureVisualOverride VisualOverride)
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
	HISMComponent->SetRenderCustomDepth(true);
	HISMComponent->SetStaticMesh(StructureData.StaticMesh);
	if (VisualOverride == ESRStructureVisualOverride::Delete)
	{
		HISMComponent->SetVisibility(false, true);
		HISMComponent->SetHiddenInGame(true);
		HISMComponent->SetCastShadow(false);
	}
	UMaterialInterface* OverrideMaterial = nullptr;
	if (VisualOverride == ESRStructureVisualOverride::Ghost)
	{
		OverrideMaterial = StructureData.GhostMaterial.Get();
	}
	else if (VisualOverride == ESRStructureVisualOverride::Delete)
	{
		OverrideMaterial = StructureData.DeleteMaterial.Get();
	}

	if (IsValid(OverrideMaterial))
	{
		const int32 MaterialSlotCount = FMath::Max(1, HISMComponent->GetNumMaterials());
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
		{
			HISMComponent->SetMaterial(MaterialIndex, OverrideMaterial);
		}
	}
	else if (!bUseStaticMeshMaterials && IsValid(StructureData.Material.Get()))
	{
		HISMComponent->SetMaterial(0, StructureData.Material);
	}
	HISMComponent->ComponentTags.AddUnique(TEXT("StarRovers.StructureInstances"));
	OwnerActor->AddInstanceComponent(HISMComponent);
	HISMComponent->RegisterComponent();

	VisualGroup.Component = HISMComponent;
	return VisualGroup;
}

bool USRStructureInstanceManagerComponent::IsDeletePreviewTarget(FName OccupantId) const
{
	if (OccupantId.IsNone())
	{
		return false;
	}

	const FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId);
	if (!PlacedStructure || !IsValid(PlacedStructure->StructureDataAsset.Get()))
	{
		return false;
	}

	return !PlacedStructure->StructureDataAsset->BuildData().bIsResourceDeposit;
}

USRStructureInstanceManagerComponent::ESRStructureVisualOverride USRStructureInstanceManagerComponent::GetVisualOverrideForOccupant(FName OccupantId) const
{
	if (DeletePreviewedStructureOccupantIds.Contains(OccupantId))
	{
		return ESRStructureVisualOverride::Delete;
	}

	if (ConstructionReplacementPreviewedStructureOccupantIds.Contains(OccupantId)
		|| GhostedStructureOccupantIds.Contains(OccupantId))
	{
		return ESRStructureVisualOverride::Ghost;
	}

	return ESRStructureVisualOverride::None;
}

bool USRStructureInstanceManagerComponent::BuildPlacedStructureInstanceWorldTransform(
	const FSRPlacedStructureInstance& PlacedStructure,
	FTransform& OutInstanceWorldTransform) const
{
	if (!IsValid(PlacedStructure.StructureDataAsset))
	{
		return false;
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
			PlacedStructure.OriginCellId,
			PlacedStructure.StructureDataAsset,
			PlacementTransform,
			StarRovers::Structure::PlacementRotationStepsToYawDegrees(PlacedStructure.PlacementRotationSteps)))
	{
		return false;
	}

	const FSRStructureData StructureData = PlacedStructure.StructureDataAsset->BuildData();
	OutInstanceWorldTransform = BuildInstanceWorldTransform(PlacementTransform, StructureData);
	return true;
}

void USRStructureInstanceManagerComponent::RemoveStructureByOccupantId(USRPlanetSurfaceGrid* SurfaceGrid, FName OccupantId)
{
	RemoveStructuresByOccupantIds(SurfaceGrid, TConstArrayView<FName>(&OccupantId, 1));
}

void USRStructureInstanceManagerComponent::RemoveStructuresByOccupantIds(USRPlanetSurfaceGrid* SurfaceGrid, TConstArrayView<FName> OccupantIds)
{
	if (OccupantIds.IsEmpty())
	{
		return;
	}

	TArray<FSRPlanetSurfaceGridCellId> ClearedCellIds;
	TMap<FName, TArray<FName>> RemovedOccupantIdsByVisualKey;
	int32 RemovedUserStructureCount = 0;
	int32 RemovedNaturalStructureCount = 0;
	for (const FName OccupantId : OccupantIds)
	{
		FSRPlacedStructureInstance RemovedStructure;
		if (!PlacedStructuresByOccupantId.RemoveAndCopyValue(OccupantId, RemovedStructure))
		{
			continue;
		}

		DestroyStructureNameLabel(OccupantId);
		GhostedStructureOccupantIds.Remove(OccupantId);
		DeletePreviewedStructureOccupantIds.Remove(OccupantId);
		ConstructionReplacementPreviewedStructureOccupantIds.Remove(OccupantId);
		ResourceDepositsByOccupantId.Remove(OccupantId);
		ClearedCellIds.Append(RemovedStructure.FootprintCellIds);
		RemovedOccupantIdsByVisualKey.FindOrAdd(RemovedStructure.VisualKey).Add(OccupantId);
		if (USRFacilityNetworkComponent* FacilityNetwork = GetOwner() ? GetOwner()->FindComponentByClass<USRFacilityNetworkComponent>() : nullptr)
		{
			FacilityNetwork->UnregisterFacility(OccupantId);
		}
		if (RemovedStructure.bNaturalStructure)
		{
			++RemovedNaturalStructureCount;
		}
		else
		{
			++RemovedUserStructureCount;
		}
	}

	if (IsValid(SurfaceGrid) && !ClearedCellIds.IsEmpty())
	{
		SurfaceGrid->BeginInteractionHighlightBatch();
		SurfaceGrid->SetCellsOccupied(ClearedCellIds, false, NAME_None);
		SurfaceGrid->EndInteractionHighlightBatch();
	}

	for (const TPair<FName, TArray<FName>>& RemovedGroupPair : RemovedOccupantIdsByVisualKey)
	{
		RemoveVisualInstances(RemovedGroupPair.Key, RemovedGroupPair.Value);
	}

	const bool bForceGC = CVarSRMemoryDiagnosticsForceGCOnStructureDelete.GetValueOnAnyThread() != 0;
	if (RemovedUserStructureCount > 0)
	{
		LogStructureMemoryDiagnostics(TEXT("StructureDelete.User"), bForceGC, RemovedUserStructureCount, ClearedCellIds.Num());
	}
	if (RemovedNaturalStructureCount > 0)
	{
		LogStructureMemoryDiagnostics(TEXT("StructureDelete.Natural"), bForceGC, RemovedNaturalStructureCount, ClearedCellIds.Num());
	}
}

void USRStructureInstanceManagerComponent::RemoveVisualInstances(FName VisualKey, const TArray<FName>& RemovedOccupantIds)
{
	FSRStructureVisualGroup* VisualGroup = VisualGroupsByKey.Find(VisualKey);
	if (!VisualGroup || !IsValid(VisualGroup->Component) || RemovedOccupantIds.IsEmpty())
	{
		return;
	}

	TSet<FName> RemovedOccupantIdSet;
	RemovedOccupantIdSet.Reserve(RemovedOccupantIds.Num());
	for (const FName RemovedOccupantId : RemovedOccupantIds)
	{
		RemovedOccupantIdSet.Add(RemovedOccupantId);
	}

	TArray<int32> InstanceIndicesToRemove;
	InstanceIndicesToRemove.Reserve(RemovedOccupantIds.Num());
	for (int32 InstanceIndex = 0; InstanceIndex < VisualGroup->OccupantIds.Num(); ++InstanceIndex)
	{
		const FName OccupantId = VisualGroup->OccupantIds[InstanceIndex];
		if (OccupantId.IsNone()
			|| RemovedOccupantIdSet.Contains(OccupantId)
			|| !PlacedStructuresByOccupantId.Contains(OccupantId))
		{
			InstanceIndicesToRemove.Add(InstanceIndex);
		}
	}

	RemoveVisualInstanceIndices(VisualKey, MoveTemp(InstanceIndicesToRemove));
}

bool USRStructureInstanceManagerComponent::RemoveVisualInstanceIndices(FName VisualKey, TArray<int32> InstanceIndicesToRemove)
{
	FSRStructureVisualGroup* VisualGroup = VisualGroupsByKey.Find(VisualKey);
	if (!VisualGroup || !IsValid(VisualGroup->Component) || InstanceIndicesToRemove.IsEmpty())
	{
		return false;
	}

	InstanceIndicesToRemove.Sort(TGreater<int32>());

	TArray<int32> UniqueValidInstanceIndices;
	UniqueValidInstanceIndices.Reserve(InstanceIndicesToRemove.Num());
	int32 LastAddedInstanceIndex = INDEX_NONE;
	for (const int32 InstanceIndex : InstanceIndicesToRemove)
	{
		if (InstanceIndex == LastAddedInstanceIndex)
		{
			continue;
		}

		if (!VisualGroup->OccupantIds.IsValidIndex(InstanceIndex))
		{
			continue;
		}

		UniqueValidInstanceIndices.Add(InstanceIndex);
		LastAddedInstanceIndex = InstanceIndex;
	}

	if (UniqueValidInstanceIndices.IsEmpty())
	{
		return false;
	}

	if (UniqueValidInstanceIndices.Num() == VisualGroup->OccupantIds.Num())
	{
		VisualGroup->Component->ClearInstances();
		VisualGroup->OccupantIds.Reset();
		return true;
	}

	if (!VisualGroup->Component->RemoveInstances(UniqueValidInstanceIndices, true))
	{
		RebuildVisualGroup(VisualKey);
		return false;
	}

	for (const int32 RemovedInstanceIndex : UniqueValidInstanceIndices)
	{
		const int32 LastInstanceIndex = VisualGroup->OccupantIds.Num() - 1;
		if (!VisualGroup->OccupantIds.IsValidIndex(RemovedInstanceIndex))
		{
			continue;
		}

		const FName MovedOccupantId = RemovedInstanceIndex != LastInstanceIndex
			? VisualGroup->OccupantIds[LastInstanceIndex]
			: NAME_None;
		VisualGroup->OccupantIds.RemoveAtSwap(RemovedInstanceIndex, 1, EAllowShrinking::No);

		if (!MovedOccupantId.IsNone())
		{
			if (FSRPlacedStructureInstance* MovedStructure = PlacedStructuresByOccupantId.Find(MovedOccupantId))
			{
				if (MovedStructure->VisualKey == VisualKey)
				{
					MovedStructure->InstanceIndex = RemovedInstanceIndex;
				}
			}
		}
	}

	return true;
}

void USRStructureInstanceManagerComponent::MoveVisualInstanceToOverride(FName OccupantId, ESRStructureVisualOverride VisualOverride)
{
	FSRPlacedStructureInstance* PlacedStructure = PlacedStructuresByOccupantId.Find(OccupantId);
	if (!PlacedStructure || !IsValid(PlacedStructure->StructureDataAsset))
	{
		return;
	}

	const FName CurrentVisualKey = PlacedStructure->VisualKey;
	const FName DesiredVisualKey = MakeVisualKey(
		PlacedStructure->StructureDataAsset,
		PlacedStructure->bUseStaticMeshMaterials,
		VisualOverride);
	if (CurrentVisualKey == DesiredVisualKey)
	{
		return;
	}

	FSRStructureVisualGroup* CurrentVisualGroup = VisualGroupsByKey.Find(CurrentVisualKey);
	if (!CurrentVisualGroup || !IsValid(CurrentVisualGroup->Component))
	{
		return;
	}

	int32 CurrentInstanceIndex = PlacedStructure->InstanceIndex;
	if (!CurrentVisualGroup->OccupantIds.IsValidIndex(CurrentInstanceIndex)
		|| CurrentVisualGroup->OccupantIds[CurrentInstanceIndex] != OccupantId)
	{
		CurrentInstanceIndex = CurrentVisualGroup->OccupantIds.IndexOfByKey(OccupantId);
		if (CurrentInstanceIndex == INDEX_NONE)
		{
			RebuildVisualGroup(CurrentVisualKey);
			return;
		}

		PlacedStructure->InstanceIndex = CurrentInstanceIndex;
	}

	FTransform InstanceWorldTransform;
	if (!BuildPlacedStructureInstanceWorldTransform(*PlacedStructure, InstanceWorldTransform))
	{
		if (!CurrentVisualGroup->Component->GetInstanceTransform(CurrentInstanceIndex, InstanceWorldTransform, true))
		{
			return;
		}
	}

	FSRStructureVisualGroup& DesiredVisualGroup = FindOrCreateVisualGroup(
		PlacedStructure->StructureDataAsset,
		DesiredVisualKey,
		PlacedStructure->bUseStaticMeshMaterials,
		VisualOverride);
	if (!IsValid(DesiredVisualGroup.Component))
	{
		return;
	}

	TArray<int32> SourceInstanceIndicesToRemove;
	SourceInstanceIndicesToRemove.Add(CurrentInstanceIndex);
	if (!RemoveVisualInstanceIndices(CurrentVisualKey, MoveTemp(SourceInstanceIndicesToRemove)))
	{
		return;
	}

	const int32 DesiredInstanceIndex = DesiredVisualGroup.Component->AddInstance(InstanceWorldTransform, true);
	if (DesiredInstanceIndex == INDEX_NONE)
	{
		return;
	}

	DesiredVisualGroup.OccupantIds.Add(OccupantId);
	PlacedStructure->VisualKey = DesiredVisualKey;
	PlacedStructure->InstanceIndex = DesiredInstanceIndex;
}

void USRStructureInstanceManagerComponent::RefreshVisualInstancesForOccupants(const TSet<FName>& OccupantIds)
{
	for (const FName OccupantId : OccupantIds)
	{
		if (!PlacedStructuresByOccupantId.Contains(OccupantId))
		{
			continue;
		}

		MoveVisualInstanceToOverride(OccupantId, GetVisualOverrideForOccupant(OccupantId));
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
		if (!PlacedStructure || PlacedStructure->VisualKey != VisualKey)
		{
			continue;
		}

		FTransform InstanceWorldTransform;
		if (!BuildPlacedStructureInstanceWorldTransform(*PlacedStructure, InstanceWorldTransform))
		{
			continue;
		}

		PlacedStructure->InstanceIndex = VisualGroup->Component->AddInstance(InstanceWorldTransform, true);
		if (PlacedStructure->InstanceIndex != INDEX_NONE)
		{
			RebuiltOccupantIds.Add(OccupantId);
		}
	}
	VisualGroup->OccupantIds = MoveTemp(RebuiltOccupantIds);
}

void USRStructureInstanceManagerComponent::LogStructureMemoryDiagnostics(const TCHAR* Label, bool bRequestGarbageCollection, int32 AffectedStructures, int32 AffectedCells) const
{
	if (CVarSRMemoryDiagnosticsStructureMutation.GetValueOnAnyThread() == 0)
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
