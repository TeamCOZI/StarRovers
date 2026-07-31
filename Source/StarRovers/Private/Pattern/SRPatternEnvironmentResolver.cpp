#include "Pattern/SRPatternEnvironmentResolver.h"

namespace StarRovers::PatternEnvironments::Private
{
	FSRPatternEnvironmentResolveResult MakeFailure(ESRPatternEnvironmentResolveFailure Failure)
	{
		FSRPatternEnvironmentResolveResult Result;
		Result.Failure = Failure;
		return Result;
	}

	bool IsValidFluidSidePreference(ESRPatternFluidSidePreference SidePreference)
	{
		return SidePreference == ESRPatternFluidSidePreference::ClockwiseFirst
			|| SidePreference == ESRPatternFluidSidePreference::CounterClockwiseFirst;
	}

	FSRPatternMask BuildAffectedGlyphMask(const FSRPattern& Pattern, ESRGlyphType AffectedGlyph)
	{
		FSRPatternMask SelectionMask;
		SelectionMask.Reset(false);
		for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
		{
			const ESRGlyphType Glyph = Pattern.Cells[CellIndex];
			const bool bMatches = Glyph != ESRGlyphType::Empty
				&& (AffectedGlyph == ESRGlyphType::Empty || Glyph == AffectedGlyph);
			SelectionMask.ActiveCells[CellIndex] = bMatches;
		}
		return SelectionMask;
	}

	void AppendTraceEvents(
		TArray<FSRPatternTraceEvent>& InOutTraceEvents,
		const TArray<FSRPatternTraceEvent>& NewTraceEvents,
		int32 EnvironmentEffectIndex,
		int32 EnvironmentCommandIndex)
	{
		for (const FSRPatternTraceEvent& SourceEvent : NewTraceEvents)
		{
			FSRPatternTraceEvent Event = SourceEvent;
			Event.Sequence = InOutTraceEvents.Num();
			Event.CommandIndex = EnvironmentCommandIndex;
			Event.EnvironmentEffectIndex = EnvironmentEffectIndex;
			InOutTraceEvents.Add(MoveTemp(Event));
		}
	}

	bool ApplyMove(
		const FSRPattern& InputPattern,
		const FSRPatternEnvironmentEffectSpec& EffectSpec,
		int32 Distance,
		int32 EnvironmentEffectIndex,
		int32 EnvironmentCommandIndex,
		int32 OrganicGrowthsPerComponent,
		bool bSelectAffectedGlyphs,
		FSRPattern& OutPattern,
		TArray<FSRPatternTraceEvent>& InOutTraceEvents)
	{
		FSRPatternMoveCommand Command;
		Command.SelectionMask = bSelectAffectedGlyphs
			? BuildAffectedGlyphMask(InputPattern, EffectSpec.AffectedGlyph)
			: FSRPatternMask(false);
		Command.Direction = EffectSpec.Direction;
		Command.Distance = Distance;
		Command.FluidSidePreference = EffectSpec.FluidSidePreference;
		const FSRPatternResolveResult ResolveResult = FSRPatternResolver::ResolveMoveCycle(
			InputPattern,
			Command,
			OrganicGrowthsPerComponent);
		if (!ResolveResult.bSucceeded)
		{
			return false;
		}

		OutPattern = ResolveResult.OutputPattern;
		AppendTraceEvents(
			InOutTraceEvents,
			ResolveResult.TraceEvents,
			EnvironmentEffectIndex,
			EnvironmentCommandIndex);
		return true;
	}
}

bool FSRPatternEnvironmentSpec::IsCanonical() const
{
	return FSRPatternEnvironmentResolver::IsValidEnvironmentSpec(*this);
}

void FSRPatternEnvironmentSpec::Normalize()
{
	if (EnvironmentId.IsNone())
	{
		EnvironmentId = FName(TEXT("Neutral"));
	}
	if (Effects.Num() > FSRPatternEnvironmentResolver::MaxEnvironmentEffects)
	{
		Effects.SetNum(FSRPatternEnvironmentResolver::MaxEnvironmentEffects);
	}

	for (FSRPatternEnvironmentEffectSpec& EffectSpec : Effects)
	{
		switch (EffectSpec.EffectKind)
		{
		case ESRPatternEnvironmentEffectKind::DirectionalPull:
		case ESRPatternEnvironmentEffectKind::ContinuousDrift:
		case ESRPatternEnvironmentEffectKind::OrganicBloom:
			break;
		default:
			EffectSpec.EffectKind = ESRPatternEnvironmentEffectKind::DirectionalPull;
			break;
		}

		if (EffectSpec.AffectedGlyph != ESRGlyphType::Empty
			&& !StarRovers::Pattern::IsValidGlyph(EffectSpec.AffectedGlyph))
		{
			EffectSpec.AffectedGlyph = ESRGlyphType::Empty;
		}
		if (!StarRovers::Pattern::IsValidDirection(EffectSpec.Direction))
		{
			EffectSpec.Direction = ESRPatternDirection::Down;
		}
		EffectSpec.Distance = FMath::Clamp(
			EffectSpec.Distance,
			1,
			FSRPatternEnvironmentResolver::MaxDirectionalDistance);
		EffectSpec.MaxDriftSteps = FMath::Clamp(
			EffectSpec.MaxDriftSteps,
			1,
			FSRPatternEnvironmentResolver::MaxContinuousDriftSteps);
		EffectSpec.OrganicGrowthsPerComponent = FMath::Clamp(
			EffectSpec.OrganicGrowthsPerComponent,
			1,
			FSRPatternResolver::MaxOrganicGrowthsPerComponent);
		if (!StarRovers::PatternEnvironments::Private::IsValidFluidSidePreference(
			EffectSpec.FluidSidePreference))
		{
			EffectSpec.FluidSidePreference = ESRPatternFluidSidePreference::ClockwiseFirst;
		}
	}
}

bool FSRPatternEnvironmentResolver::IsValidEffectSpec(
	const FSRPatternEnvironmentEffectSpec& EffectSpec)
{
	if ((EffectSpec.AffectedGlyph != ESRGlyphType::Empty
			&& !StarRovers::Pattern::IsValidGlyph(EffectSpec.AffectedGlyph))
		|| !StarRovers::Pattern::IsValidDirection(EffectSpec.Direction)
		|| !StarRovers::PatternEnvironments::Private::IsValidFluidSidePreference(
			EffectSpec.FluidSidePreference))
	{
		return false;
	}

	switch (EffectSpec.EffectKind)
	{
	case ESRPatternEnvironmentEffectKind::DirectionalPull:
		return EffectSpec.Distance >= 1 && EffectSpec.Distance <= MaxDirectionalDistance;
	case ESRPatternEnvironmentEffectKind::ContinuousDrift:
		return EffectSpec.MaxDriftSteps >= 1 && EffectSpec.MaxDriftSteps <= MaxContinuousDriftSteps;
	case ESRPatternEnvironmentEffectKind::OrganicBloom:
		return EffectSpec.OrganicGrowthsPerComponent >= 1
			&& EffectSpec.OrganicGrowthsPerComponent <= FSRPatternResolver::MaxOrganicGrowthsPerComponent;
	default:
		return false;
	}
}

bool FSRPatternEnvironmentResolver::IsValidEnvironmentSpec(
	const FSRPatternEnvironmentSpec& EnvironmentSpec)
{
	if (EnvironmentSpec.EnvironmentId.IsNone()
		|| EnvironmentSpec.Effects.Num() > MaxEnvironmentEffects)
	{
		return false;
	}
	for (const FSRPatternEnvironmentEffectSpec& EffectSpec : EnvironmentSpec.Effects)
	{
		if (!IsValidEffectSpec(EffectSpec))
		{
			return false;
		}
	}
	return true;
}

FSRPatternEnvironmentResolveResult FSRPatternEnvironmentResolver::Resolve(
	const FSRPattern& InputPattern,
	const FSRPatternEnvironmentSpec& EnvironmentSpec)
{
	using namespace StarRovers::PatternEnvironments::Private;

	if (!InputPattern.IsCanonical())
	{
		return MakeFailure(ESRPatternEnvironmentResolveFailure::InvalidInputPattern);
	}
	if (!IsValidEnvironmentSpec(EnvironmentSpec))
	{
		return MakeFailure(ESRPatternEnvironmentResolveFailure::InvalidEnvironmentSpec);
	}

	FSRPattern CurrentPattern = InputPattern;
	TArray<FSRPatternTraceEvent> TraceEvents;
	for (int32 EffectIndex = 0; EffectIndex < EnvironmentSpec.Effects.Num(); ++EffectIndex)
	{
		const FSRPatternEnvironmentEffectSpec& EffectSpec = EnvironmentSpec.Effects[EffectIndex];
		switch (EffectSpec.EffectKind)
		{
		case ESRPatternEnvironmentEffectKind::DirectionalPull:
		{
			FSRPattern NextPattern;
			if (!ApplyMove(
				CurrentPattern,
				EffectSpec,
				EffectSpec.Distance,
				EffectIndex,
				0,
				0,
				true,
				NextPattern,
				TraceEvents))
			{
				return MakeFailure(ESRPatternEnvironmentResolveFailure::ResolverFailure);
			}
			CurrentPattern = MoveTemp(NextPattern);
			break;
		}
		case ESRPatternEnvironmentEffectKind::ContinuousDrift:
		{
			for (int32 DriftStep = 0; DriftStep < EffectSpec.MaxDriftSteps; ++DriftStep)
			{
				FSRPattern NextPattern;
				if (!ApplyMove(
					CurrentPattern,
					EffectSpec,
					1,
					EffectIndex,
					DriftStep,
					0,
					true,
					NextPattern,
					TraceEvents))
				{
					return MakeFailure(ESRPatternEnvironmentResolveFailure::ResolverFailure);
				}
				const bool bReachedStablePattern = NextPattern == CurrentPattern;
				CurrentPattern = MoveTemp(NextPattern);
				if (bReachedStablePattern)
				{
					break;
				}
			}
			break;
		}
		case ESRPatternEnvironmentEffectKind::OrganicBloom:
		{
			FSRPattern NextPattern;
			if (!ApplyMove(
				CurrentPattern,
				EffectSpec,
				1,
				EffectIndex,
				0,
				EffectSpec.OrganicGrowthsPerComponent,
				false,
				NextPattern,
				TraceEvents))
			{
				return MakeFailure(ESRPatternEnvironmentResolveFailure::ResolverFailure);
			}
			CurrentPattern = MoveTemp(NextPattern);
			break;
		}
		default:
			return MakeFailure(ESRPatternEnvironmentResolveFailure::InvalidEnvironmentSpec);
		}
	}

	FSRPatternEnvironmentResolveResult Result;
	Result.bSucceeded = true;
	Result.OutputPattern = MoveTemp(CurrentPattern);
	Result.TraceEvents = MoveTemp(TraceEvents);
	return Result;
}
