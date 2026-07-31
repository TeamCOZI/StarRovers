#if WITH_DEV_AUTOMATION_TESTS

#include "Pattern/SRPatternResolver.h"

#include "Misc/AutomationTest.h"

namespace StarRovers::PatternResolverTests
{
	FSRPatternMoveCommand MakeCommand(
		ESRPatternDirection Direction,
		int32 Distance)
	{
		FSRPatternMoveCommand Command;
		Command.SelectionMask.Reset(false);
		Command.Direction = Direction;
		Command.Distance = Distance;
		return Command;
	}

	FSRPatternResolveResult ResolveWithoutGrowth(
		const FSRPattern& Pattern,
		const FSRPatternMoveCommand& Command)
	{
		return FSRPatternResolver::ResolveMoveCycle(Pattern, Command, 0);
	}

	void SelectCell(FSRPatternMoveCommand& Command, int32 Row, int32 Column)
	{
		Command.SelectionMask.SetCellActive(Row, Column, true);
	}

	bool AreTraceEventsEqual(const FSRPatternTraceEvent& Left, const FSRPatternTraceEvent& Right)
	{
		return Left.Sequence == Right.Sequence
			&& Left.CommandIndex == Right.CommandIndex
			&& Left.OutputIndex == Right.OutputIndex
			&& Left.EnvironmentEffectIndex == Right.EnvironmentEffectIndex
			&& Left.EventKind == Right.EventKind
			&& Left.Glyph == Right.Glyph
			&& Left.OtherGlyph == Right.OtherGlyph
			&& Left.FromRow == Right.FromRow
			&& Left.FromColumn == Right.FromColumn
			&& Left.ToRow == Right.ToRow
			&& Left.ToColumn == Right.ToColumn
			&& Left.CollisionOutcome == Right.CollisionOutcome;
	}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRGlyphDominanceTest,
	"StarRovers.Pattern.Resolver.GlyphDominance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRGlyphDominanceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const ESRGlyphType Glyphs[] =
	{
		ESRGlyphType::Metal,
		ESRGlyphType::Organic,
		ESRGlyphType::Crystal,
		ESRGlyphType::Fluid,
		ESRGlyphType::Plasma,
	};
	for (const ESRGlyphType Glyph : Glyphs)
	{
		int32 WinCount = 0;
		int32 LossCount = 0;
		for (const ESRGlyphType OtherGlyph : Glyphs)
		{
			const ESRGlyphCollisionOutcome Outcome = FSRPatternResolver::ResolveCollision(Glyph, OtherGlyph);
			if (Glyph == OtherGlyph)
			{
				TestTrue(TEXT("Equal glyphs block each other."), Outcome == ESRGlyphCollisionOutcome::SameGlyphBlocked);
				continue;
			}

			WinCount += Outcome == ESRGlyphCollisionOutcome::MoverWins ? 1 : 0;
			const bool bMoverLost = Outcome == ESRGlyphCollisionOutcome::MoverBlocked
				|| Outcome == ESRGlyphCollisionOutcome::MoverDestroyed;
			LossCount += bMoverLost ? 1 : 0;
			const ESRGlyphCollisionOutcome ReverseOutcome = FSRPatternResolver::ResolveCollision(OtherGlyph, Glyph);
			const bool bReverseMoverLost = ReverseOutcome == ESRGlyphCollisionOutcome::MoverBlocked
				|| ReverseOutcome == ESRGlyphCollisionOutcome::MoverDestroyed;
			TestTrue(
				TEXT("Every pair has one winner."),
				(Outcome == ESRGlyphCollisionOutcome::MoverWins && bReverseMoverLost)
				|| (bMoverLost && ReverseOutcome == ESRGlyphCollisionOutcome::MoverWins));
		}

		TestEqual(TEXT("Every glyph wins two matchups."), WinCount, 2);
		TestEqual(TEXT("Every glyph loses two matchups."), LossCount, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRGlyphMovementIdentityTest,
	"StarRovers.Pattern.Resolver.MovementIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRGlyphMovementIdentityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern MetalPattern;
	MetalPattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	FSRPatternMoveCommand MetalCommand = MakeCommand(ESRPatternDirection::Right, 4);
	SelectCell(MetalCommand, 0, 0);
	const FSRPatternResolveResult MetalResult = ResolveWithoutGrowth(MetalPattern, MetalCommand);
	TestTrue(TEXT("Metal movement resolves."), MetalResult.bSucceeded);
	TestTrue(TEXT("Metal moves at most one cell."), MetalResult.OutputPattern.GetGlyph(0, 1) == ESRGlyphType::Metal);

	FSRPattern OrganicPattern;
	OrganicPattern.SetGlyph(1, 0, ESRGlyphType::Organic);
	FSRPatternMoveCommand OrganicCommand = MakeCommand(ESRPatternDirection::Right, 2);
	SelectCell(OrganicCommand, 1, 0);
	const FSRPatternResolveResult OrganicResult = ResolveWithoutGrowth(OrganicPattern, OrganicCommand);
	TestTrue(TEXT("Organic uses the requested distance."), OrganicResult.OutputPattern.GetGlyph(1, 2) == ESRGlyphType::Organic);

	FSRPattern CrystalPattern;
	CrystalPattern.SetGlyph(2, 1, ESRGlyphType::Crystal);
	FSRPatternMoveCommand CrystalCommand = MakeCommand(ESRPatternDirection::Right, 1);
	SelectCell(CrystalCommand, 2, 1);
	const FSRPatternResolveResult CrystalResult = ResolveWithoutGrowth(CrystalPattern, CrystalCommand);
	TestTrue(TEXT("Crystal slides to the board boundary."), CrystalResult.OutputPattern.GetGlyph(2, 4) == ESRGlyphType::Crystal);

	FSRPattern PlasmaPattern;
	PlasmaPattern.SetGlyph(3, 0, ESRGlyphType::Plasma);
	PlasmaPattern.SetGlyph(3, 1, ESRGlyphType::Organic);
	FSRPatternMoveCommand PlasmaCommand = MakeCommand(ESRPatternDirection::Right, 3);
	SelectCell(PlasmaCommand, 3, 0);
	const FSRPatternResolveResult PlasmaResult = ResolveWithoutGrowth(PlasmaPattern, PlasmaCommand);
	TestTrue(TEXT("Plasma lands at its exact jump distance."), PlasmaResult.OutputPattern.GetGlyph(3, 3) == ESRGlyphType::Plasma);
	TestTrue(TEXT("Plasma ignores intermediate cells."), PlasmaResult.OutputPattern.GetGlyph(3, 1) == ESRGlyphType::Organic);

	FSRPattern FluidPattern;
	FluidPattern.SetGlyph(2, 1, ESRGlyphType::Fluid);
	FluidPattern.SetGlyph(2, 2, ESRGlyphType::Organic);
	FSRPatternMoveCommand FluidCommand = MakeCommand(ESRPatternDirection::Right, 1);
	SelectCell(FluidCommand, 2, 1);
	const FSRPatternResolveResult FluidResult = ResolveWithoutGrowth(FluidPattern, FluidCommand);
	TestTrue(TEXT("Fluid detours clockwise around a stronger glyph."), FluidResult.OutputPattern.GetGlyph(3, 1) == ESRGlyphType::Fluid);
	TestTrue(TEXT("Fluid detour preserves the blocker."), FluidResult.OutputPattern.GetGlyph(2, 2) == ESRGlyphType::Organic);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRGlyphCollisionMovementTest,
	"StarRovers.Pattern.Resolver.CollisionMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRGlyphCollisionMovementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern MetalPattern;
	MetalPattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	MetalPattern.SetGlyph(0, 1, ESRGlyphType::Organic);
	FSRPatternMoveCommand MetalCommand = MakeCommand(ESRPatternDirection::Right, 5);
	SelectCell(MetalCommand, 0, 0);
	const FSRPatternResolveResult MetalResult = ResolveWithoutGrowth(MetalPattern, MetalCommand);
	TestTrue(TEXT("Metal overwrites Organic."), MetalResult.OutputPattern.GetGlyph(0, 1) == ESRGlyphType::Metal);
	TestEqual(TEXT("The overwritten pattern contains one occupied cell."), MetalResult.OutputPattern.GetOccupiedCellCount(), 1);

	FSRPattern BlockedOrganicPattern;
	BlockedOrganicPattern.SetGlyph(0, 0, ESRGlyphType::Organic);
	BlockedOrganicPattern.SetGlyph(0, 1, ESRGlyphType::Metal);
	FSRPatternMoveCommand BlockedOrganicCommand = MakeCommand(ESRPatternDirection::Right, 1);
	SelectCell(BlockedOrganicCommand, 0, 0);
	const FSRPatternResolveResult BlockedOrganicResult = ResolveWithoutGrowth(
		BlockedOrganicPattern,
		BlockedOrganicCommand);
	TestTrue(TEXT("Organic remains in place when Metal blocks it."), BlockedOrganicResult.OutputPattern.GetGlyph(0, 0) == ESRGlyphType::Organic);
	TestTrue(TEXT("Blocking Metal remains in place."), BlockedOrganicResult.OutputPattern.GetGlyph(0, 1) == ESRGlyphType::Metal);

	FSRPattern CrystalPattern;
	CrystalPattern.SetGlyph(1, 0, ESRGlyphType::Crystal);
	CrystalPattern.SetGlyph(1, 1, ESRGlyphType::Organic);
	CrystalPattern.SetGlyph(1, 3, ESRGlyphType::Plasma);
	FSRPatternMoveCommand CrystalCommand = MakeCommand(ESRPatternDirection::Right, 1);
	SelectCell(CrystalCommand, 1, 0);
	const FSRPatternResolveResult CrystalResult = ResolveWithoutGrowth(CrystalPattern, CrystalCommand);
	TestTrue(TEXT("Crystal cuts weaker glyphs and continues sliding."), CrystalResult.OutputPattern.GetGlyph(1, 4) == ESRGlyphType::Crystal);
	TestEqual(TEXT("Cut glyphs are destroyed."), CrystalResult.OutputPattern.GetOccupiedCellCount(), 1);

	FSRPattern BrokenCrystalPattern;
	BrokenCrystalPattern.SetGlyph(2, 0, ESRGlyphType::Crystal);
	BrokenCrystalPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	FSRPatternMoveCommand BrokenCrystalCommand = MakeCommand(ESRPatternDirection::Right, 1);
	SelectCell(BrokenCrystalCommand, 2, 0);
	const FSRPatternResolveResult BrokenCrystalResult = ResolveWithoutGrowth(BrokenCrystalPattern, BrokenCrystalCommand);
	TestTrue(TEXT("Crystal breaks against Metal."), BrokenCrystalResult.OutputPattern.GetGlyph(2, 0) == ESRGlyphType::Empty);
	TestTrue(TEXT("Metal survives a Crystal collision."), BrokenCrystalResult.OutputPattern.GetGlyph(2, 2) == ESRGlyphType::Metal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSROrganicGrowthTest,
	"StarRovers.Pattern.Resolver.OrganicGrowth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSROrganicGrowthTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern Pattern;
	Pattern.SetGlyph(2, 2, ESRGlyphType::Organic);
	Pattern.SetGlyph(2, 3, ESRGlyphType::Organic);
	Pattern.SetGlyph(4, 4, ESRGlyphType::Organic);
	FSRPatternMoveCommand Command = MakeCommand(ESRPatternDirection::Right, 1);
	const FSRPatternResolveResult Result = FSRPatternResolver::ResolveMoveCycle(Pattern, Command, 1);

	TestTrue(TEXT("Organic growth resolves without selected movers."), Result.bSucceeded);
	TestEqual(TEXT("Each Organic component creates one glyph."), Result.OutputPattern.CountGlyph(ESRGlyphType::Organic), 5);
	TestTrue(TEXT("The connected component follows directional priority."), Result.OutputPattern.GetGlyph(3, 2) == ESRGlyphType::Organic);
	TestTrue(TEXT("The edge component falls back to a valid direction."), Result.OutputPattern.GetGlyph(4, 3) == ESRGlyphType::Organic);

	int32 GrowthTraceCount = 0;
	for (const FSRPatternTraceEvent& Event : Result.TraceEvents)
	{
		GrowthTraceCount += Event.EventKind == ESRPatternTraceEventKind::OrganicGrowth ? 1 : 0;
	}
	TestEqual(TEXT("Every growth is represented in the trace."), GrowthTraceCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternResolverOrderingAndValidationTest,
	"StarRovers.Pattern.Resolver.OrderingAndValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternResolverOrderingAndValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern Pattern;
	Pattern.SetGlyph(0, 1, ESRGlyphType::Metal);
	Pattern.SetGlyph(0, 2, ESRGlyphType::Metal);
	FSRPatternMoveCommand Command = MakeCommand(ESRPatternDirection::Right, 1);
	SelectCell(Command, 0, 1);
	SelectCell(Command, 0, 2);
	const FSRPatternResolveResult Result = ResolveWithoutGrowth(Pattern, Command);
	TestTrue(TEXT("Selected glyphs resolve from front to back."), Result.OutputPattern.GetGlyph(0, 2) == ESRGlyphType::Metal);
	TestTrue(TEXT("The leading glyph vacates space for the trailing glyph."), Result.OutputPattern.GetGlyph(0, 3) == ESRGlyphType::Metal);
	TestEqual(TEXT("Both moves are traced."), Result.TraceEvents.Num(), 2);
	if (Result.TraceEvents.Num() >= 2)
	{
		TestEqual(TEXT("Trace sequence starts at zero."), Result.TraceEvents[0].Sequence, 0);
		TestEqual(TEXT("Trace sequence is contiguous."), Result.TraceEvents[1].Sequence, 1);
	}

	const FSRPatternResolveResult ReplayResult = ResolveWithoutGrowth(Pattern, Command);
	TestTrue(TEXT("Replaying a command produces the same pattern."), ReplayResult.OutputPattern == Result.OutputPattern);
	TestEqual(TEXT("Replaying a command produces the same trace length."), ReplayResult.TraceEvents.Num(), Result.TraceEvents.Num());
	const int32 ComparableEventCount = FMath::Min(Result.TraceEvents.Num(), ReplayResult.TraceEvents.Num());
	for (int32 EventIndex = 0; EventIndex < ComparableEventCount; ++EventIndex)
	{
		TestTrue(
			TEXT("Replaying a command produces the same ordered trace."),
			AreTraceEventsEqual(Result.TraceEvents[EventIndex], ReplayResult.TraceEvents[EventIndex]));
	}

	FSRPatternMoveCommand InvalidCommand = Command;
	InvalidCommand.Distance = 0;
	const FSRPatternResolveResult InvalidResult = ResolveWithoutGrowth(Pattern, InvalidCommand);
	TestFalse(TEXT("An invalid command fails without mutation."), InvalidResult.bSucceeded);
	TestTrue(TEXT("Invalid distance has a specific failure."), InvalidResult.Failure == ESRPatternResolveFailure::InvalidDistance);
	TestTrue(TEXT("Failed resolution preserves the input pattern."), InvalidResult.OutputPattern == Pattern);
	TestEqual(TEXT("Failed resolution emits no trace."), InvalidResult.TraceEvents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternCycleCompositionTest,
	"StarRovers.Pattern.Resolver.CycleComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternCycleCompositionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern Pattern;
	Pattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	Pattern.SetGlyph(2, 2, ESRGlyphType::Organic);

	FSRPatternMoveCommand FacilityCommand = MakeCommand(ESRPatternDirection::Right, 1);
	SelectCell(FacilityCommand, 0, 0);
	FSRPatternMoveCommand EnvironmentCommand = MakeCommand(ESRPatternDirection::Down, 1);
	SelectCell(EnvironmentCommand, 0, 1);

	FSRPatternCycleRequest Request;
	Request.MoveCommands.Add(FacilityCommand);
	Request.MoveCommands.Add(EnvironmentCommand);
	Request.OrganicGrowthsPerComponent = 1;
	Request.OrganicGrowthPriority = ESRPatternDirection::Right;
	const FSRPatternResolveResult Result = FSRPatternResolver::ResolveCycle(Pattern, Request);

	TestTrue(TEXT("A composed cycle resolves."), Result.bSucceeded);
	TestTrue(TEXT("A later command selects the current working pattern."), Result.OutputPattern.GetGlyph(1, 1) == ESRGlyphType::Metal);
	TestEqual(TEXT("Organic grows once after all move commands."), Result.OutputPattern.CountGlyph(ESRGlyphType::Organic), 2);
	TestTrue(TEXT("Cycle growth uses its explicit priority."), Result.OutputPattern.GetGlyph(2, 3) == ESRGlyphType::Organic);
	TestEqual(TEXT("Two moves and one growth share one trace."), Result.TraceEvents.Num(), 3);
	if (Result.TraceEvents.Num() >= 3)
	{
		TestEqual(TEXT("The first command owns the first move trace."), Result.TraceEvents[0].CommandIndex, 0);
		TestEqual(TEXT("The second command owns the second move trace."), Result.TraceEvents[1].CommandIndex, 1);
		TestEqual(TEXT("Post-cycle growth is outside a move command."), Result.TraceEvents[2].CommandIndex, INDEX_NONE);
	}

	FSRPatternCycleRequest InvalidRequest = Request;
	InvalidRequest.MoveCommands[1].Distance = 0;
	const FSRPatternResolveResult InvalidResult = FSRPatternResolver::ResolveCycle(Pattern, InvalidRequest);
	TestFalse(TEXT("A cycle with an invalid command fails."), InvalidResult.bSucceeded);
	TestEqual(TEXT("The failing command index is reported."), InvalidResult.FailedCommandIndex, 1);
	TestTrue(TEXT("Cycle validation is atomic."), InvalidResult.OutputPattern == Pattern);
	TestEqual(TEXT("An invalid cycle emits no partial trace."), InvalidResult.TraceEvents.Num(), 0);
	return true;
}
}

#endif
