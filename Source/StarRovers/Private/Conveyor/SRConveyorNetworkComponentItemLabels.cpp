#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Conveyor/SRConveyorItemLabelResolver.h"
#include "Conveyor/SRConveyorItemLabelUpdater.h"

namespace
{
	StarRovers::Conveyor::FSRConveyorItemLabelSettings MakeConveyorItemLabelSettings(
		float DefaultLayerHeight,
		float BeltSurfaceOffset,
		float ItemLabelHeightOffset,
		float ItemPatternLabelWorldSize,
		float ItemPatternLabelMaxScale,
		const FLinearColor& ItemPatternSparseColor,
		const FLinearColor& ItemPatternDenseColor,
		const FLinearColor& ItemPatternSpecialColor)
	{
		StarRovers::Conveyor::FSRConveyorItemLabelSettings Settings;
		Settings.DefaultLayerHeight = DefaultLayerHeight;
		Settings.BeltSurfaceOffset = BeltSurfaceOffset;
		Settings.ItemLabelHeightOffset = ItemLabelHeightOffset;
		Settings.ItemPatternLabelWorldSize = ItemPatternLabelWorldSize;
		Settings.ItemPatternLabelMaxScale = ItemPatternLabelMaxScale;
		Settings.ItemPatternSparseColor = ItemPatternSparseColor;
		Settings.ItemPatternDenseColor = ItemPatternDenseColor;
		Settings.ItemPatternSpecialColor = ItemPatternSpecialColor;
		return Settings;
	}
}

void USRConveyorNetworkComponent::RefreshConveyorItemLabels(USRPlanetSurfaceGrid* SurfaceGrid, float /*DeltaTime*/)
{
	const StarRovers::Conveyor::FSRConveyorItemLabelSettings LabelSettings = MakeConveyorItemLabelSettings(
		DefaultLayerHeight,
		BeltSurfaceOffset,
		ItemLabelHeightOffset,
		ItemPatternLabelWorldSize,
		ItemPatternLabelMaxScale,
		ItemPatternSparseColor,
		ItemPatternDenseColor,
		ItemPatternSpecialColor);
	StarRovers::Conveyor::FSRConveyorItemLabelUpdater::Refresh(
		GetOwner(),
		this,
		GetWorld(),
		SurfaceGrid,
		Segments,
		BeltPaths,
		LabelSettings,
		TransportState);
}

void USRConveyorNetworkComponent::DestroyConveyorItemLabels()
{
	StarRovers::Conveyor::FSRConveyorItemLabelUpdater::Destroy(TransportState);
}
