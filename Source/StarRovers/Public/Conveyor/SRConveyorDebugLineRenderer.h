#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class ULineBatchComponent;
class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct FSRConveyorDebugLineSettings
	{
		bool bShowPathDebugLine = false;
		bool bShowConnectionDebugLine = false;
		FLinearColor PathDebugLineColor = FLinearColor::White;
		FLinearColor ConnectionDebugLineColor = FLinearColor::White;
		FLinearColor BrokenConnectionDebugLineColor = FLinearColor::Red;
		FLinearColor EndpointDebugLineColor = FLinearColor::Yellow;
		float PathDebugLineThickness = 0.0f;
		float ConnectionDebugLineThickness = 0.0f;
		float ConnectionDebugLineHeightOffset = 0.0f;
		float BeltSurfaceOffset = 0.0f;
		float ConnectionLayerHeight = 0.0f;
	};

	struct STARROVERS_API FSRConveyorDebugLineRenderer
	{
		static void Draw(
			USRPlanetSurfaceGrid* SurfaceGrid,
			ULineBatchComponent* LineBatchComponent,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRConveyorDebugLineSettings& Settings);
	};
}
