#include "Assembly/SRAssemblyPlacementQueue.h"

#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	void FSRAssemblyPlacementQueue::ConfigurePerformance(
		int32 NewMaxStructurePlacementsPerFrame,
		int32 NewMaxQueuedStructurePlacements)
	{
		MaxStructurePlacementsPerFrame = FMath::Max(1, NewMaxStructurePlacementsPerFrame);
		MaxQueuedStructurePlacements = FMath::Max(1, NewMaxQueuedStructurePlacements);
	}

	void FSRAssemblyPlacementQueue::Reset()
	{
		Queue.Reset();
	}

	bool FSRAssemblyPlacementQueue::IsEmpty() const
	{
		return Queue.IsEmpty();
	}

	void FSRAssemblyPlacementQueue::Enqueue(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 PlacementRotationSteps)
	{
		if (!IsValid(SurfaceGrid))
		{
			return;
		}

		for (const FSRQueuedStructurePlacement& PendingPlacement : Queue)
		{
			if (PendingPlacement.SurfaceGrid.Get() == SurfaceGrid && PendingPlacement.CellId == CellId)
			{
				return;
			}
		}

		const int32 MaxQueueSize = FMath::Max(1, MaxQueuedStructurePlacements);
		if (Queue.Num() >= MaxQueueSize)
		{
			Queue.RemoveAt(0, Queue.Num() - MaxQueueSize + 1, EAllowShrinking::No);
		}

		FSRQueuedStructurePlacement QueuedPlacement;
		QueuedPlacement.SurfaceGrid = SurfaceGrid;
		QueuedPlacement.CellId = CellId;
		QueuedPlacement.PlacementRotationSteps = PlacementRotationSteps;
		Queue.Add(QueuedPlacement);
	}

	void FSRAssemblyPlacementQueue::PopNextFrame(TArray<FSRQueuedStructurePlacement>& OutPlacements)
	{
		OutPlacements.Reset();
		if (Queue.IsEmpty())
		{
			return;
		}

		const int32 PlacementBudget = FMath::Max(1, MaxStructurePlacementsPerFrame);
		const int32 PlacementCount = FMath::Min(PlacementBudget, Queue.Num());
		OutPlacements.Reserve(PlacementCount);
		for (int32 PlacementIndex = 0; PlacementIndex < PlacementCount; ++PlacementIndex)
		{
			OutPlacements.Add(Queue[0]);
			Queue.RemoveAt(0, 1, EAllowShrinking::No);
		}
	}
}
