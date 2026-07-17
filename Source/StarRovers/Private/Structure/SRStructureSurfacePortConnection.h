#pragma once

#include "CoreMinimal.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Surface/SRPlanetSurfaceGridInteractionCoordinateMapping.h"

namespace StarRovers::Structure::SurfacePorts
{
	inline bool ShouldInvertDisplayPortOffsetsForFace(ESRCubeSphereFace Face)
	{
		return Face == ESRCubeSphereFace::PositiveZ || Face == ESRCubeSphereFace::NegativeZ;
	}

	inline bool TryGetPortFootprintCellId(
		const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds,
		int32 FootprintCellsX,
		int32 FootprintCellsY,
		const FSRStructurePortSpec& PortSpec,
		FSRPlanetSurfaceGridCellId& OutFootprintCellId)
	{
		OutFootprintCellId = FSRPlanetSurfaceGridCellId();
		if (FootprintCellIds.IsEmpty())
		{
			return false;
		}

		const int32 SafeFootprintCellsX = FMath::Max(1, FootprintCellsX);
		const int32 SafeFootprintCellsY = FMath::Max(1, FootprintCellsY);
		if (PortSpec.CellOffsetX < 0
			|| PortSpec.CellOffsetY < 0
			|| PortSpec.CellOffsetX >= SafeFootprintCellsX
			|| PortSpec.CellOffsetY >= SafeFootprintCellsY)
		{
			return false;
		}

		int32 FootprintCellX = PortSpec.CellOffsetX;
		int32 FootprintCellY = PortSpec.CellOffsetY;
		if (ShouldInvertDisplayPortOffsetsForFace(FootprintCellIds[0].Face))
		{
			FootprintCellX = SafeFootprintCellsX - 1 - FootprintCellX;
			FootprintCellY = SafeFootprintCellsY - 1 - FootprintCellY;
		}

		const int32 FootprintIndex = FootprintCellY * SafeFootprintCellsX + FootprintCellX;
		if (!FootprintCellIds.IsValidIndex(FootprintIndex))
		{
			return false;
		}

		OutFootprintCellId = FootprintCellIds[FootprintIndex];
		return true;
	}

	inline bool TryResolvePortDirectionStep(ESRStructurePortDirection Direction, FIntPoint& OutStep)
	{
		switch (Direction)
		{
		case ESRStructurePortDirection::Left:
			OutStep = FIntPoint(-1, 0);
			return true;
		case ESRStructurePortDirection::Right:
			OutStep = FIntPoint(1, 0);
			return true;
		case ESRStructurePortDirection::Top:
			OutStep = FIntPoint(0, -1);
			return true;
		case ESRStructurePortDirection::Bottom:
			OutStep = FIntPoint(0, 1);
			return true;
		default:
			OutStep = FIntPoint::ZeroValue;
			return false;
		}
	}

	inline bool TryGetPortConnectionCellId(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& PortCellId,
		ESRStructurePortDirection Direction,
		FSRPlanetSurfaceGridCellId& OutConnectionCellId)
	{
		OutConnectionCellId = FSRPlanetSurfaceGridCellId();
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		const int32 FaceResolution = SurfaceGrid->GetFaceResolution();
		if (!PortCellId.IsValid(FaceResolution))
		{
			return false;
		}

		FIntPoint DirectionStep = FIntPoint::ZeroValue;
		if (!TryResolvePortDirectionStep(Direction, DirectionStep))
		{
			return false;
		}

		const StarRovers::Surface::Interaction::FSRPlanetSurfaceGridDisplayMapper DisplayMapper(FaceResolution);
		const StarRovers::Surface::Interaction::FSRPlanetSurfaceGridDisplayCoord PortDisplayCoord =
			DisplayMapper.CanonicalToDisplay(PortCellId);
		StarRovers::Surface::Interaction::FSRPlanetSurfaceGridDisplayCoord ConnectionDisplayCoord;
		bool bCrossedEdge = false;
		if (!DisplayMapper.TryStepDisplayCoord(PortDisplayCoord, DirectionStep, ConnectionDisplayCoord, bCrossedEdge))
		{
			return false;
		}

		OutConnectionCellId = DisplayMapper.DisplayToCanonical(ConnectionDisplayCoord);
		FSRPlanetSurfaceGridCell ConnectionCell;
		return SurfaceGrid->GetCellById(OutConnectionCellId, ConnectionCell);
	}
}
