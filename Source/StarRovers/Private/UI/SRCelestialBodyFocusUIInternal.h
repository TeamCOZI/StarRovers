#pragma once

#include "Surface/SRPlanetSurfaceGridTypes.h"

namespace StarRovers::UI::CelestialBodyFocus
{
	inline int32 GetCubeSphereFaceNumber(const ESRCubeSphereFace Face)
	{
		return static_cast<int32>(Face) + 1;
	}
}
