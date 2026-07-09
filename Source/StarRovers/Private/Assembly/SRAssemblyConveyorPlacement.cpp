#include "Assembly/SRAssemblyConveyorPlacement.h"

#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	FName FSRAssemblyConveyorPlacement::MakeNetworkId(AActor* FocusedActor, int32 Layer)
	{
		return FName(*FString::Printf(TEXT("Conveyor_%s_%d"), *GetNameSafe(FocusedActor), FMath::Max(0, Layer)));
	}

	bool FSRAssemblyConveyorPlacement::TryPlacePath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		USRStructureDataAsset* ConveyorDataAsset,
		const FSRStructureData& ConveyorData,
		const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
		FName NetworkId,
		const FSRAssemblyPlacementHistory& PlacementHistory,
		FSRAssemblyConveyorPlacementResult& OutResult)
	{
		OutResult = FSRAssemblyConveyorPlacementResult();
		if (!IsValid(SurfaceGrid)
			|| !IsValid(ConveyorNetwork)
			|| !IsValid(ConveyorDataAsset)
			|| ConveyorData.BuildKind != ESRStructureBuildKind::Conveyor
			|| PathCellIds.IsEmpty())
		{
			return false;
		}

		PlacementHistory.BuildConveyorPlacementPayload(
			SurfaceGrid,
			ConveyorNetwork,
			ConveyorDataAsset,
			PathCellIds,
			ConveyorData.ConveyorLayer,
			ConveyorData.ConveyorLayerHeight,
			NetworkId,
			OutResult.HistoryBeltPath,
			OutResult.HistoryPlacedCellIds,
			OutResult.HistoryRemovedNaturalStructures);

		return ConveyorNetwork->TryPlaceConveyorPath(
			SurfaceGrid,
			PathCellIds,
			ConveyorData.ConveyorLayer,
			ConveyorData.ConveyorLayerHeight,
			ConveyorDataAsset,
			NetworkId);
	}
}
