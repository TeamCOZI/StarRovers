#include "Automation/SRFacilityNetworkComponent.h"

#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool GetNeighborCellIdByStructurePortDirection(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRStructurePortDirection Direction,
		FSRPlanetSurfaceGridCellId& OutNeighborCellId)
	{
		OutNeighborCellId = FSRPlanetSurfaceGridCellId();
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(CellId, Neighbors))
		{
			return false;
		}

		switch (Direction)
		{
		case ESRStructurePortDirection::Left:
			OutNeighborCellId = Neighbors.NegativeU;
			break;
		case ESRStructurePortDirection::Right:
			OutNeighborCellId = Neighbors.PositiveU;
			break;
		case ESRStructurePortDirection::Top:
			OutNeighborCellId = Neighbors.NegativeV;
			break;
		case ESRStructurePortDirection::Bottom:
			OutNeighborCellId = Neighbors.PositiveV;
			break;
		default:
			return false;
		}

		return true;
	}

	bool GetStructurePortFootprintCellId(
		const FSRFacilityInstance& FacilityInstance,
		const FSRStructureData& StructureData,
		const FSRStructurePortSpec& PortSpec,
		FSRPlanetSurfaceGridCellId& OutCellId)
	{
		OutCellId = FSRPlanetSurfaceGridCellId();
		if (FacilityInstance.FootprintCellIds.IsEmpty())
		{
			return false;
		}

		const int32 FootprintCellsX = FMath::Max(1, StructureData.FootprintCellsX);
		const int32 FootprintCellsY = FMath::Max(1, StructureData.FootprintCellsY);
		if (PortSpec.CellOffsetX < 0
			|| PortSpec.CellOffsetY < 0
			|| PortSpec.CellOffsetX >= FootprintCellsX
			|| PortSpec.CellOffsetY >= FootprintCellsY)
		{
			return false;
		}

		const int32 FootprintIndex = PortSpec.CellOffsetY * FootprintCellsX + PortSpec.CellOffsetX;
		if (!FacilityInstance.FootprintCellIds.IsValidIndex(FootprintIndex))
		{
			return false;
		}

		OutCellId = FacilityInstance.FootprintCellIds[FootprintIndex];
		return true;
	}

	bool IsConveyorCellConnectedToStructurePorts(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRStructureData& StructureData,
		const TArray<FSRStructurePortSpec>& Ports,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId)
	{
		for (const FSRStructurePortSpec& PortSpec : Ports)
		{
			FSRPlanetSurfaceGridCellId PortCellId;
			if (!GetStructurePortFootprintCellId(FacilityInstance, StructureData, PortSpec, PortCellId))
			{
				continue;
			}

			FSRPlanetSurfaceGridCellId NeighborCellId;
			if (GetNeighborCellIdByStructurePortDirection(SurfaceGrid, PortCellId, PortSpec.Direction, NeighborCellId)
				&& NeighborCellId == ConveyorCellId)
			{
				return true;
			}
		}

		return false;
	}
}

bool USRFacilityNetworkComponent::TryAcceptInputResourceFromConveyorCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	const FSRResourceInstance& ResourceInstance,
	FName SourceFacilityOccupantId)
{
	if (!IsValid(SurfaceGrid) || ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
	{
		return false;
	}

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset)
			|| (!SourceFacilityOccupantId.IsNone() && FacilityInstance.OccupantId == SourceFacilityOccupantId)
			|| FacilityInstance.InputInventory.Num() >= FMath::Max(1, FacilityDataAsset->InputCapacity)
			|| !IsConveyorCellConnectedToFacilityPort(SurfaceGrid, FacilityInstance, ConveyorCellId, ESRFacilityPortKind::Input))
		{
			continue;
		}

		FacilityInstance.InputInventory.Add(ResourceInstance);
		SetComponentTickEnabled(bAutoProcessFacilities);
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[FacilityNetwork] Transfer accepted input: OccupantId=%s ResourceId=%s InputCount=%d Owner=%s"),
				*FacilityInstance.OccupantId.ToString(),
				*ResourceInstance.ResourceId.ToString(),
				FacilityInstance.InputInventory.Num(),
				*GetNameSafe(GetOwner()));
		}
		return true;
	}

	return false;
}

bool USRFacilityNetworkComponent::TryPullOutputResourceToConveyorCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	FSRResourceInstance& OutResourceInstance,
	FName& OutSourceFacilityOccupantId)
{
	if (!IsValid(SurfaceGrid))
	{
		OutResourceInstance = FSRResourceInstance();
		OutSourceFacilityOccupantId = NAME_None;
		return false;
	}

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		if (FacilityInstance.OutputInventory.IsEmpty()
			|| !IsConveyorCellConnectedToFacilityPort(SurfaceGrid, FacilityInstance, ConveyorCellId, ESRFacilityPortKind::Output))
		{
			continue;
		}

		OutResourceInstance = FacilityInstance.OutputInventory[0];
		OutSourceFacilityOccupantId = FacilityInstance.OccupantId;
		FacilityInstance.OutputInventory.RemoveAt(0);
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[FacilityNetwork] Transfer provided output: OccupantId=%s ResourceId=%s RemainingOutput=%d Owner=%s"),
				*FacilityInstance.OccupantId.ToString(),
				*OutResourceInstance.ResourceId.ToString(),
				FacilityInstance.OutputInventory.Num(),
				*GetNameSafe(GetOwner()));
		}
		return true;
	}

	OutResourceInstance = FSRResourceInstance();
	OutSourceFacilityOccupantId = NAME_None;
	return false;
}

bool USRFacilityNetworkComponent::IsConveyorCellConnectedToFacilityPort(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRFacilityInstance& FacilityInstance,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	ESRFacilityPortKind PortKind) const
{
	if (IsValid(FacilityInstance.StructureDataAsset.Get()))
	{
		const FSRStructureData StructureData = FacilityInstance.StructureDataAsset->BuildData();
		const bool bHasStructurePortLayout = !StructureData.InputPorts.IsEmpty() || !StructureData.OutputPorts.IsEmpty();
		if (bHasStructurePortLayout)
		{
			const TArray<FSRStructurePortSpec>& Ports = PortKind == ESRFacilityPortKind::Output
				? StructureData.OutputPorts
				: StructureData.InputPorts;
			return IsConveyorCellConnectedToStructurePorts(SurfaceGrid, FacilityInstance, StructureData, Ports, ConveyorCellId);
		}
	}

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return false;
	}

	bool bHasExplicitPortForKind = false;
	for (const FSRFacilityPortSpec& PortSpec : FacilityDataAsset->Ports)
	{
		if (PortSpec.PortKind != PortKind)
		{
			continue;
		}

		bHasExplicitPortForKind = true;
		if (IsConveyorCellConnectedToExplicitPort(SurfaceGrid, FacilityInstance, PortSpec, ConveyorCellId))
		{
			return true;
		}
	}

	return !bHasExplicitPortForKind
		&& IsConveyorCellAdjacentToFacilityFootprint(SurfaceGrid, FacilityInstance, ConveyorCellId);
}

bool USRFacilityNetworkComponent::IsConveyorCellConnectedToExplicitPort(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRFacilityInstance& FacilityInstance,
	const FSRFacilityPortSpec& PortSpec,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId) const
{
	FSRPlanetSurfaceGridCellId PortCellId;
	if (!GetFootprintCellIdByOffset(FacilityInstance, PortSpec.FootprintCellX, PortSpec.FootprintCellY, PortCellId))
	{
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> NeighborCellIds;
	if (!GetNeighborCellIdByFacilityPortDirection(SurfaceGrid, PortCellId, PortSpec.Direction, NeighborCellIds))
	{
		return false;
	}

	return NeighborCellIds.Contains(ConveyorCellId);
}

bool USRFacilityNetworkComponent::IsConveyorCellAdjacentToFacilityFootprint(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRFacilityInstance& FacilityInstance,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId) const
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	for (const FSRPlanetSurfaceGridCellId& FootprintCellId : FacilityInstance.FootprintCellIds)
	{
		TArray<FSRPlanetSurfaceGridCellId> NeighborCellIds;
		if (GetNeighborCellIdByFacilityPortDirection(SurfaceGrid, FootprintCellId, ESRFacilityPortDirection::Any, NeighborCellIds)
			&& NeighborCellIds.Contains(ConveyorCellId))
		{
			return true;
		}
	}

	return false;
}

bool USRFacilityNetworkComponent::GetFootprintCellIdByOffset(
	const FSRFacilityInstance& FacilityInstance,
	int32 FootprintCellX,
	int32 FootprintCellY,
	FSRPlanetSurfaceGridCellId& OutCellId) const
{
	OutCellId = FSRPlanetSurfaceGridCellId();
	if (FacilityInstance.FootprintCellIds.IsEmpty())
	{
		return false;
	}

	int32 FootprintCellsX = 1;
	if (IsValid(FacilityInstance.StructureDataAsset.Get()))
	{
		FootprintCellsX = FMath::Max(1, FacilityInstance.StructureDataAsset->BuildData().FootprintCellsX);
	}

	const int32 SafeX = FMath::Max(0, FootprintCellX);
	const int32 SafeY = FMath::Max(0, FootprintCellY);
	const int32 FootprintIndex = SafeY * FootprintCellsX + SafeX;
	if (FacilityInstance.FootprintCellIds.IsValidIndex(FootprintIndex))
	{
		OutCellId = FacilityInstance.FootprintCellIds[FootprintIndex];
		return true;
	}

	OutCellId = FacilityInstance.OriginCellId;
	return true;
}

bool USRFacilityNetworkComponent::GetNeighborCellIdByFacilityPortDirection(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	ESRFacilityPortDirection Direction,
	TArray<FSRPlanetSurfaceGridCellId>& OutNeighborCellIds) const
{
	OutNeighborCellIds.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	if (!SurfaceGrid->GetCellNeighbors(CellId, Neighbors))
	{
		return false;
	}

	switch (Direction)
	{
	case ESRFacilityPortDirection::NegativeU:
		OutNeighborCellIds.Add(Neighbors.NegativeU);
		break;
	case ESRFacilityPortDirection::PositiveU:
		OutNeighborCellIds.Add(Neighbors.PositiveU);
		break;
	case ESRFacilityPortDirection::NegativeV:
		OutNeighborCellIds.Add(Neighbors.NegativeV);
		break;
	case ESRFacilityPortDirection::PositiveV:
		OutNeighborCellIds.Add(Neighbors.PositiveV);
		break;
	case ESRFacilityPortDirection::Any:
	default:
		OutNeighborCellIds.Add(Neighbors.NegativeU);
		OutNeighborCellIds.Add(Neighbors.PositiveU);
		OutNeighborCellIds.Add(Neighbors.NegativeV);
		OutNeighborCellIds.Add(Neighbors.PositiveV);
		break;
	}

	return !OutNeighborCellIds.IsEmpty();
}
