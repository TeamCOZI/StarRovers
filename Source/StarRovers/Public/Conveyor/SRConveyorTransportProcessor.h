#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorNetworkRuntimeState.h"

class USRFacilityNetworkComponent;
class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct FSRConveyorTransportSettings
	{
		float ItemSpeedCellsPerSecond = 1.0f;
		int32 MaxItemTransfersPerTick = 1;
	};

	struct STARROVERS_API FSRConveyorTransportProcessor
	{
		static void Process(
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRFacilityNetworkComponent* FacilityNetwork,
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			FSRConveyorTransportRuntimeState& TransportState,
			float DeltaTime,
			const FSRConveyorTransportSettings& Settings);

	private:
		static bool HasConnectedOutputLane(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRConveyorSegment& Segment);

		static bool TryResolveNextTransferLane(
			USRPlanetSurfaceGrid* SurfaceGrid,
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRConveyorTransportRuntimeState& TransportState,
			FSRConveyorSegment& Segment,
			const TMap<FSRConveyorLaneKey, FSRConveyorItem>& NextItemsByLane,
			FSRConveyorLaneKey& OutNextLane);

		static bool CanTransferIntoMergeConveyorSegment(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRConveyorTransportRuntimeState& TransportState,
			FSRConveyorSegment& MergeSegment,
			ESRConveyorGridDirection IncomingInputDirection,
			const TMap<FSRConveyorLaneKey, FSRConveyorItem>& NextItemsByLane);

		static bool TryPullFacilityOutputToConveyor(
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRFacilityNetworkComponent* FacilityNetwork,
			const FSRConveyorLaneKey& LaneKey,
			TMap<FSRConveyorLaneKey, FSRConveyorItem>& OutNextItems);
	};
}
