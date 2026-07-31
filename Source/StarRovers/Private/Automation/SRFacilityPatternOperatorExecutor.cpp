#include "SRFacilityPatternOperatorExecutor.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceDataAsset.h"
#include "Pattern/SRPatternEnvironmentResolver.h"
#include "SRFacilityRunModifierResolver.h"

namespace StarRovers::FacilityPatterns::Private
{
	bool IsValidPatternResource(const FSRResourceInstance& ResourceInstance)
	{
		return !ResourceInstance.ResourceId.IsNone()
			&& ResourceInstance.StackCount > 0
			&& ResourceInstance.Pattern.IsCanonical();
	}

	bool TryResolveOperation(
		const USRFacilityDataAsset* FacilityDataAsset,
		const TArray<FSRResourceInstance>& InputResources,
		const FSRResolvedRunModifiers& Modifiers,
		FSRPatternFacilityResolveResult& OutResult)
	{
		OutResult = FSRPatternFacilityResolveResult();
		if (!IsValid(FacilityDataAsset))
		{
			return false;
		}

		switch (FacilityDataAsset->OperationKind)
		{
		case ESRFacilityOperationKind::Process:
		{
			if (InputResources.Num() != 1)
			{
				return false;
			}
			FSRPatternTransformOperatorSpec TransformOperator = FacilityDataAsset->TransformOperator;
			TransformOperator.OrganicGrowthsPerComponent = FMath::Clamp(
				TransformOperator.OrganicGrowthsPerComponent + Modifiers.TransformOrganicGrowthDelta,
				0,
				StarRovers::Pattern::GridSize - 1);
			OutResult = FSRPatternFacilityResolver::ResolveTransform(
				InputResources[0].Pattern,
				TransformOperator);
			break;
		}

		case ESRFacilityOperationKind::Synthesize:
			if (InputResources.Num() != 2 || !IsValid(FacilityDataAsset->SynthesisOutputResource.Get()))
			{
				return false;
			}
			OutResult = FSRPatternFacilityResolver::ResolveSynthesis(
				InputResources[0].Pattern,
				InputResources[1].Pattern);
			break;

		case ESRFacilityOperationKind::Separate:
			if (InputResources.Num() != 1)
			{
				return false;
			}
			OutResult = FSRPatternFacilityResolver::ResolveSeparation(
				InputResources[0].Pattern,
				FacilityDataAsset->SeparationOperator);
			break;

		default:
			return false;
		}

		return OutResult.bSucceeded;
	}

	FSRResourceInstance MakeOutputResource(
		const USRFacilityDataAsset* FacilityDataAsset,
		const TArray<FSRResourceInstance>& InputResources,
		const FSRPattern& OutputPattern)
	{
		FSRResourceInstance OutputResource;
		if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize)
		{
			OutputResource = FacilityDataAsset->SynthesisOutputResource->BuildDefaultInstance();
			OutputResource.SourcePatternId = NAME_None;
			OutputResource.SourcePatternSeed = 0;
		}
		else
		{
			OutputResource = InputResources[0];
		}

		OutputResource.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutputResource.Pattern = OutputPattern;
		OutputResource.StackCount = 1;
		return OutputResource;
	}

	bool ApplyEnvironment(
		const FSRPatternEnvironmentSpec& EnvironmentSpec,
		FSRPatternFacilityResolveResult& InOutResolveResult)
	{
		TArray<FSRPatternTraceEvent> EnvironmentTraceEvents;
		for (int32 OutputIndex = 0; OutputIndex < InOutResolveResult.OutputPatterns.Num(); ++OutputIndex)
		{
			const FSRPatternEnvironmentResolveResult EnvironmentResult =
				FSRPatternEnvironmentResolver::Resolve(
					InOutResolveResult.OutputPatterns[OutputIndex],
					EnvironmentSpec);
			if (!EnvironmentResult.bSucceeded)
			{
				return false;
			}

			InOutResolveResult.OutputPatterns[OutputIndex] = EnvironmentResult.OutputPattern;
			for (const FSRPatternTraceEvent& SourceEvent : EnvironmentResult.TraceEvents)
			{
				FSRPatternTraceEvent Event = SourceEvent;
				Event.OutputIndex = OutputIndex;
				EnvironmentTraceEvents.Add(MoveTemp(Event));
			}
		}

		for (FSRPatternTraceEvent& Event : EnvironmentTraceEvents)
		{
			Event.Sequence = InOutResolveResult.TraceEvents.Num();
			InOutResolveResult.TraceEvents.Add(MoveTemp(Event));
		}
		return true;
	}
}

bool FSRFacilityPatternOperatorExecutor::IsPatternOperation(const USRFacilityDataAsset* FacilityDataAsset)
{
	if (!IsValid(FacilityDataAsset) || FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard)
	{
		return false;
	}

	return FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process
		|| FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize
		|| FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Separate;
}

int32 FSRFacilityPatternOperatorExecutor::ResolveRequiredInputCount(const USRFacilityDataAsset* FacilityDataAsset)
{
	if (!IsPatternOperation(FacilityDataAsset))
	{
		return 0;
	}

	return FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize ? 2 : 1;
}

int32 FSRFacilityPatternOperatorExecutor::ResolveExpectedOutputCount(const USRFacilityDataAsset* FacilityDataAsset)
{
	if (!IsPatternOperation(FacilityDataAsset))
	{
		return 0;
	}

	return FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Separate ? 2 : 1;
}

bool FSRFacilityPatternOperatorExecutor::CanExecute(
	const USRFacilityDataAsset* FacilityDataAsset,
	const TArray<FSRResourceInstance>& InputResources)
{
	if (!IsPatternOperation(FacilityDataAsset)
		|| InputResources.Num() != ResolveRequiredInputCount(FacilityDataAsset))
	{
		return false;
	}

	for (const FSRResourceInstance& InputResource : InputResources)
	{
		if (!StarRovers::FacilityPatterns::Private::IsValidPatternResource(InputResource))
		{
			return false;
		}
	}

	FSRPatternFacilityResolveResult ResolveResult;
	return StarRovers::FacilityPatterns::Private::TryResolveOperation(
		FacilityDataAsset,
		InputResources,
		FSRResolvedRunModifiers(),
		ResolveResult);
}

bool FSRFacilityPatternOperatorExecutor::TryBuildOutputResources(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	TArray<FSRResourceInstance>& OutOutputResources,
	FSRPatternFacilityResolveResult* OutResolveResult)
{
	OutOutputResources.Reset();
	if (OutResolveResult)
	{
		*OutResolveResult = FSRPatternFacilityResolveResult();
	}

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsPatternOperation(FacilityDataAsset)
		|| InputResources.Num() != ResolveRequiredInputCount(FacilityDataAsset))
	{
		return false;
	}
	for (const FSRResourceInstance& InputResource : InputResources)
	{
		if (!StarRovers::FacilityPatterns::Private::IsValidPatternResource(InputResource))
		{
			return false;
		}
	}

	FSRPatternFacilityResolveResult ResolveResult;
	const FSRResolvedRunModifiers Modifiers = FSRFacilityRunModifierResolver::Resolve(
		FacilityInstance,
		InputResources);
	if (!StarRovers::FacilityPatterns::Private::TryResolveOperation(
		FacilityDataAsset,
		InputResources,
		Modifiers,
		ResolveResult)
		|| ResolveResult.OutputPatterns.Num() != ResolveExpectedOutputCount(FacilityDataAsset)
		|| !StarRovers::FacilityPatterns::Private::ApplyEnvironment(
			FSRFacilityRunModifierResolver::ResolveEnvironmentSpec(
				FacilityInstance.PatternEnvironment,
				Modifiers.EnvironmentIntensityDelta),
			ResolveResult))
	{
		return false;
	}

	OutOutputResources.Reserve(ResolveResult.OutputPatterns.Num());
	for (const FSRPattern& OutputPattern : ResolveResult.OutputPatterns)
	{
		OutOutputResources.Add(StarRovers::FacilityPatterns::Private::MakeOutputResource(
			FacilityDataAsset,
			InputResources,
			OutputPattern));
	}
	if (OutResolveResult)
	{
		*OutResolveResult = MoveTemp(ResolveResult);
	}
	return true;
}
