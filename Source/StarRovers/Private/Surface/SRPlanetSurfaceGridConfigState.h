#pragma once

#include "CoreMinimal.h"

namespace StarRovers::SurfaceGridConfigState
{
	void ApplyPlanetRadius(
		float NewPlanetRadius,
		float& PlanetRadius,
		bool& bCellsDirty,
		bool& bGridMeshDirty);

	void ApplyFaceResolution(
		int32 NewFaceResolution,
		int32& FaceResolution,
		bool& bCellsDirty,
		bool& bGridMeshDirty);
}
