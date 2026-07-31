#include "Pattern/SRStellarPatternContract.h"

namespace StarRovers::StellarContract::Private
{
	struct FShapeCell
	{
		int32 Row = 0;
		int32 Column = 0;
		ESRGlyphType Glyph = ESRGlyphType::Empty;
	};

	struct FMatchedCandidate
	{
		int32 RuleIndex = INDEX_NONE;
		uint32 CellBits = 0;
		uint64 PlacementKey = 0;
		ESRGlyphType MatchedGlyph = ESRGlyphType::Empty;
		FSRPatternHandMatch Match;
	};

	bool IsValidRuleKind(ESRPatternHandRuleKind RuleKind)
	{
		return RuleKind == ESRPatternHandRuleKind::SameGlyphShape
			|| RuleKind == ESRPatternHandRuleKind::ExactGlyphShape;
	}

	bool IsValidTransformPolicy(ESRPatternHandTransformPolicy TransformPolicy)
	{
		switch (TransformPolicy)
		{
		case ESRPatternHandTransformPolicy::Fixed:
		case ESRPatternHandTransformPolicy::Translate:
		case ESRPatternHandTransformPolicy::RotateAndTranslate:
		case ESRPatternHandTransformPolicy::RotateReflectAndTranslate:
			return true;
		default:
			return false;
		}
	}

	bool IsValidRarity(ESRPatternHandRarity Rarity)
	{
		return static_cast<uint8>(Rarity) <= static_cast<uint8>(ESRPatternHandRarity::Legendary);
	}

	bool HasPlayableGlyphOnMask(const FSRPattern& Pattern, const FSRPatternMask& Mask)
	{
		if (!Pattern.IsCanonical() || !Mask.IsCanonical())
		{
			return false;
		}

		for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
		{
			if (Mask.ActiveCells[CellIndex] && Pattern.Cells[CellIndex] != ESRGlyphType::Empty)
			{
				return true;
			}
		}
		return false;
	}

	int64 SaturatingAdd(int64 Left, int64 Right)
	{
		if (Right > 0 && Left > MAX_int64 - Right)
		{
			return MAX_int64;
		}
		if (Right < 0 && Left < MIN_int64 - Right)
		{
			return MIN_int64;
		}
		return Left + Right;
	}

	int64 SaturatingMultiply(int64 Value, int32 Multiplier)
	{
		if (Value <= 0 || Multiplier <= 0)
		{
			return 0;
		}
		return Value > MAX_int64 / static_cast<int64>(Multiplier)
			? MAX_int64
			: Value * static_cast<int64>(Multiplier);
	}

	int64 ScaleNonNegativeScore(int64 Value, double Multiplier)
	{
		if (Value <= 0 || !FMath::IsFinite(Multiplier) || Multiplier <= 0.0)
		{
			return 0;
		}
		const long double ScaledValue = static_cast<long double>(Value) * static_cast<long double>(Multiplier);
		if (ScaledValue >= static_cast<long double>(MAX_int64))
		{
			return MAX_int64;
		}
		return FMath::Max<int64>(0, FMath::RoundToInt64(static_cast<double>(ScaledValue)));
	}

	void TransformCell(
		int32 LocalRow,
		int32 LocalColumn,
		int32 SourceHeight,
		int32 SourceWidth,
		int32 RotationQuarterTurns,
		bool bReflect,
		int32& OutRow,
		int32& OutColumn,
		int32& OutHeight,
		int32& OutWidth)
	{
		const int32 ReflectedColumn = bReflect
			? SourceWidth - 1 - LocalColumn
			: LocalColumn;
		switch (RotationQuarterTurns & 3)
		{
		case 0:
			OutRow = LocalRow;
			OutColumn = ReflectedColumn;
			OutHeight = SourceHeight;
			OutWidth = SourceWidth;
			break;
		case 1:
			OutRow = ReflectedColumn;
			OutColumn = SourceHeight - 1 - LocalRow;
			OutHeight = SourceWidth;
			OutWidth = SourceHeight;
			break;
		case 2:
			OutRow = SourceHeight - 1 - LocalRow;
			OutColumn = SourceWidth - 1 - ReflectedColumn;
			OutHeight = SourceHeight;
			OutWidth = SourceWidth;
			break;
		default:
			OutRow = SourceWidth - 1 - ReflectedColumn;
			OutColumn = LocalRow;
			OutHeight = SourceWidth;
			OutWidth = SourceHeight;
			break;
		}
	}

	uint64 BuildPlacementKey(uint32 CellBits, const TArray<FShapeCell>& Cells)
	{
		uint32 ExpectedHash = 2166136261u;
		for (const FShapeCell& Cell : Cells)
		{
			const int32 CellIndex = Cell.Row * StarRovers::Pattern::GridSize + Cell.Column;
			ExpectedHash = (ExpectedHash ^ static_cast<uint32>(CellIndex)) * 16777619u;
			ExpectedHash = (ExpectedHash ^ static_cast<uint32>(Cell.Glyph)) * 16777619u;
		}
		return (static_cast<uint64>(ExpectedHash) << 32) | static_cast<uint64>(CellBits);
	}

	bool DoesPlacementMatch(
		const FSRPattern& Pattern,
		const FSRPatternHandRule& Rule,
		const TArray<FShapeCell>& Placement,
		ESRGlyphType& OutMatchedGlyph)
	{
		OutMatchedGlyph = ESRGlyphType::Empty;
		if (Placement.IsEmpty())
		{
			return false;
		}

		if (Rule.RuleKind == ESRPatternHandRuleKind::SameGlyphShape)
		{
			const FShapeCell& FirstCell = Placement[0];
			OutMatchedGlyph = Pattern.GetGlyph(FirstCell.Row, FirstCell.Column);
			if (OutMatchedGlyph == ESRGlyphType::Empty)
			{
				return false;
			}

			for (const FShapeCell& Cell : Placement)
			{
				if (Pattern.GetGlyph(Cell.Row, Cell.Column) != OutMatchedGlyph)
				{
					return false;
				}
			}
			return true;
		}

		for (const FShapeCell& Cell : Placement)
		{
			if (Pattern.GetGlyph(Cell.Row, Cell.Column) != Cell.Glyph)
			{
				return false;
			}
		}
		return true;
	}

	void TryAddPlacement(
		const FSRPattern& Pattern,
		const FSRPatternHandRule& Rule,
		int32 RuleIndex,
		TArray<FShapeCell> Placement,
		TSet<uint64>& SeenPlacements,
		TArray<FMatchedCandidate>& OutCandidates)
	{
		Placement.Sort([](const FShapeCell& Left, const FShapeCell& Right)
		{
			const int32 LeftIndex = Left.Row * StarRovers::Pattern::GridSize + Left.Column;
			const int32 RightIndex = Right.Row * StarRovers::Pattern::GridSize + Right.Column;
			return LeftIndex < RightIndex;
		});

		uint32 CellBits = 0;
		for (const FShapeCell& Cell : Placement)
		{
			const int32 CellIndex = Cell.Row * StarRovers::Pattern::GridSize + Cell.Column;
			CellBits |= 1u << CellIndex;
		}

		const uint64 PlacementKey = Rule.RuleKind == ESRPatternHandRuleKind::ExactGlyphShape
			? BuildPlacementKey(CellBits, Placement)
			: static_cast<uint64>(CellBits);
		if (SeenPlacements.Contains(PlacementKey))
		{
			return;
		}
		SeenPlacements.Add(PlacementKey);

		ESRGlyphType MatchedGlyph = ESRGlyphType::Empty;
		if (!DoesPlacementMatch(Pattern, Rule, Placement, MatchedGlyph))
		{
			return;
		}

		FMatchedCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
		Candidate.RuleIndex = RuleIndex;
		Candidate.CellBits = CellBits;
		Candidate.PlacementKey = PlacementKey;
		Candidate.MatchedGlyph = MatchedGlyph;
		Candidate.Match.RuleId = Rule.RuleId;
		Candidate.Match.DisplayName = Rule.DisplayName;
		Candidate.Match.Rarity = Rule.Rarity;
		Candidate.Match.MatchedGlyph = MatchedGlyph;
		Candidate.Match.BonusScore = Rule.BonusScore;
		Candidate.Match.MatchedCells.Reset(false);
		for (const FShapeCell& Cell : Placement)
		{
			Candidate.Match.MatchedCells.SetCellActive(Cell.Row, Cell.Column, true);
		}
	}

	void AppendMatchedCandidatesForRule(
		const FSRPattern& Pattern,
		const FSRPatternHandRule& Rule,
		int32 RuleIndex,
		TArray<FMatchedCandidate>& OutCandidates)
	{
		TArray<FShapeCell> SourceCells;
		int32 MinRow = StarRovers::Pattern::GridSize;
		int32 MinColumn = StarRovers::Pattern::GridSize;
		int32 MaxRow = INDEX_NONE;
		int32 MaxColumn = INDEX_NONE;
		for (int32 Row = 0; Row < StarRovers::Pattern::GridSize; ++Row)
		{
			for (int32 Column = 0; Column < StarRovers::Pattern::GridSize; ++Column)
			{
				if (!Rule.ShapeMask.IsCellActive(Row, Column))
				{
					continue;
				}

				FShapeCell& Cell = SourceCells.AddDefaulted_GetRef();
				Cell.Row = Row;
				Cell.Column = Column;
				Cell.Glyph = Rule.RequiredPattern.GetGlyph(Row, Column);
				MinRow = FMath::Min(MinRow, Row);
				MinColumn = FMath::Min(MinColumn, Column);
				MaxRow = FMath::Max(MaxRow, Row);
				MaxColumn = FMath::Max(MaxColumn, Column);
			}
		}

		TSet<uint64> SeenPlacements;
		if (Rule.TransformPolicy == ESRPatternHandTransformPolicy::Fixed)
		{
			TryAddPlacement(Pattern, Rule, RuleIndex, MoveTemp(SourceCells), SeenPlacements, OutCandidates);
			return;
		}

		const int32 SourceHeight = MaxRow - MinRow + 1;
		const int32 SourceWidth = MaxColumn - MinColumn + 1;
		for (FShapeCell& Cell : SourceCells)
		{
			Cell.Row -= MinRow;
			Cell.Column -= MinColumn;
		}

		const int32 RotationCount = Rule.TransformPolicy == ESRPatternHandTransformPolicy::Translate ? 1 : 4;
		const int32 ReflectionCount = Rule.TransformPolicy == ESRPatternHandTransformPolicy::RotateReflectAndTranslate ? 2 : 1;
		for (int32 ReflectionIndex = 0; ReflectionIndex < ReflectionCount; ++ReflectionIndex)
		{
			for (int32 RotationIndex = 0; RotationIndex < RotationCount; ++RotationIndex)
			{
				TArray<FShapeCell> TransformedCells;
				TransformedCells.Reserve(SourceCells.Num());
				int32 TransformedHeight = 0;
				int32 TransformedWidth = 0;
				for (const FShapeCell& SourceCell : SourceCells)
				{
					FShapeCell& TransformedCell = TransformedCells.AddDefaulted_GetRef();
					TransformCell(
						SourceCell.Row,
						SourceCell.Column,
						SourceHeight,
						SourceWidth,
						RotationIndex,
						ReflectionIndex == 1,
						TransformedCell.Row,
						TransformedCell.Column,
						TransformedHeight,
						TransformedWidth);
					TransformedCell.Glyph = SourceCell.Glyph;
				}

				for (int32 RowOffset = 0; RowOffset <= StarRovers::Pattern::GridSize - TransformedHeight; ++RowOffset)
				{
					for (int32 ColumnOffset = 0; ColumnOffset <= StarRovers::Pattern::GridSize - TransformedWidth; ++ColumnOffset)
					{
						TArray<FShapeCell> Placement = TransformedCells;
						for (FShapeCell& Cell : Placement)
						{
							Cell.Row += RowOffset;
							Cell.Column += ColumnOffset;
						}
						TryAddPlacement(Pattern, Rule, RuleIndex, MoveTemp(Placement), SeenPlacements, OutCandidates);
					}
				}
			}
		}
	}

	FSRPatternHandRule MakeSameGlyphRule(
		FName RuleId,
		const TCHAR* DisplayName,
		ESRPatternHandRarity Rarity,
		ESRPatternHandTransformPolicy TransformPolicy,
		int32 BonusScore,
		int32 MaximumMatches,
		std::initializer_list<FIntPoint> Cells)
	{
		FSRPatternHandRule Rule;
		Rule.RuleId = RuleId;
		Rule.DisplayName = FText::FromString(DisplayName);
		Rule.RuleKind = ESRPatternHandRuleKind::SameGlyphShape;
		Rule.Rarity = Rarity;
		Rule.TransformPolicy = TransformPolicy;
		Rule.BonusScore = BonusScore;
		Rule.MaximumMatches = MaximumMatches;
		Rule.ShapeMask.Reset(false);
		for (const FIntPoint& Cell : Cells)
		{
			Rule.ShapeMask.SetCellActive(Cell.X, Cell.Y, true);
		}
		return Rule;
	}
}

FSRStellarPatternContract::FSRStellarPatternContract()
{
	using namespace StarRovers::StellarContract::Private;

	ContractId = FName(TEXT("DefaultStellarContract"));
	RequiredPattern.Reset();
	RequiredMask.Reset(false);
	for (int32 Row = 1; Row <= 3; ++Row)
	{
		for (int32 Column = 1; Column <= 3; ++Column)
		{
			RequiredMask.SetCellActive(Row, Column, true);
		}
	}
	RequiredPattern.SetGlyph(2, 2, ESRGlyphType::Metal);

	BonusRules.Add(MakeSameGlyphRule(
		FName(TEXT("FiveInLine")),
		TEXT("Five in a Line"),
		ESRPatternHandRarity::Common,
		ESRPatternHandTransformPolicy::RotateAndTranslate,
		5,
		2,
		{ FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(0, 2), FIntPoint(0, 3), FIntPoint(0, 4) }));
	BonusRules.Add(MakeSameGlyphRule(
		FName(TEXT("SolidBlock")),
		TEXT("Solid Block"),
		ESRPatternHandRarity::Uncommon,
		ESRPatternHandTransformPolicy::Translate,
		8,
		2,
		{ FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(1, 1) }));
	BonusRules.Add(MakeSameGlyphRule(
		FName(TEXT("StellarCross")),
		TEXT("Stellar Cross"),
		ESRPatternHandRarity::Rare,
		ESRPatternHandTransformPolicy::RotateAndTranslate,
		14,
		1,
		{ FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(1, 1), FIntPoint(1, 2), FIntPoint(2, 1) }));
	BonusRules.Add(MakeSameGlyphRule(
		FName(TEXT("OuterRing")),
		TEXT("Outer Ring"),
		ESRPatternHandRarity::Legendary,
		ESRPatternHandTransformPolicy::Fixed,
		40,
		1,
		{
			FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(0, 2), FIntPoint(0, 3), FIntPoint(0, 4),
			FIntPoint(1, 0), FIntPoint(1, 4), FIntPoint(2, 0), FIntPoint(2, 4),
			FIntPoint(3, 0), FIntPoint(3, 4), FIntPoint(4, 0), FIntPoint(4, 1), FIntPoint(4, 2),
			FIntPoint(4, 3), FIntPoint(4, 4)
		}));
}

bool FSRStellarPatternContractResolver::ValidateContract(
	const FSRStellarPatternContract& Contract,
	FString& OutFailureReason)
{
	using namespace StarRovers::StellarContract::Private;

	OutFailureReason.Reset();
	if (Contract.ContractId.IsNone())
	{
		OutFailureReason = TEXT("ContractId must not be None.");
		return false;
	}
	if (!Contract.RequiredPattern.IsCanonical() || !Contract.RequiredMask.IsCanonical())
	{
		OutFailureReason = TEXT("The demand Pattern and Mask must both be canonical 5x5 values.");
		return false;
	}
	if (!Contract.RequiredMask.HasAnyActiveCell())
	{
		OutFailureReason = TEXT("The demand Mask must contain at least one active cell.");
		return false;
	}
	if (!HasPlayableGlyphOnMask(Contract.RequiredPattern, Contract.RequiredMask))
	{
		OutFailureReason = TEXT("The demand Mask must require at least one playable glyph.");
		return false;
	}
	if (Contract.BaseScorePerPattern <= 0 || Contract.RequiredScorePerCycle <= 0 || Contract.RequiredScoreGrowthPerCycle < 0)
	{
		OutFailureReason = TEXT("Base and required scores must be positive, and score growth must be non-negative.");
		return false;
	}
	if (!FMath::IsFinite(Contract.StellarHealthMaximum)
		|| !FMath::IsFinite(Contract.StartingStellarHealth)
		|| !FMath::IsFinite(Contract.InitialStellarHealthDecreasePerSecond)
		|| !FMath::IsFinite(Contract.StellarHealthDecreaseMultiplierPerPeriod)
		|| !FMath::IsFinite(Contract.StellarHealthRestoredPerPatternScore)
		|| Contract.StellarHealthMaximum <= 0.0
		|| Contract.StartingStellarHealth <= 0.0
		|| Contract.StartingStellarHealth > Contract.StellarHealthMaximum
		|| Contract.InitialStellarHealthDecreasePerSecond < 0.0
		|| Contract.StellarHealthDecreaseMultiplierPerPeriod < 1.0
		|| Contract.StellarHealthRestoredPerPatternScore < 0.0)
	{
		OutFailureReason = TEXT("Stellar health values must be finite and non-negative, initial health must fit the maximum, and the Period decrease multiplier cannot be below 1.");
		return false;
	}

	TSet<FName> RuleIds;
	for (int32 RuleIndex = 0; RuleIndex < Contract.BonusRules.Num(); ++RuleIndex)
	{
		const FSRPatternHandRule& Rule = Contract.BonusRules[RuleIndex];
		if (Rule.RuleId.IsNone() || RuleIds.Contains(Rule.RuleId))
		{
			OutFailureReason = FString::Printf(TEXT("Bonus rule %d has an empty or duplicate RuleId."), RuleIndex);
			return false;
		}
		RuleIds.Add(Rule.RuleId);
		if (!IsValidRuleKind(Rule.RuleKind)
			|| !IsValidTransformPolicy(Rule.TransformPolicy)
			|| !IsValidRarity(Rule.Rarity)
			|| !Rule.ShapeMask.IsCanonical()
			|| !Rule.ShapeMask.HasAnyActiveCell()
			|| Rule.ShapeMask.GetActiveCellCount() < 2
			|| Rule.BonusScore <= 0
			|| Rule.MaximumMatches <= 0
			|| Rule.MaximumMatches > StarRovers::Pattern::CellCount)
		{
			OutFailureReason = FString::Printf(TEXT("Bonus rule '%s' has invalid shape, enum, score, or match-count data."), *Rule.RuleId.ToString());
			return false;
		}
		if (Rule.RuleKind == ESRPatternHandRuleKind::ExactGlyphShape
			&& (!Rule.RequiredPattern.IsCanonical() || !HasPlayableGlyphOnMask(Rule.RequiredPattern, Rule.ShapeMask)))
		{
			OutFailureReason = FString::Printf(TEXT("Exact bonus rule '%s' must require at least one playable glyph on its ShapeMask."), *Rule.RuleId.ToString());
			return false;
		}
	}
	return true;
}

bool FSRStellarPatternContractResolver::DoesPatternMatchDemand(
	const FSRPattern& Pattern,
	const FSRStellarPatternContract& Contract)
{
	if (!Pattern.IsCanonical()
		|| !Contract.RequiredPattern.IsCanonical()
		|| !Contract.RequiredMask.IsCanonical()
		|| !Contract.RequiredMask.HasAnyActiveCell())
	{
		return false;
	}

	for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
	{
		if (Contract.RequiredMask.ActiveCells[CellIndex]
			&& Pattern.Cells[CellIndex] != Contract.RequiredPattern.Cells[CellIndex])
		{
			return false;
		}
	}
	return true;
}

int64 FSRStellarPatternContractResolver::GetRequiredScoreForCycle(
	const FSRStellarPatternContract& Contract,
	int32 CycleIndex,
	const FSRStellarPatternContractModifiers& Modifiers)
{
	using namespace StarRovers::StellarContract::Private;

	const int64 BaseScore = FMath::Max(0, Contract.RequiredScorePerCycle);
	const int64 GrowthScore = SaturatingMultiply(
		FMath::Max(0, Contract.RequiredScoreGrowthPerCycle),
		FMath::Max(0, CycleIndex));
	return ScaleNonNegativeScore(
		SaturatingAdd(BaseScore, GrowthScore),
		Modifiers.RequiredScoreMultiplier);
}

double FSRStellarPatternContractResolver::GetStellarHealthDecreasePerSecondForPeriod(
	const FSRStellarPatternContract& Contract,
	int32 PeriodIndex,
	const FSRStellarPatternContractModifiers& Modifiers)
{
	const double InitialDecrease = FMath::Max(0.0, Contract.InitialStellarHealthDecreasePerSecond);
	const double PeriodMultiplier = FMath::Max(1.0, Contract.StellarHealthDecreaseMultiplierPerPeriod);
	const double ModifierMultiplier = FMath::Max(0.0, Modifiers.HealthDamageMultiplier);
	const double MaximumUsefulDecrease = FMath::Max(0.0, Contract.StellarHealthMaximum);
	const double GrownDecrease = InitialDecrease
		* FMath::Pow(PeriodMultiplier, static_cast<double>(FMath::Max(0, PeriodIndex)))
		* ModifierMultiplier;
	return FMath::IsFinite(GrownDecrease)
		? FMath::Clamp(GrownDecrease, 0.0, MaximumUsefulDecrease)
		: MaximumUsefulDecrease;
}

double FSRStellarPatternContractResolver::GetStellarHealthRestorationForScore(
	const FSRStellarPatternContract& Contract,
	int64 PatternScore,
	const FSRStellarPatternContractModifiers& Modifiers)
{
	const double Restoration = static_cast<double>(FMath::Max<int64>(0, PatternScore))
		* FMath::Max(0.0, Contract.StellarHealthRestoredPerPatternScore)
		* FMath::Max(0.0, Modifiers.HealthRecoveryMultiplier);
	return FMath::IsFinite(Restoration)
		? FMath::Clamp(Restoration, 0.0, FMath::Max(0.0, Contract.StellarHealthMaximum))
		: FMath::Max(0.0, Contract.StellarHealthMaximum);
}

FSRStellarPatternScoreResult FSRStellarPatternContractResolver::ScorePattern(
	const FSRPattern& Pattern,
	int32 StackCount,
	const FSRStellarPatternContract& Contract,
	const FSRStellarPatternContractModifiers& Modifiers)
{
	using namespace StarRovers::StellarContract::Private;

	FSRStellarPatternScoreResult Result;
	Result.StackCount = FMath::Max(0, StackCount);
	Result.bPatternValid = Pattern.IsCanonical() && !Pattern.IsEmpty() && StackCount > 0;
	Result.bContractValid = ValidateContract(Contract, Result.FailureReason);
	if (!Result.bContractValid || !Result.bPatternValid)
	{
		if (Result.FailureReason.IsEmpty())
		{
			Result.FailureReason = TEXT("The submitted Pattern must be a non-empty canonical 5x5 Pattern with a positive stack count.");
		}
		return Result;
	}

	Result.bMatchesDemand = DoesPatternMatchDemand(Pattern, Contract);
	if (!Result.bMatchesDemand)
	{
		Result.FailureReason = TEXT("The submitted Pattern does not match every active demand-mask cell.");
		return Result;
	}

	TArray<FMatchedCandidate> Candidates;
	for (int32 RuleIndex = 0; RuleIndex < Contract.BonusRules.Num(); ++RuleIndex)
	{
		AppendMatchedCandidatesForRule(Pattern, Contract.BonusRules[RuleIndex], RuleIndex, Candidates);
	}
	Candidates.Sort([](const FMatchedCandidate& Left, const FMatchedCandidate& Right)
	{
		if (Left.Match.BonusScore != Right.Match.BonusScore)
		{
			return Left.Match.BonusScore > Right.Match.BonusScore;
		}
		if (Left.Match.Rarity != Right.Match.Rarity)
		{
			return static_cast<uint8>(Left.Match.Rarity) > static_cast<uint8>(Right.Match.Rarity);
		}
		if (Left.Match.RuleId != Right.Match.RuleId)
		{
			return Left.Match.RuleId.LexicalLess(Right.Match.RuleId);
		}
		return Left.PlacementKey < Right.PlacementKey;
	});

	uint32 ClaimedCells = 0;
	TArray<int32> MatchCountsByRule;
	MatchCountsByRule.Init(0, Contract.BonusRules.Num());
	for (const FMatchedCandidate& Candidate : Candidates)
	{
		if (!MatchCountsByRule.IsValidIndex(Candidate.RuleIndex)
			|| MatchCountsByRule[Candidate.RuleIndex] >= Contract.BonusRules[Candidate.RuleIndex].MaximumMatches)
		{
			continue;
		}
		if (!Contract.bAllowOverlappingBonusHands && (ClaimedCells & Candidate.CellBits) != 0)
		{
			continue;
		}

		++MatchCountsByRule[Candidate.RuleIndex];
		ClaimedCells |= Candidate.CellBits;
		Result.HandMatches.Add(Candidate.Match);
		Result.BonusScorePerPattern = SaturatingAdd(Result.BonusScorePerPattern, Candidate.Match.BonusScore);
	}

	Result.BaseScorePerPattern = static_cast<int32>(FMath::Min<int64>(
		MAX_int32,
		ScaleNonNegativeScore(Contract.BaseScorePerPattern, Modifiers.BaseScoreMultiplier)));
	Result.BonusScorePerPattern = ScaleNonNegativeScore(
		Result.BonusScorePerPattern,
		Modifiers.BonusScoreMultiplier);
	Result.ScorePerPattern = SaturatingAdd(Result.BaseScorePerPattern, Result.BonusScorePerPattern);
	Result.TotalScore = SaturatingMultiply(Result.ScorePerPattern, Result.StackCount);
	Result.FailureReason.Reset();
	return Result;
}

FSRStellarContractCycleSettlement FSRStellarPatternContractResolver::SettleCycle(
	const FSRStellarPatternContract& Contract,
	int32 CycleIndex,
	int64 SubmittedScore,
	const FSRStellarPatternContractModifiers& Modifiers)
{
	FSRStellarContractCycleSettlement Result;
	Result.CycleIndex = FMath::Max(0, CycleIndex);
	if (!ValidateContract(Contract, Result.FailureReason))
	{
		return Result;
	}

	Result.bValid = true;
	Result.RequiredScore = GetRequiredScoreForCycle(Contract, Result.CycleIndex, Modifiers);
	Result.SubmittedScore = FMath::Max<int64>(0, SubmittedScore);
	Result.MissingScore = FMath::Max<int64>(0, Result.RequiredScore - Result.SubmittedScore);
	Result.SurplusScore = FMath::Max<int64>(0, Result.SubmittedScore - Result.RequiredScore);
	Result.bRequirementMet = Result.MissingScore == 0;
	return Result;
}
