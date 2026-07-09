#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Conveyor/SRConveyorItemLabelResolver.h"
#include "Conveyor/SRConveyorItemLabelUpdater.h"

namespace
{
	StarRovers::Conveyor::FSRConveyorItemLabelSettings MakeConveyorItemLabelSettings(
		float DefaultLayerHeight,
		float BeltSurfaceOffset,
		float ItemLabelHeightOffset,
		float ItemEnergyLabelWorldSize,
		float ItemEnergyLabelMaxScale,
		const FLinearColor& ItemEnergyLowColor,
		const FLinearColor& ItemEnergyHighColor,
		const FLinearColor& ItemEnergyNegativeColor)
	{
		StarRovers::Conveyor::FSRConveyorItemLabelSettings Settings;
		Settings.DefaultLayerHeight = DefaultLayerHeight;
		Settings.BeltSurfaceOffset = BeltSurfaceOffset;
		Settings.ItemLabelHeightOffset = ItemLabelHeightOffset;
		Settings.ItemEnergyLabelWorldSize = ItemEnergyLabelWorldSize;
		Settings.ItemEnergyLabelMaxScale = ItemEnergyLabelMaxScale;
		Settings.ItemEnergyLowColor = ItemEnergyLowColor;
		Settings.ItemEnergyHighColor = ItemEnergyHighColor;
		Settings.ItemEnergyNegativeColor = ItemEnergyNegativeColor;
		return Settings;
	}
}

void USRConveyorNetworkComponent::RefreshConveyorItemLabels(USRPlanetSurfaceGrid* SurfaceGrid, float /*DeltaTime*/)
{
	const StarRovers::Conveyor::FSRConveyorItemLabelSettings LabelSettings = MakeConveyorItemLabelSettings(
		DefaultLayerHeight,
		BeltSurfaceOffset,
		ItemLabelHeightOffset,
		ItemEnergyLabelWorldSize,
		ItemEnergyLabelMaxScale,
		ItemEnergyLowColor,
		ItemEnergyHighColor,
		ItemEnergyNegativeColor);
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
