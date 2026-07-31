#include "Pattern/SRPatternFacilityResolver.h"

namespace StarRovers::PatternFacilities::Private
{
	FSRPatternFacilityResolveResult MakeFailure(ESRPatternFacilityResolveFailure Failure)
	{
		FSRPatternFacilityResolveResult Result;
		Result.Failure = Failure;
		return Result;
	}

	FSRPatternTraceEvent MakeCellTraceEvent(
		int32 Sequence,
		ESRPatternTraceEventKind EventKind,
		ESRGlyphType Glyph,
		ESRGlyphType OtherGlyph,
		int32 CellIndex,
		int32 OutputIndex,
		ESRGlyphCollisionOutcome CollisionOutcome = ESRGlyphCollisionOutcome::None)
	{
		int32 Row = INDEX_NONE;
		int32 Column = INDEX_NONE;
		StarRovers::Pattern::TryIndexToCoordinate(CellIndex, Row, Column);

		FSRPatternTraceEvent Event;
		Event.Sequence = Sequence;
		Event.EventKind = EventKind;
		Event.Glyph = Glyph;
		Event.OtherGlyph = OtherGlyph;
		Event.FromRow = Row;
		Event.FromColumn = Column;
		Event.ToRow = Row;
		Event.ToColumn = Column;
		Event.OutputIndex = OutputIndex;
		Event.CollisionOutcome = CollisionOutcome;
		return Event;
	}
}

bool FSRPatternFacilityResolver::IsValidTransformOperatorSpec(
	const FSRPatternTransformOperatorSpec& OperatorSpec)
{
	return OperatorSpec.SelectionMask.IsCanonical()
		&& OperatorSpec.SelectionMask.HasAnyActiveCell()
		&& StarRovers::Pattern::IsValidDirection(OperatorSpec.Direction)
		&& OperatorSpec.OrganicGrowthsPerComponent >= 0
		&& OperatorSpec.OrganicGrowthsPerComponent <= FSRPatternResolver::MaxOrganicGrowthsPerComponent
		&& (OperatorSpec.FluidSidePreference == ESRPatternFluidSidePreference::ClockwiseFirst
			|| OperatorSpec.FluidSidePreference == ESRPatternFluidSidePreference::CounterClockwiseFirst);
}

bool FSRPatternFacilityResolver::IsValidSeparationOperatorSpec(
	const FSRPatternSeparationOperatorSpec& OperatorSpec)
{
	return OperatorSpec.PrimaryOutputMask.IsCanonical()
		&& OperatorSpec.PrimaryOutputMask.HasAnyActiveCell()
		&& !OperatorSpec.PrimaryOutputMask.AreAllCellsActive();
}

FSRPatternFacilityResolveResult FSRPatternFacilityResolver::ResolveTransform(
	const FSRPattern& InputPattern,
	const FSRPatternTransformOperatorSpec& OperatorSpec)
{
	using namespace StarRovers::PatternFacilities::Private;

	if (!InputPattern.IsCanonical())
	{
		return MakeFailure(ESRPatternFacilityResolveFailure::InvalidInputPattern);
	}
	if (!IsValidTransformOperatorSpec(OperatorSpec))
	{
		return MakeFailure(ESRPatternFacilityResolveFailure::InvalidTransformSpec);
	}

	FSRPatternMoveCommand MoveCommand;
	MoveCommand.SelectionMask = OperatorSpec.SelectionMask;
	MoveCommand.Direction = OperatorSpec.Direction;
	MoveCommand.Distance = 1;
	MoveCommand.FluidSidePreference = OperatorSpec.FluidSidePreference;
	const FSRPatternResolveResult MoveResult = FSRPatternResolver::ResolveMoveCycle(
		InputPattern,
		MoveCommand,
		OperatorSpec.OrganicGrowthsPerComponent);
	if (!MoveResult.bSucceeded)
	{
		return MakeFailure(ESRPatternFacilityResolveFailure::InvalidTransformSpec);
	}

	FSRPatternFacilityResolveResult Result;
	Result.bSucceeded = true;
	Result.OutputPatterns.Add(MoveResult.OutputPattern);
	Result.TraceEvents = MoveResult.TraceEvents;
	for (FSRPatternTraceEvent& TraceEvent : Result.TraceEvents)
	{
		TraceEvent.OutputIndex = 0;
	}
	return Result;
}

FSRPatternFacilityResolveResult FSRPatternFacilityResolver::ResolveSynthesis(
	const FSRPattern& BasePattern,
	const FSRPattern& OverlayPattern)
{
	using namespace StarRovers::PatternFacilities::Private;

	if (!BasePattern.IsCanonical() || !OverlayPattern.IsCanonical())
	{
		return MakeFailure(ESRPatternFacilityResolveFailure::InvalidInputPattern);
	}

	FSRPattern OutputPattern = BasePattern;
	FSRPatternFacilityResolveResult Result;
	for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
	{
		const ESRGlyphType BaseGlyph = BasePattern.Cells[CellIndex];
		const ESRGlyphType OverlayGlyph = OverlayPattern.Cells[CellIndex];
		if (OverlayGlyph == ESRGlyphType::Empty)
		{
			continue;
		}
		if (BaseGlyph == ESRGlyphType::Empty)
		{
			OutputPattern.Cells[CellIndex] = OverlayGlyph;
			continue;
		}

		const ESRGlyphCollisionOutcome CollisionOutcome = FSRPatternResolver::ResolveCollision(
			OverlayGlyph,
			BaseGlyph);
		if (CollisionOutcome == ESRGlyphCollisionOutcome::MoverWins)
		{
			OutputPattern.Cells[CellIndex] = OverlayGlyph;
		}
		Result.TraceEvents.Add(MakeCellTraceEvent(
			Result.TraceEvents.Num(),
			ESRPatternTraceEventKind::Collision,
			OverlayGlyph,
			BaseGlyph,
			CellIndex,
			0,
			CollisionOutcome));
	}

	Result.bSucceeded = true;
	Result.OutputPatterns.Add(MoveTemp(OutputPattern));
	return Result;
}

FSRPatternFacilityResolveResult FSRPatternFacilityResolver::ResolveSeparation(
	const FSRPattern& InputPattern,
	const FSRPatternSeparationOperatorSpec& OperatorSpec)
{
	using namespace StarRovers::PatternFacilities::Private;

	if (!InputPattern.IsCanonical())
	{
		return MakeFailure(ESRPatternFacilityResolveFailure::InvalidInputPattern);
	}
	if (!IsValidSeparationOperatorSpec(OperatorSpec))
	{
		return MakeFailure(ESRPatternFacilityResolveFailure::InvalidSeparationMask);
	}

	FSRPattern PrimaryOutput;
	FSRPattern SecondaryOutput;
	int32 PrimaryGlyphCount = 0;
	int32 SecondaryGlyphCount = 0;
	FSRPatternFacilityResolveResult Result;
	for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
	{
		const ESRGlyphType Glyph = InputPattern.Cells[CellIndex];
		if (Glyph == ESRGlyphType::Empty)
		{
			continue;
		}

		const int32 OutputIndex = OperatorSpec.PrimaryOutputMask.ActiveCells[CellIndex] ? 0 : 1;
		if (OutputIndex == 0)
		{
			PrimaryOutput.Cells[CellIndex] = Glyph;
			++PrimaryGlyphCount;
		}
		else
		{
			SecondaryOutput.Cells[CellIndex] = Glyph;
			++SecondaryGlyphCount;
		}
		Result.TraceEvents.Add(MakeCellTraceEvent(
			Result.TraceEvents.Num(),
			ESRPatternTraceEventKind::Separation,
			Glyph,
			ESRGlyphType::Empty,
			CellIndex,
			OutputIndex));
	}

	if (PrimaryGlyphCount <= 0 || SecondaryGlyphCount <= 0)
	{
		return MakeFailure(ESRPatternFacilityResolveFailure::EmptySeparationOutput);
	}

	Result.bSucceeded = true;
	Result.OutputPatterns.Add(MoveTemp(PrimaryOutput));
	Result.OutputPatterns.Add(MoveTemp(SecondaryOutput));
	return Result;
}
