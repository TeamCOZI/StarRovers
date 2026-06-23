#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

namespace StarRovers::FacilityNetwork
{
	constexpr int32 HalfLifeDefaultCycles = 3;

	inline FString BuildFacilityCellDebugString(const FSRPlanetSurfaceGridCellId& CellId)
	{
		return FString::Printf(
			TEXT("Face=%d X=%d Y=%d"),
			static_cast<int32>(CellId.Face),
			CellId.CellX,
			CellId.CellY);
	}

	inline FString BuildResourceDebugString(const FSRResourceInstance& ResourceInstance)
	{
		return FString::Printf(
			TEXT("ResourceId=%s Energy=%.3f RemainingProcessLimit=%d ProcessCount=%d StackCount=%d Tags=%d StellarFuel=%s FuelMultiplier=%.3f"),
			*ResourceInstance.ResourceId.ToString(),
			ResourceInstance.EnergyValue,
			ResourceInstance.RemainingProcessLimit,
			ResourceInstance.ProcessCount,
			ResourceInstance.StackCount,
			ResourceInstance.Tags.Num(),
			ResourceInstance.bCountsAsStellarFuel ? TEXT("true") : TEXT("false"),
			ResourceInstance.StellarFuelValueMultiplier);
	}
}
