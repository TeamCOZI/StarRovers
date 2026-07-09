#include "Conveyor/SRConveyorSegmentMerger.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"

bool StarRovers::Conveyor::FSRConveyorSegmentMerger::CanMergeSegment(
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const FSRConveyorSegment& Segment)
{
	const FSRConveyorSegment* ExistingSegment = Segments.Find(Segment.Lane);
	if (!ExistingSegment)
	{
		return true;
	}

	TArray<ESRConveyorGridDirection> InputDirections;
	TArray<ESRConveyorGridDirection> OutputDirections;
	FSRConveyorNetworkGeometry::CollectInputDirections(*ExistingSegment, InputDirections);
	FSRConveyorNetworkGeometry::CollectOutputDirections(*ExistingSegment, OutputDirections);
	auto CanAddDirection = [](TArray<ESRConveyorGridDirection>& Directions, ESRConveyorGridDirection IncomingDirection)
	{
		if (IncomingDirection == ESRConveyorGridDirection::None || Directions.Contains(IncomingDirection))
		{
			return true;
		}
		if (Directions.Num() >= 3)
		{
			return false;
		}

		Directions.Add(IncomingDirection);
		return true;
	};

	if (!CanAddDirection(InputDirections, Segment.InputDirection)
		|| !CanAddDirection(InputDirections, Segment.MergeInputDirection)
		|| !CanAddDirection(InputDirections, Segment.SecondMergeInputDirection)
		|| !CanAddDirection(OutputDirections, Segment.OutputDirection)
		|| !CanAddDirection(OutputDirections, Segment.BranchOutputDirection)
		|| !CanAddDirection(OutputDirections, Segment.SecondBranchOutputDirection))
	{
		return false;
	}

	return AreBranchCountsValid(InputDirections.Num(), OutputDirections.Num());
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
