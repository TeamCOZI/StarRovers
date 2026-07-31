#include "Pattern/SRPatternGenerationValidator.h"

void StarRovers::PatternGeneration::BuildSeededCandidateOrder(
	int32 CandidateCount,
	int32 RuntimeGenerationSeed,
	uint32 GeneratedSystemSignature,
	TArray<int32>& OutCandidateOrder)
{
	OutCandidateOrder.Reset(FMath::Max(0, CandidateCount));
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
	{
		OutCandidateOrder.Add(CandidateIndex);
	}

	uint32 SelectionSeed = HashCombineFast(
		GetTypeHash(FMath::Max(0, RuntimeGenerationSeed)),
		GeneratedSystemSignature);
	SelectionSeed = HashCombineFast(SelectionSeed, 0xA511E9B3u);
	FRandomStream SelectionStream(static_cast<int32>(SelectionSeed));
	for (int32 Index = OutCandidateOrder.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = SelectionStream.RandRange(0, Index);
		OutCandidateOrder.Swap(Index, SwapIndex);
	}
}

namespace StarRovers::PatternGeneration::Private
{
	struct FReachabilityState
	{
		int32 BodyIndex = INDEX_NONE;
		FSRPattern Pattern;
		int32 OperationDepth = 0;
		bool bUsedInterBodyTransfer = false;
		// Synthesis changes the resource identity to the configured product. The
		// Stellar Loom's source-resource filters therefore reject that state as input.
		bool bCanEnterSynthesis = true;
	};

	struct FSearchOutcome
	{
		bool bFoundGoal = false;
		bool bReachedStateLimit = false;
		int32 MinimumOperationDepth = INDEX_NONE;
		FName GoalBodyId = NAME_None;
		int32 ExploredStateCount = 0;
	};

	uint32 MakeStateHash(const FReachabilityState& State, FName BodyId)
	{
		uint32 Hash = HashCombineFast(GetTypeHash(BodyId), State.Pattern.GetStableHash());
		Hash = HashCombineFast(Hash, GetTypeHash(State.bUsedInterBodyTransfer));
		return HashCombineFast(Hash, GetTypeHash(State.bCanEnterSynthesis));
	}

	bool AreStatesEquivalent(
		const FReachabilityState& Left,
		const FReachabilityState& Right)
	{
		return Left.BodyIndex == Right.BodyIndex
			&& Left.bUsedInterBodyTransfer == Right.bUsedInterBodyTransfer
			&& Left.bCanEnterSynthesis == Right.bCanEnterSynthesis
			&& Left.Pattern == Right.Pattern;
	}

	bool ValidateRequest(
		const FSRPatternGenerationValidationRequest& Request,
		TMap<FName, int32>& OutBodyIndexById)
	{
		OutBodyIndexById.Reset();
		if (Request.Sources.IsEmpty()
			|| Request.Bodies.IsEmpty()
			|| !Request.Goal.RequiredPattern.IsCanonical()
			|| !Request.Goal.RequiredMask.IsCanonical()
			|| !Request.Goal.RequiredMask.HasAnyActiveCell()
			|| Request.MaxOperationDepth < 0
			|| Request.MaxOperationDepth > FSRPatternGenerationValidator::MaxOperationDepth
			|| Request.MaxReachableStates <= 0
			|| Request.MaxReachableStates > FSRPatternGenerationValidator::MaxReachableStates)
		{
			return false;
		}

		for (int32 BodyIndex = 0; BodyIndex < Request.Bodies.Num(); ++BodyIndex)
		{
			const FSRPatternGenerationBodySpec& BodySpec = Request.Bodies[BodyIndex];
			if (BodySpec.BodyId.IsNone()
				|| OutBodyIndexById.Contains(BodySpec.BodyId)
				|| !FSRPatternEnvironmentResolver::IsValidEnvironmentSpec(BodySpec.Environment)
				|| BodySpec.TransformOperators.Num() > FSRPatternGenerationValidator::MaxOperatorsPerBody
				|| BodySpec.SeparationOperators.Num() > FSRPatternGenerationValidator::MaxOperatorsPerBody)
			{
				return false;
			}

			for (const FSRPatternTransformOperatorSpec& TransformOperator : BodySpec.TransformOperators)
			{
				if (!FSRPatternFacilityResolver::IsValidTransformOperatorSpec(TransformOperator))
				{
					return false;
				}
			}
			for (const FSRPatternSeparationOperatorSpec& SeparationOperator : BodySpec.SeparationOperators)
			{
				if (!FSRPatternFacilityResolver::IsValidSeparationOperatorSpec(SeparationOperator))
				{
					return false;
				}
			}
			OutBodyIndexById.Add(BodySpec.BodyId, BodyIndex);
		}

		TSet<FName> SourceIds;
		for (const FSRPatternGenerationSourceSpec& SourceSpec : Request.Sources)
		{
			if (SourceSpec.SourceId.IsNone()
				|| SourceIds.Contains(SourceSpec.SourceId)
				|| !OutBodyIndexById.Contains(SourceSpec.BodyId)
				|| !SourceSpec.Pattern.IsCanonical()
				|| SourceSpec.Pattern.IsEmpty())
			{
				return false;
			}
			SourceIds.Add(SourceSpec.SourceId);
		}
		return true;
	}

	bool TryResolveEnvironment(
		const FSRPattern& FacilityOutput,
		const FSRPatternGenerationBodySpec& BodySpec,
		FSRPattern& OutPattern)
	{
		const FSRPatternEnvironmentResolveResult EnvironmentResult =
			FSRPatternEnvironmentResolver::Resolve(FacilityOutput, BodySpec.Environment);
		if (!EnvironmentResult.bSucceeded)
		{
			return false;
		}
		OutPattern = EnvironmentResult.OutputPattern;
		return true;
	}

	uint32 MakeGlyphPresenceMask(const FSRPattern& Pattern)
	{
		uint32 GlyphMask = 0;
		for (const ESRGlyphType Glyph : Pattern.Cells)
		{
			if (Glyph != ESRGlyphType::Empty)
			{
				GlyphMask |= 1u << static_cast<uint8>(Glyph);
			}
		}
		return GlyphMask;
	}

	bool CanProveGoalRequiresInterBodySources(
		const FSRPatternGenerationValidationRequest& Request,
		const TMap<FName, int32>& BodyIndexById)
	{
		uint32 RequiredGlyphMask = 0;
		for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
		{
			if (!Request.Goal.RequiredMask.ActiveCells[CellIndex])
			{
				continue;
			}

			const ESRGlyphType RequiredGlyph = Request.Goal.RequiredPattern.Cells[CellIndex];
			if (RequiredGlyph != ESRGlyphType::Empty)
			{
				RequiredGlyphMask |= 1u << static_cast<uint8>(RequiredGlyph);
			}
		}
		if (RequiredGlyphMask == 0)
		{
			return false;
		}

		TArray<uint32> AvailableGlyphMasks;
		AvailableGlyphMasks.Init(0, Request.Bodies.Num());
		for (const FSRPatternGenerationSourceSpec& SourceSpec : Request.Sources)
		{
			const int32* BodyIndex = BodyIndexById.Find(SourceSpec.BodyId);
			if (BodyIndex)
			{
				AvailableGlyphMasks[*BodyIndex] |= MakeGlyphPresenceMask(SourceSpec.Pattern);
			}
		}

		for (const uint32 AvailableGlyphMask : AvailableGlyphMasks)
		{
			if ((AvailableGlyphMask & RequiredGlyphMask) == RequiredGlyphMask)
			{
				return false;
			}
		}
		return true;
	}

	FSearchOutcome Search(
		const FSRPatternGenerationValidationRequest& Request,
		const TMap<FName, int32>& BodyIndexById,
		bool bAllowInterBodyTransfer,
		bool bRetainSynthesisOutputs)
	{
		FSearchOutcome Outcome;
		TArray<FReachabilityState> States;
		States.Reserve(Request.MaxReachableStates);
		TMap<uint32, TArray<int32>> StateIndicesByHash;
		TArray<TArray<int32>> WorkQueuesByDepth;
		WorkQueuesByDepth.SetNum(Request.MaxOperationDepth + 1);
		TArray<int32> WorkQueueIndices;
		WorkQueueIndices.Init(0, WorkQueuesByDepth.Num());

		auto RecordGoal = [&](const FReachabilityState& CandidateState, FName BodyId)
		{
			if (FSRPatternGenerationValidator::DoesPatternMatchGoal(CandidateState.Pattern, Request.Goal)
				&& (!Outcome.bFoundGoal || CandidateState.OperationDepth < Outcome.MinimumOperationDepth))
			{
				Outcome.bFoundGoal = true;
				Outcome.MinimumOperationDepth = CandidateState.OperationDepth;
				Outcome.GoalBodyId = BodyId;
			}
		};

		auto TryAddState = [&](FReachabilityState&& CandidateState)
		{
			if (Outcome.bReachedStateLimit)
			{
				return false;
			}
			const FName BodyId = Request.Bodies[CandidateState.BodyIndex].BodyId;
			RecordGoal(CandidateState, BodyId);
			if (Outcome.bFoundGoal
				&& CandidateState.OperationDepth >= Outcome.MinimumOperationDepth)
			{
				return true;
			}
			const uint32 StateHash = MakeStateHash(CandidateState, BodyId);
			if (TArray<int32>* ExistingIndices = StateIndicesByHash.Find(StateHash))
			{
				for (const int32 ExistingIndex : *ExistingIndices)
				{
					FReachabilityState& ExistingState = States[ExistingIndex];
					if (!AreStatesEquivalent(ExistingState, CandidateState))
					{
						continue;
					}
					if (CandidateState.OperationDepth < ExistingState.OperationDepth)
					{
						ExistingState.OperationDepth = CandidateState.OperationDepth;
						WorkQueuesByDepth[CandidateState.OperationDepth].Add(ExistingIndex);
					}
					return true;
				}
			}

			if (States.Num() >= Request.MaxReachableStates)
			{
				Outcome.bReachedStateLimit = true;
				return false;
			}

			const int32 NewStateIndex = States.Add(MoveTemp(CandidateState));
			StateIndicesByHash.FindOrAdd(StateHash).Add(NewStateIndex);
			WorkQueuesByDepth[States[NewStateIndex].OperationDepth].Add(NewStateIndex);
			return true;
		};

		for (const FSRPatternGenerationSourceSpec& SourceSpec : Request.Sources)
		{
			FReachabilityState InitialState;
			InitialState.BodyIndex = BodyIndexById.FindChecked(SourceSpec.BodyId);
			InitialState.Pattern = SourceSpec.Pattern;
			TryAddState(MoveTemp(InitialState));
		}

		for (int32 WorkDepth = 0;
			WorkDepth < WorkQueuesByDepth.Num() && !Outcome.bReachedStateLimit;
			++WorkDepth)
		{
			TArray<int32>& WorkQueue = WorkQueuesByDepth[WorkDepth];
			int32& WorkQueueIndex = WorkQueueIndices[WorkDepth];
			while (WorkQueue.IsValidIndex(WorkQueueIndex) && !Outcome.bReachedStateLimit)
			{
				const int32 StateIndex = WorkQueue[WorkQueueIndex++];
				if (!States.IsValidIndex(StateIndex))
				{
					continue;
				}
				const FReachabilityState CurrentState = States[StateIndex];
				if (CurrentState.OperationDepth != WorkDepth)
				{
					continue;
				}
				const FSRPatternGenerationBodySpec& BodySpec = Request.Bodies[CurrentState.BodyIndex];
				if (FSRPatternGenerationValidator::DoesPatternMatchGoal(CurrentState.Pattern, Request.Goal)
					|| CurrentState.OperationDepth >= Request.MaxOperationDepth
					|| (Outcome.bFoundGoal && CurrentState.OperationDepth >= Outcome.MinimumOperationDepth))
				{
					continue;
				}

			if (bAllowInterBodyTransfer)
			{
				for (int32 DestinationBodyIndex = 0;
					DestinationBodyIndex < Request.Bodies.Num() && !Outcome.bReachedStateLimit;
					++DestinationBodyIndex)
				{
					if (DestinationBodyIndex == CurrentState.BodyIndex)
					{
						continue;
					}
					FReachabilityState TransferredState = CurrentState;
					TransferredState.BodyIndex = DestinationBodyIndex;
					TransferredState.bUsedInterBodyTransfer = true;
					TryAddState(MoveTemp(TransferredState));
				}
			}

			auto AddFacilityOutput = [&](
				const FSRPattern& FacilityOutput,
				int32 OperationDepth,
				bool bUsedTransfer,
				bool bCanEnterSynthesis,
				bool bStoreState)
			{
				FSRPattern EnvironmentOutput;
				if (!TryResolveEnvironment(FacilityOutput, BodySpec, EnvironmentOutput))
				{
					return;
				}
				FReachabilityState OutputState;
				OutputState.BodyIndex = CurrentState.BodyIndex;
				OutputState.Pattern = MoveTemp(EnvironmentOutput);
				OutputState.OperationDepth = OperationDepth;
				OutputState.bUsedInterBodyTransfer = bUsedTransfer;
				OutputState.bCanEnterSynthesis = bCanEnterSynthesis;
				if (bStoreState)
				{
					TryAddState(MoveTemp(OutputState));
				}
				else
				{
					RecordGoal(OutputState, BodySpec.BodyId);
				}
			};

			for (const FSRPatternTransformOperatorSpec& TransformOperator : BodySpec.TransformOperators)
			{
				if (Outcome.bReachedStateLimit)
				{
					break;
				}
				const FSRPatternFacilityResolveResult TransformResult =
					FSRPatternFacilityResolver::ResolveTransform(CurrentState.Pattern, TransformOperator);
				if (TransformResult.bSucceeded && TransformResult.OutputPatterns.Num() == 1)
				{
					AddFacilityOutput(
						TransformResult.OutputPatterns[0],
						CurrentState.OperationDepth + 1,
						CurrentState.bUsedInterBodyTransfer,
						CurrentState.bCanEnterSynthesis,
						true);
				}
			}

			for (const FSRPatternSeparationOperatorSpec& SeparationOperator : BodySpec.SeparationOperators)
			{
				if (Outcome.bReachedStateLimit)
				{
					break;
				}
				const FSRPatternFacilityResolveResult SeparationResult =
					FSRPatternFacilityResolver::ResolveSeparation(CurrentState.Pattern, SeparationOperator);
				if (!SeparationResult.bSucceeded)
				{
					continue;
				}
				for (const FSRPattern& SeparationOutput : SeparationResult.OutputPatterns)
				{
					AddFacilityOutput(
						SeparationOutput,
						CurrentState.OperationDepth + 1,
						CurrentState.bUsedInterBodyTransfer,
						CurrentState.bCanEnterSynthesis,
						true);
				}
			}

			if (BodySpec.bAllowSynthesis && CurrentState.bCanEnterSynthesis)
			{
				const int32 KnownStateCount = States.Num();
				for (int32 OtherStateIndex = 0;
					OtherStateIndex < KnownStateCount && !Outcome.bReachedStateLimit;
					++OtherStateIndex)
				{
					const FReachabilityState OtherState = States[OtherStateIndex];
					if (OtherState.BodyIndex != CurrentState.BodyIndex)
					{
						continue;
					}
					if (!OtherState.bCanEnterSynthesis)
					{
						continue;
					}
					const int32 SynthesisDepth = FMath::Max(
						CurrentState.OperationDepth,
						OtherState.OperationDepth) + 1;
					if (SynthesisDepth > Request.MaxOperationDepth)
					{
						continue;
					}

					const bool bUsedTransfer = CurrentState.bUsedInterBodyTransfer
						|| OtherState.bUsedInterBodyTransfer;
					const FSRPatternFacilityResolveResult ForwardResult =
						FSRPatternFacilityResolver::ResolveSynthesis(CurrentState.Pattern, OtherState.Pattern);
					if (ForwardResult.bSucceeded && ForwardResult.OutputPatterns.Num() == 1)
					{
						AddFacilityOutput(
							ForwardResult.OutputPatterns[0],
							SynthesisDepth,
							bUsedTransfer,
							false,
							bRetainSynthesisOutputs);
					}

					if (OtherStateIndex != StateIndex)
					{
						const FSRPatternFacilityResolveResult ReverseResult =
							FSRPatternFacilityResolver::ResolveSynthesis(OtherState.Pattern, CurrentState.Pattern);
						if (ReverseResult.bSucceeded && ReverseResult.OutputPatterns.Num() == 1)
						{
							AddFacilityOutput(
								ReverseResult.OutputPatterns[0],
								SynthesisDepth,
								bUsedTransfer,
								false,
								bRetainSynthesisOutputs);
						}
					}
				}
			}
			}
		}

		Outcome.ExploredStateCount = States.Num();
		return Outcome;
	}
}

bool FSRPatternGenerationValidator::DoesPatternMatchGoal(
	const FSRPattern& Pattern,
	const FSRPatternGenerationGoal& Goal)
{
	if (!Pattern.IsCanonical()
		|| !Goal.RequiredPattern.IsCanonical()
		|| !Goal.RequiredMask.IsCanonical()
		|| !Goal.RequiredMask.HasAnyActiveCell())
	{
		return false;
	}
	for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
	{
		if (Goal.RequiredMask.ActiveCells[CellIndex]
			&& Pattern.Cells[CellIndex] != Goal.RequiredPattern.Cells[CellIndex])
		{
			return false;
		}
	}
	return true;
}

FSRPatternGenerationValidationResult FSRPatternGenerationValidator::Validate(
	const FSRPatternGenerationValidationRequest& Request)
{
	using namespace StarRovers::PatternGeneration::Private;

	FSRPatternGenerationValidationResult Result;
	Result.bValidationPerformed = true;
	TMap<FName, int32> BodyIndexById;
	if (!ValidateRequest(Request, BodyIndexById))
	{
		Result.Failure = ESRPatternGenerationValidationFailure::InvalidRequest;
		return Result;
	}

	const bool bGoalProvablyRequiresInterBodySources =
		Request.bRequireInterBodyTransfer
		&& CanProveGoalRequiresInterBodySources(Request, BodyIndexById);
	if (!bGoalProvablyRequiresInterBodySources)
	{
		const FSearchOutcome LocalOutcome = Search(Request, BodyIndexById, false, true);
		Result.ExploredStateCount = LocalOutcome.ExploredStateCount;
		if (LocalOutcome.bFoundGoal)
		{
			Result.bGoalReachable = true;
			Result.MinimumOperationDepth = LocalOutcome.MinimumOperationDepth;
			Result.GoalBodyId = LocalOutcome.GoalBodyId;
			if (Request.bRequireInterBodyTransfer)
			{
				Result.Failure = ESRPatternGenerationValidationFailure::InterBodyTransferNotRequired;
				return Result;
			}

			Result.bSolvable = true;
			return Result;
		}
		if (LocalOutcome.bReachedStateLimit)
		{
			Result.Failure = ESRPatternGenerationValidationFailure::StateLimitExceeded;
			return Result;
		}
	}

	// Check the common two-branch automation spine first without retaining every
	// non-goal product. If it cannot witness the goal, the full pass below keeps
	// products so post-Synthesis Transform and Separation paths remain searchable.
	const FSearchOutcome DirectSynthesisOutcome = Search(Request, BodyIndexById, true, false);
	if (DirectSynthesisOutcome.bFoundGoal)
	{
		Result.bSolvable = true;
		Result.bGoalReachable = true;
		Result.bRequiresInterBodyTransfer = true;
		Result.MinimumOperationDepth = DirectSynthesisOutcome.MinimumOperationDepth;
		Result.GoalBodyId = DirectSynthesisOutcome.GoalBodyId;
		Result.ExploredStateCount = DirectSynthesisOutcome.ExploredStateCount;
		return Result;
	}

	const FSearchOutcome GlobalOutcome = Search(Request, BodyIndexById, true, true);
	Result.ExploredStateCount = GlobalOutcome.ExploredStateCount;
	if (GlobalOutcome.bFoundGoal)
	{
		Result.bSolvable = true;
		Result.bGoalReachable = true;
		Result.bRequiresInterBodyTransfer = true;
		Result.MinimumOperationDepth = GlobalOutcome.MinimumOperationDepth;
		Result.GoalBodyId = GlobalOutcome.GoalBodyId;
		return Result;
	}

	Result.Failure = GlobalOutcome.bReachedStateLimit
		? ESRPatternGenerationValidationFailure::StateLimitExceeded
		: ESRPatternGenerationValidationFailure::GoalUnreachable;
	return Result;
}
