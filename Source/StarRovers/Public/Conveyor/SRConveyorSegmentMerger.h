#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorSegmentMerger
	{
		static bool CanMergeSegment(
			const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRConveyorSegment& Segment);

		static void MergeSegment(
			TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
			const FSRConveyorSegment& Segment);

	private:
		static bool AreBranchCountsValid(int32 InputDirectionCount, int32 OutputDirectionCount);
		static void MergeInputDirection(FSRConveyorSegment& ExistingSegment, ESRConveyorGridDirection IncomingDirection);
		static void MergeOutputDirection(FSRConveyorSegment& ExistingSegment, ESRConveyorGridDirection IncomingDirection);
	};
}
