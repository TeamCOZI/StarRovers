#include "SRPlanetSurfaceGridConfigState.h"

namespace StarRovers::SurfaceGridConfigState
{
	void ApplyPlanetRadius(
		float NewPlanetRadius,
		float& PlanetRadius,
		bool& bCellsDirty,
		bool& bGridMeshDirty)
	{
		PlanetRadius = FMath::Max(1.0f, NewPlanetRadius);
		bCellsDirty = true;
		bGridMeshDirty = true;
	}

	void ApplyFaceResolution(
		int32 NewFaceResolution,
		int32& FaceResolution,
		bool& bCellsDirty,
		bool& bGridMeshDirty)
	{
		FaceResolution = FMath::Max(1, NewFaceResolution);
		bCellsDirty = true;
		bGridMeshDirty = true;
	}
}
