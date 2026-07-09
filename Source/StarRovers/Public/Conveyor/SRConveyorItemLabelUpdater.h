#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorItemLabelResolver.h"
#include "Conveyor/SRConveyorNetworkRuntimeState.h"

class AActor;
class USceneComponent;
class UTextRenderComponent;
class UWorld;
class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorItemLabelUpdater
	{
		static void Refresh(
			AActor* OwnerActor,
			USceneComponent* AttachParent,
			UWorld* World,
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			const FSRConveyorItemLabelSettings& LabelSettings,
			FSRConveyorTransportRuntimeState& TransportState);

		static void Destroy(FSRConveyorTransportRuntimeState& TransportState);

	private:
		static UTextRenderComponent* EnsureLabelComponent(
			AActor* OwnerActor,
			USceneComponent* AttachParent,
			const FSRConveyorLaneKey& LaneKey,
			const FSRConveyorItemLabelSettings& LabelSettings,
			TMap<FSRConveyorLaneKey, TObjectPtr<UTextRenderComponent>>& ItemLabelsByLane);
	};
}
