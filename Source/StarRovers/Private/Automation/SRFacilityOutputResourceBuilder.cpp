#include "SRFacilityOutputResourceBuilder.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "SRFacilityPatternOperatorExecutor.h"

namespace
{
	void ResetOutputArguments(
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount,
		FSRResourceInstance* OutBaselinePrimaryResource,
		TArray<FString>* OutOperationSummaryTexts)
	{
		OutOutputResources.Reset();
		if (OutPrimaryOutputCount)
		{
			*OutPrimaryOutputCount = 0;
		}
		if (OutBaselinePrimaryResource)
		{
			*OutBaselinePrimaryResource = FSRResourceInstance();
		}
		if (OutOperationSummaryTexts)
		{
			OutOperationSummaryTexts->Reset();
		}
	}

	const TCHAR* GetOperationLabel(ESRFacilityOperationKind OperationKind)
	{
		switch (OperationKind)
		{
		case ESRFacilityOperationKind::Process:
			return TEXT("Transform");
		case ESRFacilityOperationKind::Synthesize:
			return TEXT("Synthesis");
		case ESRFacilityOperationKind::Separate:
			return TEXT("Separation");
		case ESRFacilityOperationKind::Mine:
			return TEXT("Mining");
		default:
			return TEXT("Invalid");
		}
	}
}

bool FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
	const USRFacilityDataAsset* FacilityDataAsset,
	const TArray<FSRResourceInstance>& InputResources,
	ESRFacilityTemperatureState TemperatureState)
{
	(void)TemperatureState;
	if (!IsValid(FacilityDataAsset) || FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard)
	{
		return false;
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return InputResources.IsEmpty();
	}

	return FSRFacilityPatternOperatorExecutor::CanExecute(FacilityDataAsset, InputResources);
}

int32 FSRFacilityOutputResourceBuilder::CountProducedOutputResources(
	const USRFacilityDataAsset* FacilityDataAsset)
{
	if (!IsValid(FacilityDataAsset) || FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard)
	{
		return 0;
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return 1;
	}

	return FSRFacilityPatternOperatorExecutor::ResolveExpectedOutputCount(FacilityDataAsset);
}

int32 FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	return CountProducedOutputResources(FacilityDataAsset) > 0
		&& !FacilityInstance.OutputPortInventories.IsEmpty()
		? 1
		: 0;
}

int32 FSRFacilityOutputResourceBuilder::ResolveRequiredOutputSlots(
	const FSRFacilityInstance& FacilityInstance)
{
	return CountProducedOutputResources(FacilityInstance.FacilityDataAsset.Get());
}

bool FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(
	const FSRFacilityInstance& FacilityInstance)
{
	(void)FacilityInstance;
	return false;
}

void FSRFacilityOutputResourceBuilder::BuildOutputResources(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	TArray<FSRResourceInstance>& OutOutputResources,
	int32* OutPrimaryOutputCount,
	FSRResourceInstance* OutBaselinePrimaryResource,
	TArray<FString>* OutOperationTraceTexts)
{
	ResetOutputArguments(
		OutOutputResources,
		OutPrimaryOutputCount,
		OutBaselinePrimaryResource,
		OutOperationTraceTexts);

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!FSRFacilityPatternOperatorExecutor::IsPatternOperation(FacilityDataAsset)
		|| !FSRFacilityPatternOperatorExecutor::TryBuildOutputResources(
			FacilityInstance,
			InputResources,
			OutOutputResources))
	{
		return;
	}

	if (OutPrimaryOutputCount)
	{
		*OutPrimaryOutputCount = OutOutputResources.IsEmpty() ? 0 : 1;
	}
	if (OutBaselinePrimaryResource && !InputResources.IsEmpty())
	{
		*OutBaselinePrimaryResource = InputResources[0];
	}
	if (OutOperationTraceTexts)
	{
		for (int32 OutputIndex = 0; OutputIndex < OutOutputResources.Num(); ++OutputIndex)
		{
			OutOperationTraceTexts->Add(FString::Printf(
				TEXT("Pattern %s output %d"),
				GetOperationLabel(FacilityDataAsset->OperationKind),
				OutputIndex + 1));
		}
	}
}

void FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	const FSRResourceInstance& PrimaryResource,
	TArray<FSRResourceInstance>& OutOutputResources,
	int32* OutPrimaryOutputCount,
	FSRResourceInstance* OutBaselinePrimaryResource,
	TArray<FString>* OutOperationTraceTexts)
{
	(void)InputResources;
	ResetOutputArguments(
		OutOutputResources,
		OutPrimaryOutputCount,
		OutBaselinePrimaryResource,
		OutOperationTraceTexts);

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset)
		|| FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine
		|| !StarRovers::PatternRouting::IsValidPatternPayload(PrimaryResource))
	{
		return;
	}

	FSRResourceInstance MinedOutput = PrimaryResource;
	MinedOutput.StackCount = 1;
	OutOutputResources.Add(MoveTemp(MinedOutput));
	if (OutPrimaryOutputCount)
	{
		*OutPrimaryOutputCount = 1;
	}
	if (OutBaselinePrimaryResource)
	{
		*OutBaselinePrimaryResource = PrimaryResource;
	}
	if (OutOperationTraceTexts)
	{
		OutOperationTraceTexts->Add(TEXT("Pattern mining source copy"));
	}
}
