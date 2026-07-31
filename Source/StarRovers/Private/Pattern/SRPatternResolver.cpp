#include "Pattern/SRPatternResolver.h"

namespace StarRovers::PatternResolver
{
	struct FSelectedGlyph
	{
		int32 Row = INDEX_NONE;
		int32 Column = INDEX_NONE;
		ESRGlyphType Glyph = ESRGlyphType::Empty;
	};

	struct FOrganicGrowthProposal
	{
		int32 SourceRow = INDEX_NONE;
		int32 SourceColumn = INDEX_NONE;
		int32 TargetRow = INDEX_NONE;
		int32 TargetColumn = INDEX_NONE;
	};

	ESRGlyphType GetCanonicalGlyph(const FSRPattern& Pattern, int32 Row, int32 Column)
	{
		int32 Index = INDEX_NONE;
		return StarRovers::Pattern::TryCoordinateToIndex(Row, Column, Index)
			? Pattern.Cells[Index]
			: ESRGlyphType::Empty;
	}

	void SetCanonicalGlyph(FSRPattern& Pattern, int32 Row, int32 Column, ESRGlyphType Glyph)
	{
		int32 Index = INDEX_NONE;
		if (StarRovers::Pattern::TryCoordinateToIndex(Row, Column, Index))
		{
			Pattern.Cells[Index] = Glyph;
		}
	}

	void AddTraceEvent(
		FSRPatternResolveResult& Result,
		ESRPatternTraceEventKind EventKind,
		ESRGlyphType Glyph,
		int32 FromRow,
		int32 FromColumn,
		int32 ToRow,
		int32 ToColumn,
		ESRGlyphType OtherGlyph = ESRGlyphType::Empty,
		ESRGlyphCollisionOutcome CollisionOutcome = ESRGlyphCollisionOutcome::None)
	{
		FSRPatternTraceEvent& Event = Result.TraceEvents.AddDefaulted_GetRef();
		Event.Sequence = Result.TraceEvents.Num() - 1;
		Event.CommandIndex = Result.ActiveTraceCommandIndex;
		Event.EventKind = EventKind;
		Event.Glyph = Glyph;
		Event.OtherGlyph = OtherGlyph;
		Event.FromRow = FromRow;
		Event.FromColumn = FromColumn;
		Event.ToRow = ToRow;
		Event.ToColumn = ToColumn;
		Event.CollisionOutcome = CollisionOutcome;
	}

	void MoveGlyphToEmptyCell(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		ESRPatternTraceEventKind EventKind,
		ESRGlyphType Glyph,
		int32& InOutRow,
		int32& InOutColumn,
		int32 TargetRow,
		int32 TargetColumn)
	{
		const int32 SourceRow = InOutRow;
		const int32 SourceColumn = InOutColumn;
		SetCanonicalGlyph(Pattern, SourceRow, SourceColumn, ESRGlyphType::Empty);
		SetCanonicalGlyph(Pattern, TargetRow, TargetColumn, Glyph);
		AddTraceEvent(
			Result,
			EventKind,
			Glyph,
			SourceRow,
			SourceColumn,
			TargetRow,
			TargetColumn);
		InOutRow = TargetRow;
		InOutColumn = TargetColumn;
	}

	bool ResolveOccupiedTarget(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		ESRGlyphType MovingGlyph,
		int32& InOutRow,
		int32& InOutColumn,
		int32 TargetRow,
		int32 TargetColumn,
		bool bContinueAfterMoverWin)
	{
		const ESRGlyphType DefendingGlyph = GetCanonicalGlyph(Pattern, TargetRow, TargetColumn);
		const ESRGlyphCollisionOutcome Outcome = FSRPatternResolver::ResolveCollision(MovingGlyph, DefendingGlyph);
		if (Outcome == ESRGlyphCollisionOutcome::SameGlyphBlocked
			|| Outcome == ESRGlyphCollisionOutcome::MoverBlocked)
		{
			AddTraceEvent(
				Result,
				ESRPatternTraceEventKind::Blocked,
				MovingGlyph,
				InOutRow,
				InOutColumn,
				TargetRow,
				TargetColumn,
				DefendingGlyph,
				Outcome);
			return false;
		}

		AddTraceEvent(
			Result,
			ESRPatternTraceEventKind::Collision,
			MovingGlyph,
			InOutRow,
			InOutColumn,
			TargetRow,
			TargetColumn,
			DefendingGlyph,
			Outcome);

		if (Outcome == ESRGlyphCollisionOutcome::MoverWins)
		{
			SetCanonicalGlyph(Pattern, InOutRow, InOutColumn, ESRGlyphType::Empty);
			SetCanonicalGlyph(Pattern, TargetRow, TargetColumn, MovingGlyph);
			InOutRow = TargetRow;
			InOutColumn = TargetColumn;
			return bContinueAfterMoverWin;
		}

		if (Outcome == ESRGlyphCollisionOutcome::MoverDestroyed)
		{
			SetCanonicalGlyph(Pattern, InOutRow, InOutColumn, ESRGlyphType::Empty);
		}
		return false;
	}

	void EjectGlyph(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		ESRGlyphType Glyph,
		int32 Row,
		int32 Column,
		int32 TargetRow,
		int32 TargetColumn)
	{
		SetCanonicalGlyph(Pattern, Row, Column, ESRGlyphType::Empty);
		AddTraceEvent(
			Result,
			ESRPatternTraceEventKind::Ejected,
			Glyph,
			Row,
			Column,
			TargetRow,
			TargetColumn);
	}

	void ResolveNormalMovement(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		const FSelectedGlyph& SelectedGlyph,
		int32 Distance,
		int32 RowDelta,
		int32 ColumnDelta)
	{
		int32 CurrentRow = SelectedGlyph.Row;
		int32 CurrentColumn = SelectedGlyph.Column;
		for (int32 Step = 0; Step < Distance; ++Step)
		{
			const int32 TargetRow = CurrentRow + RowDelta;
			const int32 TargetColumn = CurrentColumn + ColumnDelta;
			if (!StarRovers::Pattern::IsValidCoordinate(TargetRow, TargetColumn))
			{
				EjectGlyph(
					Pattern,
					Result,
					SelectedGlyph.Glyph,
					CurrentRow,
					CurrentColumn,
					TargetRow,
					TargetColumn);
				return;
			}

			if (GetCanonicalGlyph(Pattern, TargetRow, TargetColumn) == ESRGlyphType::Empty)
			{
				MoveGlyphToEmptyCell(
					Pattern,
					Result,
					ESRPatternTraceEventKind::Move,
					SelectedGlyph.Glyph,
					CurrentRow,
					CurrentColumn,
					TargetRow,
					TargetColumn);
				continue;
			}

			ResolveOccupiedTarget(
				Pattern,
				Result,
				SelectedGlyph.Glyph,
				CurrentRow,
				CurrentColumn,
				TargetRow,
				TargetColumn,
				false);
			return;
		}
	}

	void ResolveCrystalMovement(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		const FSelectedGlyph& SelectedGlyph,
		int32 RowDelta,
		int32 ColumnDelta)
	{
		int32 CurrentRow = SelectedGlyph.Row;
		int32 CurrentColumn = SelectedGlyph.Column;
		while (true)
		{
			const int32 TargetRow = CurrentRow + RowDelta;
			const int32 TargetColumn = CurrentColumn + ColumnDelta;
			if (!StarRovers::Pattern::IsValidCoordinate(TargetRow, TargetColumn))
			{
				AddTraceEvent(
					Result,
					ESRPatternTraceEventKind::BoundaryStop,
					SelectedGlyph.Glyph,
					CurrentRow,
					CurrentColumn,
					TargetRow,
					TargetColumn);
				return;
			}

			if (GetCanonicalGlyph(Pattern, TargetRow, TargetColumn) == ESRGlyphType::Empty)
			{
				MoveGlyphToEmptyCell(
					Pattern,
					Result,
					ESRPatternTraceEventKind::Move,
					SelectedGlyph.Glyph,
					CurrentRow,
					CurrentColumn,
					TargetRow,
					TargetColumn);
				continue;
			}

			if (!ResolveOccupiedTarget(
				Pattern,
				Result,
				SelectedGlyph.Glyph,
				CurrentRow,
				CurrentColumn,
				TargetRow,
				TargetColumn,
				true))
			{
				return;
			}
		}
	}

	void ResolvePlasmaMovement(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		const FSelectedGlyph& SelectedGlyph,
		int32 Distance,
		int32 RowDelta,
		int32 ColumnDelta)
	{
		int32 CurrentRow = SelectedGlyph.Row;
		int32 CurrentColumn = SelectedGlyph.Column;
		const int32 TargetRow = CurrentRow + RowDelta * Distance;
		const int32 TargetColumn = CurrentColumn + ColumnDelta * Distance;
		if (!StarRovers::Pattern::IsValidCoordinate(TargetRow, TargetColumn))
		{
			EjectGlyph(
				Pattern,
				Result,
				SelectedGlyph.Glyph,
				CurrentRow,
				CurrentColumn,
				TargetRow,
				TargetColumn);
			return;
		}

		if (GetCanonicalGlyph(Pattern, TargetRow, TargetColumn) == ESRGlyphType::Empty)
		{
			MoveGlyphToEmptyCell(
				Pattern,
				Result,
				ESRPatternTraceEventKind::Move,
				SelectedGlyph.Glyph,
				CurrentRow,
				CurrentColumn,
				TargetRow,
				TargetColumn);
			return;
		}

		ResolveOccupiedTarget(
			Pattern,
			Result,
			SelectedGlyph.Glyph,
			CurrentRow,
			CurrentColumn,
			TargetRow,
			TargetColumn,
			false);
	}

	void GetFluidSideDeltas(
		int32 RowDelta,
		int32 ColumnDelta,
		ESRPatternFluidSidePreference Preference,
		int32& OutFirstRowDelta,
		int32& OutFirstColumnDelta,
		int32& OutSecondRowDelta,
		int32& OutSecondColumnDelta)
	{
		const int32 ClockwiseRowDelta = ColumnDelta;
		const int32 ClockwiseColumnDelta = -RowDelta;
		const int32 CounterClockwiseRowDelta = -ColumnDelta;
		const int32 CounterClockwiseColumnDelta = RowDelta;

		if (Preference == ESRPatternFluidSidePreference::ClockwiseFirst)
		{
			OutFirstRowDelta = ClockwiseRowDelta;
			OutFirstColumnDelta = ClockwiseColumnDelta;
			OutSecondRowDelta = CounterClockwiseRowDelta;
			OutSecondColumnDelta = CounterClockwiseColumnDelta;
			return;
		}

		OutFirstRowDelta = CounterClockwiseRowDelta;
		OutFirstColumnDelta = CounterClockwiseColumnDelta;
		OutSecondRowDelta = ClockwiseRowDelta;
		OutSecondColumnDelta = ClockwiseColumnDelta;
	}

	bool TryFluidDetour(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		ESRGlyphType Glyph,
		int32& InOutRow,
		int32& InOutColumn,
		int32 FirstRowDelta,
		int32 FirstColumnDelta,
		int32 SecondRowDelta,
		int32 SecondColumnDelta)
	{
		const int32 CandidateRowDeltas[2] = { FirstRowDelta, SecondRowDelta };
		const int32 CandidateColumnDeltas[2] = { FirstColumnDelta, SecondColumnDelta };
		for (int32 CandidateIndex = 0; CandidateIndex < 2; ++CandidateIndex)
		{
			const int32 TargetRow = InOutRow + CandidateRowDeltas[CandidateIndex];
			const int32 TargetColumn = InOutColumn + CandidateColumnDeltas[CandidateIndex];
			if (!StarRovers::Pattern::IsValidCoordinate(TargetRow, TargetColumn)
				|| GetCanonicalGlyph(Pattern, TargetRow, TargetColumn) != ESRGlyphType::Empty)
			{
				continue;
			}

			MoveGlyphToEmptyCell(
				Pattern,
				Result,
				ESRPatternTraceEventKind::FluidDetour,
				Glyph,
				InOutRow,
				InOutColumn,
				TargetRow,
				TargetColumn);
			return true;
		}

		return false;
	}

	void ResolveFluidMovement(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		const FSelectedGlyph& SelectedGlyph,
		const FSRPatternMoveCommand& Command,
		int32 RowDelta,
		int32 ColumnDelta)
	{
		int32 FirstSideRowDelta = 0;
		int32 FirstSideColumnDelta = 0;
		int32 SecondSideRowDelta = 0;
		int32 SecondSideColumnDelta = 0;
		GetFluidSideDeltas(
			RowDelta,
			ColumnDelta,
			Command.FluidSidePreference,
			FirstSideRowDelta,
			FirstSideColumnDelta,
			SecondSideRowDelta,
			SecondSideColumnDelta);

		int32 CurrentRow = SelectedGlyph.Row;
		int32 CurrentColumn = SelectedGlyph.Column;
		for (int32 Step = 0; Step < Command.Distance; ++Step)
		{
			const int32 TargetRow = CurrentRow + RowDelta;
			const int32 TargetColumn = CurrentColumn + ColumnDelta;
			if (!StarRovers::Pattern::IsValidCoordinate(TargetRow, TargetColumn))
			{
				EjectGlyph(
					Pattern,
					Result,
					SelectedGlyph.Glyph,
					CurrentRow,
					CurrentColumn,
					TargetRow,
					TargetColumn);
				return;
			}

			const ESRGlyphType DefendingGlyph = GetCanonicalGlyph(Pattern, TargetRow, TargetColumn);
			if (DefendingGlyph == ESRGlyphType::Empty)
			{
				MoveGlyphToEmptyCell(
					Pattern,
					Result,
					ESRPatternTraceEventKind::Move,
					SelectedGlyph.Glyph,
					CurrentRow,
					CurrentColumn,
					TargetRow,
					TargetColumn);
				continue;
			}

			const ESRGlyphCollisionOutcome Outcome = FSRPatternResolver::ResolveCollision(
				SelectedGlyph.Glyph,
				DefendingGlyph);
			if (Outcome != ESRGlyphCollisionOutcome::MoverWins
				&& TryFluidDetour(
					Pattern,
					Result,
					SelectedGlyph.Glyph,
					CurrentRow,
					CurrentColumn,
					FirstSideRowDelta,
					FirstSideColumnDelta,
					SecondSideRowDelta,
					SecondSideColumnDelta))
			{
				continue;
			}

			ResolveOccupiedTarget(
				Pattern,
				Result,
				SelectedGlyph.Glyph,
				CurrentRow,
				CurrentColumn,
				TargetRow,
				TargetColumn,
				false);
			return;
		}
	}

	void GetGrowthDirectionOrder(
		int32 PrimaryRowDelta,
		int32 PrimaryColumnDelta,
		int32 OutRowDeltas[4],
		int32 OutColumnDeltas[4])
	{
		OutRowDeltas[0] = PrimaryRowDelta;
		OutColumnDeltas[0] = PrimaryColumnDelta;
		OutRowDeltas[1] = PrimaryColumnDelta;
		OutColumnDeltas[1] = -PrimaryRowDelta;
		OutRowDeltas[2] = -PrimaryRowDelta;
		OutColumnDeltas[2] = -PrimaryColumnDelta;
		OutRowDeltas[3] = -PrimaryColumnDelta;
		OutColumnDeltas[3] = PrimaryRowDelta;
	}

	void ApplyOrganicGrowth(
		FSRPattern& Pattern,
		FSRPatternResolveResult& Result,
		int32 GrowthsPerComponent,
		int32 PrimaryRowDelta,
		int32 PrimaryColumnDelta)
	{
		if (GrowthsPerComponent <= 0)
		{
			return;
		}

		int32 GrowthRowDeltas[4] = {};
		int32 GrowthColumnDeltas[4] = {};
		GetGrowthDirectionOrder(
			PrimaryRowDelta,
			PrimaryColumnDelta,
			GrowthRowDeltas,
			GrowthColumnDeltas);

		TArray<bool> bVisited;
		bVisited.Init(false, StarRovers::Pattern::CellCount);
		TArray<bool> bReservedGrowthCells;
		bReservedGrowthCells.Init(false, StarRovers::Pattern::CellCount);
		TArray<FOrganicGrowthProposal> Proposals;

		for (int32 SeedIndex = 0; SeedIndex < StarRovers::Pattern::CellCount; ++SeedIndex)
		{
			if (bVisited[SeedIndex] || Pattern.Cells[SeedIndex] != ESRGlyphType::Organic)
			{
				continue;
			}

			TArray<int32> PendingIndices;
			TArray<int32> ComponentIndices;
			PendingIndices.Add(SeedIndex);
			bVisited[SeedIndex] = true;
			while (!PendingIndices.IsEmpty())
			{
				const int32 CurrentIndex = PendingIndices.Pop(EAllowShrinking::No);
				ComponentIndices.Add(CurrentIndex);

				int32 CurrentRow = INDEX_NONE;
				int32 CurrentColumn = INDEX_NONE;
				StarRovers::Pattern::TryIndexToCoordinate(CurrentIndex, CurrentRow, CurrentColumn);
				for (int32 DirectionIndex = 0; DirectionIndex < 4; ++DirectionIndex)
				{
					const int32 NeighborRow = CurrentRow + GrowthRowDeltas[DirectionIndex];
					const int32 NeighborColumn = CurrentColumn + GrowthColumnDeltas[DirectionIndex];
					int32 NeighborIndex = INDEX_NONE;
					if (!StarRovers::Pattern::TryCoordinateToIndex(NeighborRow, NeighborColumn, NeighborIndex)
						|| bVisited[NeighborIndex]
						|| Pattern.Cells[NeighborIndex] != ESRGlyphType::Organic)
					{
						continue;
					}

					bVisited[NeighborIndex] = true;
					PendingIndices.Add(NeighborIndex);
				}
			}

			ComponentIndices.Sort();
			int32 RemainingGrowths = GrowthsPerComponent;
			for (const int32 SourceIndex : ComponentIndices)
			{
				if (RemainingGrowths <= 0)
				{
					break;
				}

				int32 SourceRow = INDEX_NONE;
				int32 SourceColumn = INDEX_NONE;
				StarRovers::Pattern::TryIndexToCoordinate(SourceIndex, SourceRow, SourceColumn);
				for (int32 DirectionIndex = 0; DirectionIndex < 4 && RemainingGrowths > 0; ++DirectionIndex)
				{
					const int32 TargetRow = SourceRow + GrowthRowDeltas[DirectionIndex];
					const int32 TargetColumn = SourceColumn + GrowthColumnDeltas[DirectionIndex];
					int32 TargetIndex = INDEX_NONE;
					if (!StarRovers::Pattern::TryCoordinateToIndex(TargetRow, TargetColumn, TargetIndex)
						|| Pattern.Cells[TargetIndex] != ESRGlyphType::Empty
						|| bReservedGrowthCells[TargetIndex])
					{
						continue;
					}

					FOrganicGrowthProposal& Proposal = Proposals.AddDefaulted_GetRef();
					Proposal.SourceRow = SourceRow;
					Proposal.SourceColumn = SourceColumn;
					Proposal.TargetRow = TargetRow;
					Proposal.TargetColumn = TargetColumn;
					bReservedGrowthCells[TargetIndex] = true;
					--RemainingGrowths;
				}
			}
		}

		for (const FOrganicGrowthProposal& Proposal : Proposals)
		{
			SetCanonicalGlyph(Pattern, Proposal.TargetRow, Proposal.TargetColumn, ESRGlyphType::Organic);
			AddTraceEvent(
				Result,
				ESRPatternTraceEventKind::OrganicGrowth,
				ESRGlyphType::Organic,
				Proposal.SourceRow,
				Proposal.SourceColumn,
				Proposal.TargetRow,
				Proposal.TargetColumn);
		}
	}

	bool IsSelectedGlyphAhead(
		const FSelectedGlyph& Left,
		const FSelectedGlyph& Right,
		ESRPatternDirection Direction)
	{
		switch (Direction)
		{
		case ESRPatternDirection::Up:
			return Left.Row != Right.Row ? Left.Row < Right.Row : Left.Column < Right.Column;
		case ESRPatternDirection::Right:
			return Left.Column != Right.Column ? Left.Column > Right.Column : Left.Row < Right.Row;
		case ESRPatternDirection::Down:
			return Left.Row != Right.Row ? Left.Row > Right.Row : Left.Column < Right.Column;
		case ESRPatternDirection::Left:
			return Left.Column != Right.Column ? Left.Column < Right.Column : Left.Row < Right.Row;
		default:
			return false;
		}
	}

	TArray<FSelectedGlyph> BuildSelectedGlyphs(
		const FSRPattern& Pattern,
		const FSRPatternMask& SelectionMask,
		ESRPatternDirection Direction)
	{
		TArray<FSelectedGlyph> SelectedGlyphs;
		for (int32 Index = 0; Index < StarRovers::Pattern::CellCount; ++Index)
		{
			if (!SelectionMask.ActiveCells[Index] || Pattern.Cells[Index] == ESRGlyphType::Empty)
			{
				continue;
			}

			FSelectedGlyph& SelectedGlyph = SelectedGlyphs.AddDefaulted_GetRef();
			StarRovers::Pattern::TryIndexToCoordinate(Index, SelectedGlyph.Row, SelectedGlyph.Column);
			SelectedGlyph.Glyph = Pattern.Cells[Index];
		}

		SelectedGlyphs.Sort([Direction](const FSelectedGlyph& Left, const FSelectedGlyph& Right)
		{
			return IsSelectedGlyphAhead(Left, Right, Direction);
		});
		return SelectedGlyphs;
	}

	ESRPatternResolveFailure ValidateMoveCommand(const FSRPatternMoveCommand& Command)
	{
		if (!Command.SelectionMask.IsCanonical())
		{
			return ESRPatternResolveFailure::InvalidSelectionMask;
		}
		if (!StarRovers::Pattern::IsValidDirection(Command.Direction))
		{
			return ESRPatternResolveFailure::InvalidDirection;
		}
		if (Command.FluidSidePreference != ESRPatternFluidSidePreference::ClockwiseFirst
			&& Command.FluidSidePreference != ESRPatternFluidSidePreference::CounterClockwiseFirst)
		{
			return ESRPatternResolveFailure::InvalidFluidSidePreference;
		}
		if (Command.Distance < 1 || Command.Distance > FSRPatternResolver::MaxCommandDistance)
		{
			return ESRPatternResolveFailure::InvalidDistance;
		}
		return ESRPatternResolveFailure::None;
	}

	void ExecuteMoveCommand(
		FSRPatternResolveResult& Result,
		const FSRPatternMoveCommand& Command)
	{
		int32 RowDelta = 0;
		int32 ColumnDelta = 0;
		StarRovers::Pattern::TryGetDirectionDelta(Command.Direction, RowDelta, ColumnDelta);

		const TArray<FSelectedGlyph> SelectedGlyphs = BuildSelectedGlyphs(
			Result.OutputPattern,
			Command.SelectionMask,
			Command.Direction);
		for (const FSelectedGlyph& SelectedGlyph : SelectedGlyphs)
		{
			if (GetCanonicalGlyph(Result.OutputPattern, SelectedGlyph.Row, SelectedGlyph.Column) != SelectedGlyph.Glyph)
			{
				continue;
			}

			switch (SelectedGlyph.Glyph)
			{
			case ESRGlyphType::Metal:
				ResolveNormalMovement(
					Result.OutputPattern,
					Result,
					SelectedGlyph,
					1,
					RowDelta,
					ColumnDelta);
				break;
			case ESRGlyphType::Organic:
				ResolveNormalMovement(
					Result.OutputPattern,
					Result,
					SelectedGlyph,
					Command.Distance,
					RowDelta,
					ColumnDelta);
				break;
			case ESRGlyphType::Crystal:
				ResolveCrystalMovement(
					Result.OutputPattern,
					Result,
					SelectedGlyph,
					RowDelta,
					ColumnDelta);
				break;
			case ESRGlyphType::Fluid:
				ResolveFluidMovement(
					Result.OutputPattern,
					Result,
					SelectedGlyph,
					Command,
					RowDelta,
					ColumnDelta);
				break;
			case ESRGlyphType::Plasma:
				ResolvePlasmaMovement(
					Result.OutputPattern,
					Result,
					SelectedGlyph,
					Command.Distance,
					RowDelta,
					ColumnDelta);
				break;
			case ESRGlyphType::Empty:
			default:
				break;
			}
		}
	}
}

FSRPatternResolveResult FSRPatternResolver::ResolveCycle(
	const FSRPattern& InputPattern,
	const FSRPatternCycleRequest& Request)
{
	FSRPatternResolveResult Result;
	Result.OutputPattern = InputPattern;

	if (!InputPattern.IsCanonical())
	{
		Result.Failure = ESRPatternResolveFailure::InvalidInputPattern;
		return Result;
	}
	if (Request.MoveCommands.Num() > MaxMoveCommandsPerCycle)
	{
		Result.Failure = ESRPatternResolveFailure::InvalidMoveCommandCount;
		return Result;
	}
	if (Request.OrganicGrowthsPerComponent < 0
		|| Request.OrganicGrowthsPerComponent > MaxOrganicGrowthsPerComponent)
	{
		Result.Failure = ESRPatternResolveFailure::InvalidOrganicGrowthCount;
		return Result;
	}
	if (!StarRovers::Pattern::IsValidDirection(Request.OrganicGrowthPriority))
	{
		Result.Failure = ESRPatternResolveFailure::InvalidDirection;
		return Result;
	}

	for (int32 CommandIndex = 0; CommandIndex < Request.MoveCommands.Num(); ++CommandIndex)
	{
		const ESRPatternResolveFailure CommandFailure = StarRovers::PatternResolver::ValidateMoveCommand(
			Request.MoveCommands[CommandIndex]);
		if (CommandFailure != ESRPatternResolveFailure::None)
		{
			Result.Failure = CommandFailure;
			Result.FailedCommandIndex = CommandIndex;
			return Result;
		}
	}

	for (int32 CommandIndex = 0; CommandIndex < Request.MoveCommands.Num(); ++CommandIndex)
	{
		Result.ActiveTraceCommandIndex = CommandIndex;
		StarRovers::PatternResolver::ExecuteMoveCommand(Result, Request.MoveCommands[CommandIndex]);
	}

	Result.ActiveTraceCommandIndex = INDEX_NONE;
	int32 GrowthRowDelta = 0;
	int32 GrowthColumnDelta = 0;
	StarRovers::Pattern::TryGetDirectionDelta(
		Request.OrganicGrowthPriority,
		GrowthRowDelta,
		GrowthColumnDelta);
	StarRovers::PatternResolver::ApplyOrganicGrowth(
		Result.OutputPattern,
		Result,
		Request.OrganicGrowthsPerComponent,
		GrowthRowDelta,
		GrowthColumnDelta);
	Result.bSucceeded = true;
	Result.Failure = ESRPatternResolveFailure::None;
	return Result;
}

FSRPatternResolveResult FSRPatternResolver::ResolveMoveCycle(
	const FSRPattern& InputPattern,
	const FSRPatternMoveCommand& Command,
	int32 OrganicGrowthsPerComponent)
{
	FSRPatternCycleRequest Request;
	Request.MoveCommands.Add(Command);
	Request.OrganicGrowthsPerComponent = OrganicGrowthsPerComponent;
	Request.OrganicGrowthPriority = Command.Direction;
	return ResolveCycle(InputPattern, Request);
}

ESRGlyphCollisionOutcome FSRPatternResolver::ResolveCollision(
	ESRGlyphType MovingGlyph,
	ESRGlyphType DefendingGlyph)
{
	if (MovingGlyph == ESRGlyphType::Empty || DefendingGlyph == ESRGlyphType::Empty
		|| !StarRovers::Pattern::IsValidGlyph(MovingGlyph)
		|| !StarRovers::Pattern::IsValidGlyph(DefendingGlyph))
	{
		return ESRGlyphCollisionOutcome::None;
	}
	if (MovingGlyph == DefendingGlyph)
	{
		return ESRGlyphCollisionOutcome::SameGlyphBlocked;
	}
	if (DoesGlyphDefeat(MovingGlyph, DefendingGlyph))
	{
		return ESRGlyphCollisionOutcome::MoverWins;
	}

	return MovingGlyph == ESRGlyphType::Organic && DefendingGlyph == ESRGlyphType::Metal
		? ESRGlyphCollisionOutcome::MoverBlocked
		: ESRGlyphCollisionOutcome::MoverDestroyed;
}

bool FSRPatternResolver::DoesGlyphDefeat(
	ESRGlyphType AttackingGlyph,
	ESRGlyphType DefendingGlyph)
{
	switch (AttackingGlyph)
	{
	case ESRGlyphType::Metal:
		return DefendingGlyph == ESRGlyphType::Organic
			|| DefendingGlyph == ESRGlyphType::Crystal;
	case ESRGlyphType::Organic:
		return DefendingGlyph == ESRGlyphType::Fluid
			|| DefendingGlyph == ESRGlyphType::Plasma;
	case ESRGlyphType::Crystal:
		return DefendingGlyph == ESRGlyphType::Organic
			|| DefendingGlyph == ESRGlyphType::Plasma;
	case ESRGlyphType::Fluid:
		return DefendingGlyph == ESRGlyphType::Metal
			|| DefendingGlyph == ESRGlyphType::Crystal;
	case ESRGlyphType::Plasma:
		return DefendingGlyph == ESRGlyphType::Metal
			|| DefendingGlyph == ESRGlyphType::Fluid;
	case ESRGlyphType::Empty:
	default:
		return false;
	}
}
