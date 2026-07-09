#include "Assembly/SRAssemblyConveyorDragPathBuilder.h"

#include "Algo/Reverse.h"
#include "Assembly/SRAssemblyPlacementDragState.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Conveyor/SRConveyorTypes.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructureSurfacePortConnection.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	namespace
	{
		constexpr int32 MaxConveyorPlacementDragSegmentExtentCells = 30;

		enum class ESRConveyorPlacementEndpointRole : uint8
		{
			None,
			Source,
			Sink,
			Ambiguous,
		};

		void AppendConveyorPathSegment(
			const TArray<FSRPlanetSurfaceGridCellId>& SegmentCellIds,
			TArray<FSRPlanetSurfaceGridCellId>& OutPathCellIds)
		{
			for (const FSRPlanetSurfaceGridCellId& CellId : SegmentCellIds)
			{
				if (!OutPathCellIds.IsEmpty() && OutPathCellIds.Last() == CellId)
				{
					continue;
				}

				OutPathCellIds.Add(CellId);
			}
		}

		ESRConveyorPlacementEndpointRole CombineEndpointRole(
			ESRConveyorPlacementEndpointRole CurrentRole,
			ESRConveyorPlacementEndpointRole IncomingRole)
		{
			if (IncomingRole == ESRConveyorPlacementEndpointRole::None)
			{
				return CurrentRole;
			}
			if (CurrentRole == ESRConveyorPlacementEndpointRole::None)
			{
				return IncomingRole;
			}
			if (CurrentRole == IncomingRole)
			{
				return CurrentRole;
			}

			return ESRConveyorPlacementEndpointRole::Ambiguous;
		}

		bool DoesPortConnectToConveyorCell(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlacedStructureInstance& PlacedStructure,
			const FSRStructureData& StructureData,
			const FSRStructurePortSpec& PortSpec,
			const FSRPlanetSurfaceGridCellId& ConveyorCellId)
		{
			if (!IsValid(SurfaceGrid) || PlacedStructure.FootprintCellIds.IsEmpty())
			{
				return false;
			}

			const int32 FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacedStructure.PlacementRotationSteps);
			const int32 FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacedStructure.PlacementRotationSteps);
			if (PortSpec.CellOffsetX < 0
				|| PortSpec.CellOffsetY < 0
				|| PortSpec.CellOffsetX >= FootprintCellsX
				|| PortSpec.CellOffsetY >= FootprintCellsY)
			{
				return false;
			}

			const int32 FootprintIndex = PortSpec.CellOffsetY * FootprintCellsX + PortSpec.CellOffsetX;
			if (!PlacedStructure.FootprintCellIds.IsValidIndex(FootprintIndex))
			{
				return false;
			}

			FSRPlanetSurfaceGridCellId ConnectionCellId;
			if (!StarRovers::Structure::SurfacePorts::TryGetPortConnectionCellId(
				SurfaceGrid,
				PlacedStructure.FootprintCellIds[FootprintIndex],
				PortSpec.Direction,
				ConnectionCellId))
			{
				return false;
			}

			return ConnectionCellId == ConveyorCellId && !PlacedStructure.FootprintCellIds.Contains(ConnectionCellId);
		}

		ESRConveyorPlacementEndpointRole ResolveFacilityEndpointRole(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& CellId)
		{
			AActor* SurfaceOwner = IsValid(SurfaceGrid) ? SurfaceGrid->GetOwner() : nullptr;
			USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
				? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
				: nullptr;
			if (!IsValid(StructureInstanceManager))
			{
				return ESRConveyorPlacementEndpointRole::None;
			}

			TArray<FSRPlacedStructureInstance> PlacedStructures;
			StructureInstanceManager->GetPlacedStructures(PlacedStructures);

			ESRConveyorPlacementEndpointRole ResolvedRole = ESRConveyorPlacementEndpointRole::None;
			for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
			{
				USRStructureDataAsset* StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
				if (PlacedStructure.bNaturalStructure
					|| !IsValid(StructureDataAsset)
					|| PlacedStructure.FootprintCellIds.IsEmpty())
				{
					continue;
				}

				const FSRStructureData StructureData = StructureDataAsset->BuildData();
				if (StructureData.BuildKind != ESRStructureBuildKind::Structure)
				{
					continue;
				}

				for (const FSRStructurePortSpec& OutputPort : StructureData.OutputPorts)
				{
					const FSRStructurePortSpec RotatedOutputPort = StarRovers::Structure::RotateStructurePortSpec(
						OutputPort,
						StructureData,
						PlacedStructure.PlacementRotationSteps);
					if (DoesPortConnectToConveyorCell(SurfaceGrid, PlacedStructure, StructureData, RotatedOutputPort, CellId))
					{
						ResolvedRole = CombineEndpointRole(ResolvedRole, ESRConveyorPlacementEndpointRole::Source);
					}
				}

				for (const FSRStructurePortSpec& InputPort : StructureData.InputPorts)
				{
					const FSRStructurePortSpec RotatedInputPort = StarRovers::Structure::RotateStructurePortSpec(
						InputPort,
						StructureData,
						PlacedStructure.PlacementRotationSteps);
					if (DoesPortConnectToConveyorCell(SurfaceGrid, PlacedStructure, StructureData, RotatedInputPort, CellId))
					{
						ResolvedRole = CombineEndpointRole(ResolvedRole, ESRConveyorPlacementEndpointRole::Sink);
					}
				}
			}

			return ResolvedRole;
		}

		ESRConveyorPlacementEndpointRole ResolveExistingConveyorEndpointRole(
			const USRConveyorNetworkComponent* ConveyorNetwork,
			const FSRPlanetSurfaceGridCellId& CellId,
			int32 Layer)
		{
			if (!IsValid(ConveyorNetwork))
			{
				return ESRConveyorPlacementEndpointRole::None;
			}

			FSRConveyorLaneKey LaneKey;
			LaneKey.CellId = CellId;
			LaneKey.Layer = FMath::Max(0, Layer);

			FSRConveyorSegment Segment;
			if (!ConveyorNetwork->GetConveyorSegment(LaneKey, Segment))
			{
				return ESRConveyorPlacementEndpointRole::None;
			}

			TArray<ESRConveyorGridDirection> InputDirections;
			if (Segment.InputDirection != ESRConveyorGridDirection::None)
			{
				InputDirections.Add(Segment.InputDirection);
			}
			if (Segment.MergeInputDirection != ESRConveyorGridDirection::None
				&& Segment.MergeInputDirection != Segment.InputDirection)
			{
				InputDirections.Add(Segment.MergeInputDirection);
			}
			if (Segment.SecondMergeInputDirection != ESRConveyorGridDirection::None
				&& !InputDirections.Contains(Segment.SecondMergeInputDirection))
			{
				InputDirections.Add(Segment.SecondMergeInputDirection);
			}

			TArray<ESRConveyorGridDirection> OutputDirections;
			if (Segment.OutputDirection != ESRConveyorGridDirection::None)
			{
				OutputDirections.Add(Segment.OutputDirection);
			}
			if (Segment.BranchOutputDirection != ESRConveyorGridDirection::None
				&& Segment.BranchOutputDirection != Segment.OutputDirection)
			{
				OutputDirections.Add(Segment.BranchOutputDirection);
			}
			if (Segment.SecondBranchOutputDirection != ESRConveyorGridDirection::None
				&& !OutputDirections.Contains(Segment.SecondBranchOutputDirection))
			{
				OutputDirections.Add(Segment.SecondBranchOutputDirection);
			}

			ESRConveyorPlacementEndpointRole ResolvedRole = ESRConveyorPlacementEndpointRole::None;
			const bool bCanAddOutput = OutputDirections.Num() < 3
				&& (InputDirections.Num() <= 1 || OutputDirections.IsEmpty());
			const bool bCanAddInput = InputDirections.Num() < 3
				&& (OutputDirections.Num() <= 1 || InputDirections.IsEmpty());
			if (bCanAddOutput)
			{
				ResolvedRole = CombineEndpointRole(ResolvedRole, ESRConveyorPlacementEndpointRole::Source);
			}
			if (bCanAddInput)
			{
				ResolvedRole = CombineEndpointRole(ResolvedRole, ESRConveyorPlacementEndpointRole::Sink);
			}

			return ResolvedRole;
		}

		ESRConveyorPlacementEndpointRole ResolveConveyorPlacementEndpointRole(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const USRConveyorNetworkComponent* ConveyorNetwork,
			const FSRPlanetSurfaceGridCellId& CellId,
			int32 Layer)
		{
			const ESRConveyorPlacementEndpointRole ConveyorRole = ResolveExistingConveyorEndpointRole(ConveyorNetwork, CellId, Layer);
			return ConveyorRole != ESRConveyorPlacementEndpointRole::None
				? ConveyorRole
				: ResolveFacilityEndpointRole(SurfaceGrid, CellId);
		}

		int32 ScoreEndpointRoleAsPathStart(ESRConveyorPlacementEndpointRole Role)
		{
			switch (Role)
			{
			case ESRConveyorPlacementEndpointRole::Source:
				return 2;
			case ESRConveyorPlacementEndpointRole::Sink:
				return -2;
			default:
				return 0;
			}
		}

		int32 ScoreEndpointRoleAsPathEnd(ESRConveyorPlacementEndpointRole Role)
		{
			switch (Role)
			{
			case ESRConveyorPlacementEndpointRole::Sink:
				return 2;
			case ESRConveyorPlacementEndpointRole::Source:
				return -2;
			default:
				return 0;
			}
		}

		void OrientConveyorPathToConnectedEndpoints(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const USRConveyorNetworkComponent* ConveyorNetwork,
			int32 Layer,
			TArray<FSRPlanetSurfaceGridCellId>& PathCellIds)
		{
			if (PathCellIds.Num() < 2)
			{
				return;
			}

			const ESRConveyorPlacementEndpointRole FirstEndpointRole = ResolveConveyorPlacementEndpointRole(
				SurfaceGrid,
				ConveyorNetwork,
				PathCellIds[0],
				Layer);
			const ESRConveyorPlacementEndpointRole LastEndpointRole = ResolveConveyorPlacementEndpointRole(
				SurfaceGrid,
				ConveyorNetwork,
				PathCellIds.Last(),
				Layer);

			const int32 KeepScore = ScoreEndpointRoleAsPathStart(FirstEndpointRole)
				+ ScoreEndpointRoleAsPathEnd(LastEndpointRole);
			const int32 ReverseScore = ScoreEndpointRoleAsPathStart(LastEndpointRole)
				+ ScoreEndpointRoleAsPathEnd(FirstEndpointRole);
			if (ReverseScore > KeepScore)
			{
				Algo::Reverse(PathCellIds);
			}
		}

		bool DoesConveyorPathContainNewCell(
			const USRConveyorNetworkComponent* ConveyorNetwork,
			const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
			int32 Layer)
		{
			if (!IsValid(ConveyorNetwork))
			{
				return false;
			}

			const int32 SafeLayer = FMath::Max(0, Layer);
			for (const FSRPlanetSurfaceGridCellId& PathCellId : PathCellIds)
			{
				FSRConveyorLaneKey LaneKey;
				LaneKey.CellId = PathCellId;
				LaneKey.Layer = SafeLayer;
				if (!ConveyorNetwork->HasConveyorSegment(LaneKey))
				{
					return true;
				}
			}

			return false;
		}
	}

	const FSRPlanetSurfaceGridCellId& FSRAssemblyConveyorDragPathBuilder::ResolveAnchorCellId(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const TArray<FSRPlanetSurfaceGridCellId>& WaypointCellIds)
	{
		return WaypointCellIds.IsEmpty() ? StartCellId : WaypointCellIds.Last();
	}

	bool FSRAssemblyConveyorDragPathBuilder::IsSegmentWithinExtent(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId)
	{
		if (StartCellId.Face != EndCellId.Face)
		{
			return false;
		}

		const int32 ExtentX = FMath::Abs(EndCellId.CellX - StartCellId.CellX) + 1;
		const int32 ExtentY = FMath::Abs(EndCellId.CellY - StartCellId.CellY) + 1;
		return ExtentX <= MaxConveyorPlacementDragSegmentExtentCells
			&& ExtentY <= MaxConveyorPlacementDragSegmentExtentCells;
	}

	bool FSRAssemblyConveyorDragPathBuilder::BuildPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRAssemblyPlacementDragState& PlacementDrag,
		const FSRStructureData& ConveyorData,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutPathCellIds)
	{
		OutPathCellIds.Reset();
		if (!PlacementDrag.bIsConveyorPlacementDragActive
			|| !PlacementDrag.bHasConveyorDragStartCell
			|| !IsValid(PlacementDrag.ConveyorDragStartSurfaceGrid)
			|| PlacementDrag.ConveyorDragStartSurfaceGrid != SurfaceGrid
			|| !IsValid(SurfaceGrid)
			|| !IsValid(ConveyorNetwork)
			|| ConveyorData.BuildKind != ESRStructureBuildKind::Conveyor)
		{
			return false;
		}

		TArray<FSRPlanetSurfaceGridCellId> ControlCellIds;
		ControlCellIds.Reserve(PlacementDrag.ConveyorDragWaypointCellIds.Num() + 2);
		ControlCellIds.Add(PlacementDrag.ConveyorDragStartCellId);
		for (const FSRPlanetSurfaceGridCellId& WaypointCellId : PlacementDrag.ConveyorDragWaypointCellIds)
		{
			if (ControlCellIds.Last() == WaypointCellId)
			{
				continue;
			}

			ControlCellIds.Add(WaypointCellId);
		}
		if (!(ControlCellIds.Last() == TargetCellId))
		{
			ControlCellIds.Add(TargetCellId);
		}

		TSet<FSRPlanetSurfaceGridCellId> BlockedPreviewCellIds;
		for (int32 ControlIndex = 1; ControlIndex < ControlCellIds.Num(); ++ControlIndex)
		{
			if (!IsSegmentWithinExtent(ControlCellIds[ControlIndex - 1], ControlCellIds[ControlIndex]))
			{
				OutPathCellIds.Reset();
				return false;
			}

			TArray<FSRPlanetSurfaceGridCellId> SegmentCellIds;
			if (!ConveyorNetwork->FindConveyorPathAvoidingCells(
				SurfaceGrid,
				ControlCellIds[ControlIndex - 1],
				ControlCellIds[ControlIndex],
				ConveyorData.ConveyorLayer,
				BlockedPreviewCellIds,
				SegmentCellIds))
			{
				OutPathCellIds.Reset();
				return false;
			}

			AppendConveyorPathSegment(SegmentCellIds, OutPathCellIds);
			for (const FSRPlanetSurfaceGridCellId& SegmentCellId : SegmentCellIds)
			{
				BlockedPreviewCellIds.Add(SegmentCellId);
			}
		}

		if (OutPathCellIds.IsEmpty() && !ControlCellIds.IsEmpty())
		{
			OutPathCellIds.Add(ControlCellIds[0]);
		}
		OrientConveyorPathToConnectedEndpoints(SurfaceGrid, ConveyorNetwork, ConveyorData.ConveyorLayer, OutPathCellIds);
		if (OutPathCellIds.IsEmpty()
			|| !DoesConveyorPathContainNewCell(ConveyorNetwork, OutPathCellIds, ConveyorData.ConveyorLayer))
		{
			OutPathCellIds.Reset();
			return false;
		}

		const TSet<FSRPlanetSurfaceGridCellId> EmptyIgnoredOccupiedCellIds;
		if (!ConveyorNetwork->CanPlaceConveyorPath(
				SurfaceGrid,
				OutPathCellIds,
				ConveyorData.ConveyorLayer,
				EmptyIgnoredOccupiedCellIds))
		{
			OutPathCellIds.Reset();
			return false;
		}

		return true;
	}
}
