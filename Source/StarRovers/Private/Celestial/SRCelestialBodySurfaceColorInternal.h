#pragma once

#include "CoreMinimal.h"

namespace StarRovers::Celestial::SurfaceColors
{
	inline uint64 BuildDynamicMeshColorElementKey(int32 MeshComponentIndex, int32 ElementId)
	{
		return (static_cast<uint64>(static_cast<uint32>(MeshComponentIndex)) << 32)
			| static_cast<uint64>(static_cast<uint32>(ElementId));
	}

	inline FVector4f ToDynamicMeshVectorColor(const FLinearColor& Color)
	{
		return FVector4f(Color.R, Color.G, Color.B, Color.A);
	}
}
