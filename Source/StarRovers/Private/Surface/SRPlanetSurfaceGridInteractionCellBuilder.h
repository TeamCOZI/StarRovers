#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace StarRovers::SurfaceGridInteractionCellBuilder
{
	using FCellLookup = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)>;
	using FSurfacePointResolver = TFunctionRef<FVector(const FVector& LocalUnitDirection, float HeightOffset)>;

	void AppendInteractionCell(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const FSRPlanetSurfaceGridCell& Cell,
		const FLinearColor& LineColor,
		float LineThickness,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		FCellLookup GetCellById,
		FSurfacePointResolver ResolveLocalSurfacePoint);
}
