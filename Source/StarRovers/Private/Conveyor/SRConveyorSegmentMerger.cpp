#include "Conveyor/SRConveyorSegmentMerger.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"

namespace
{
	bool TryAddUniqueDirection(
		ESRConveyorGridDirection Direction,
		ESRConveyorGridDirection& FirstDirection,
		ESRConveyorGridDirection& SecondDirection,
		ESRConveyorGridDirection& ThirdDirection,
		int32& DirectionCount)
	{
		if (Direction == ESRConveyorGridDirection::None
			|| Direction == FirstDirection
			|| Direction == SecondDirection
			|| Direction == ThirdDirection)
		{
			return true;
		}

		if (DirectionCount >= 3)
		{
			return false;
		}

		if (DirectionCount == 0)
		{
			FirstDirection = Direction;
		}
		else if (DirectionCount == 1)
		{
			SecondDirection = Direction;
		}
		else
		{
			ThirdDirection = Direction;
		}

		++DirectionCount;
		return true;
	}

	bool TryCountMergedDirections(
		ESRConveyorGridDirection ExistingFirstDirection,
		ESRConveyorGridDirection ExistingSecondDirection,
		ESRConveyorGridDirection ExistingThirdDirection,
		ESRConveyorGridDirection IncomingFirstDirection,
		ESRConveyorGridDirection IncomingSecondDirection,
		ESRConveyorGridDirection IncomingThirdDirection,
		int32& OutDirectionCount)
	{
		ESRConveyorGridDirection FirstDirection = ESRConveyorGridDirection::None;
		ESRConveyorGridDirection SecondDirection = ESRConveyorGridDirection::None;
		ESRConveyorGridDirection ThirdDirection = ESRConveyorGridDirection::None;
		OutDirectionCount = 0;
		return TryAddUniqueDirection(ExistingFirstDirection, FirstDirection, SecondDirection, ThirdDirection, OutDirectionCount)
			&& TryAddUniqueDirection(ExistingSecondDirection, FirstDirection, SecondDirection, ThirdDirection, OutDirectionCount)
			&& TryAddUniqueDirection(ExistingThirdDirection, FirstDirection, SecondDirection, ThirdDirection, OutDirectionCount)
			&& TryAddUniqueDirection(IncomingFirstDirection, FirstDirection, SecondDirection, ThirdDirection, OutDirectionCount)
			&& TryAddUniqueDirection(IncomingSecondDirection, FirstDirection, SecondDirection, ThirdDirection, OutDirectionCount)
			&& TryAddUniqueDirection(IncomingThirdDirection, FirstDirection, SecondDirection, ThirdDirection, OutDirectionCount);
	}
}

bool StarRovers::Conveyor::FSRConveyorSegmentMerger::CanMergeSegment(
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRConveyorSegment& Segment)
{
	const FSRConveyorSegment* ExistingSegment = Segments.Find(Segment.Lane);
	if (!ExistingSegment)
	{
		return true;
	}

	int32 InputDirectionCount = 0;
	int32 OutputDirectionCount = 0;
	if (!TryCountMergedDirections(
		ExistingSegment->InputDirection,
		ExistingSegment->MergeInputDirection,
		ExistingSegment->SecondMergeInputDirection,
		Segment.InputDirection,
		Segment.MergeInputDirection,
		Segment.SecondMergeInputDirection,
		InputDirectionCount)
		|| !TryCountMergedDirections(
			ExistingSegment->OutputDirection,
			ExistingSegment->BranchOutputDirection,
			ExistingSegment->SecondBranchOutputDirection,
			Segment.OutputDirection,
			Segment.BranchOutputDirection,
			Segment.SecondBranchOutputDirection,
			OutputDirectionCount))
	{
		return false;
	}

	return AreBranchCountsValid(InputDirectionCount, OutputDirectionCount);
}

void StarRovers::Conveyor::FSRConveyorSegmentMerger::MergeSegment(
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRConveyorSegment& Segment)
{
	FSRConveyorSegment* ExistingSegment = Segments.Find(Segment.Lane);
	if (!ExistingSegment)
	{
		Segments.Add(Segment.Lane, Segment);
		return;
	}

	MergeInputDirection(*ExistingSegment, Segment.InputDirection);
	MergeInputDirection(*ExistingSegment, Segment.MergeInputDirection);
	MergeInputDirection(*ExistingSegment, Segment.SecondMergeInputDirection);
	MergeOutputDirection(*ExistingSegment, Segment.OutputDirection);
	MergeOutputDirection(*ExistingSegment, Segment.BranchOutputDirection);
	MergeOutputDirection(*ExistingSegment, Segment.SecondBranchOutputDirection);
	if (ExistingSegment->NetworkId.IsNone())
	{
		ExistingSegment->NetworkId = Segment.NetworkId;
	}
	if (!IsValid(ExistingSegment->StructureDataAsset.Get()) && IsValid(Segment.StructureDataAsset.Get()))
	{
		ExistingSegment->StructureDataAsset = Segment.StructureDataAsset;
	}

	ExistingSegment->Shape = FSRConveyorNetworkGeometry::ResolveSegmentShape(ExistingSegment->InputDirection, ExistingSegment->OutputDirection);
}

bool StarRovers::Conveyor::FSRConveyorSegmentMerger::AreBranchCountsValid(int32 InputDirectionCount, int32 OutputDirectionCount)
{
	constexpr int32 MaxConveyorBranchDirectionCount = 3;
	if (InputDirectionCount > MaxConveyorBranchDirectionCount
		|| OutputDirectionCount > MaxConveyorBranchDirectionCount)
	{
		return false;
	}

	return InputDirectionCount <= 1 || OutputDirectionCount <= 1;
}

void StarRovers::Conveyor::FSRConveyorSegmentMerger::MergeInputDirection(
	FSRConveyorSegment& ExistingSegment,
	ESRConveyorGridDirection IncomingDirection)
{
	if (IncomingDirection == ESRConveyorGridDirection::None
		|| ExistingSegment.InputDirection == IncomingDirection
		|| ExistingSegment.MergeInputDirection == IncomingDirection
		|| ExistingSegment.SecondMergeInputDirection == IncomingDirection)
	{
		return;
	}

	if (ExistingSegment.InputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.InputDirection = IncomingDirection;
		return;
	}

	if (ExistingSegment.MergeInputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.MergeInputDirection = IncomingDirection;
		ExistingSegment.NextInputDirectionIndex = 0;
		return;
	}

	if (ExistingSegment.SecondMergeInputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.SecondMergeInputDirection = IncomingDirection;
		ExistingSegment.NextInputDirectionIndex = 0;
	}
}

void StarRovers::Conveyor::FSRConveyorSegmentMerger::MergeOutputDirection(
	FSRConveyorSegment& ExistingSegment,
	ESRConveyorGridDirection IncomingDirection)
{
	if (IncomingDirection == ESRConveyorGridDirection::None
		|| ExistingSegment.OutputDirection == IncomingDirection
		|| ExistingSegment.BranchOutputDirection == IncomingDirection
		|| ExistingSegment.SecondBranchOutputDirection == IncomingDirection)
	{
		return;
	}

	if (ExistingSegment.OutputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.OutputDirection = IncomingDirection;
		return;
	}

	if (ExistingSegment.BranchOutputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.BranchOutputDirection = IncomingDirection;
		ExistingSegment.NextOutputDirectionIndex = 0;
		return;
	}

	if (ExistingSegment.SecondBranchOutputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.SecondBranchOutputDirection = IncomingDirection;
		ExistingSegment.NextOutputDirectionIndex = 0;
	}
}
