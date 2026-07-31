#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct FSRConveyorItemLabelSettings
	{
		float DefaultLayerHeight = 0.0f;
		float BeltSurfaceOffset = 0.0f;
		float ItemLabelHeightOffset = 0.0f;
		float ItemPatternLabelWorldSize = 1.0f;
		float ItemPatternLabelMaxScale = 1.0f;
		FLinearColor ItemPatternSparseColor = FLinearColor::White;
		FLinearColor ItemPatternDenseColor = FLinearColor::White;
		FLinearColor ItemPatternSpecialColor = FLinearColor::White;
	};

	struct STARROVERS_API FSRConveyorItemLabelResolver
	{
		static bool ResolveWorldLocation(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			const FSRConveyorItem& Item,
			const FSRConveyorItemLabelSettings& Settings,
			FVector& OutWorldLocation,
			FVector& OutWorldNormal);

		static FText BuildLabelText(const FSRResourceInstance& ResourceInstance);
		static FColor ResolveLabelColor(const FSRResourceInstance& ResourceInstance, const FSRConveyorItemLabelSettings& Settings);
		static float ResolveLabelWorldSize(
			const FSRConveyorLaneKey& LaneKey,
			const FSRResourceInstance& ResourceInstance,
			float TimeSeconds,
			const FSRConveyorItemLabelSettings& Settings);
	};
}
