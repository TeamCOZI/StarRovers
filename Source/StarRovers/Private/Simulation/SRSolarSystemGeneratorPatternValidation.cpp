#include "Simulation/SRSolarSystemGenerator.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRStar.h"
#include "Pattern/SRPatternGenerationProfileDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Utility/SRLog.h"

namespace StarRovers::Simulation::PatternGeneration::Private
{
	FName MakeBodyId(const ASRCelestialBody* Body)
	{
		return IsValid(Body)
			? FName(*Body->GetPathName())
			: NAME_None;
	}

	void AddFacilityOperators(
		const USRPatternGenerationProfileDataAsset& Profile,
		FSRPatternGenerationBodySpec& InOutBodySpec)
	{
		for (const TObjectPtr<USRFacilityDataAsset>& FacilityDataAssetPointer : Profile.AvailableFacilityDataAssets)
		{
			const USRFacilityDataAsset* FacilityDataAsset = FacilityDataAssetPointer.Get();
			if (!IsValid(FacilityDataAsset)
				|| FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard)
			{
				continue;
			}

			switch (FacilityDataAsset->OperationKind)
			{
			case ESRFacilityOperationKind::Process:
				InOutBodySpec.TransformOperators.Add(FacilityDataAsset->TransformOperator);
				break;
			case ESRFacilityOperationKind::Synthesize:
				InOutBodySpec.bAllowSynthesis |= IsValid(FacilityDataAsset->SynthesisOutputResource.Get());
				break;
			case ESRFacilityOperationKind::Separate:
				InOutBodySpec.SeparationOperators.Add(FacilityDataAsset->SeparationOperator);
				break;
			case ESRFacilityOperationKind::Mine:
			default:
				break;
			}
		}
	}

	void AppendBodyAndSources(
		ASRCelestialBody* Body,
		const USRPatternGenerationProfileDataAsset& Profile,
		FSRPatternGenerationValidationRequest& InOutRequest)
	{
		if (!IsValid(Body))
		{
			return;
		}

		FSRPatternGenerationBodySpec BodySpec;
		BodySpec.BodyId = MakeBodyId(Body);
		BodySpec.Environment = Body->GetData().PatternEnvironment;
		AddFacilityOperators(Profile, BodySpec);
		InOutRequest.Bodies.Add(MoveTemp(BodySpec));

		USRStructureInstanceManagerComponent* StructureManager =
			Body->FindComponentByClass<USRStructureInstanceManagerComponent>();
		if (!IsValid(StructureManager))
		{
			return;
		}

		TArray<FSRPlacedStructureInstance> PlacedStructures;
		StructureManager->GetPlacedStructures(PlacedStructures);
		TArray<FSRResourceDepositInstance> ResourceDeposits;
		for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
		{
			FSRResourceDepositInstance ResourceDeposit;
			if (StructureManager->GetResourceDepositInstance(
				PlacedStructure.OccupantId,
				ResourceDeposit))
			{
				ResourceDeposits.Add(MoveTemp(ResourceDeposit));
			}
		}
		ResourceDeposits.Sort([](
			const FSRResourceDepositInstance& Left,
			const FSRResourceDepositInstance& Right)
		{
			return Left.SourcePatternId.LexicalLess(Right.SourcePatternId);
		});

		const FName BodyId = MakeBodyId(Body);
		TMap<FName, int32> SourceCountByResourceId;
		for (const FSRResourceDepositInstance& ResourceDeposit : ResourceDeposits)
		{
			int32& SourceCount = SourceCountByResourceId.FindOrAdd(ResourceDeposit.ResourceId);
			if (SourceCount >= Profile.MaxValidationSourcesPerResourcePerBody)
			{
				continue;
			}
			++SourceCount;

			FSRPatternGenerationSourceSpec SourceSpec;
			SourceSpec.SourceId = ResourceDeposit.SourcePatternId;
			SourceSpec.BodyId = BodyId;
			SourceSpec.Pattern = ResourceDeposit.SourcePattern;
			InOutRequest.Sources.Add(MoveTemp(SourceSpec));
		}
	}

	uint32 BuildContractSelectionHash(const FSRPatternGenerationValidationRequest& Request)
	{
		uint32 Hash = 2166136261u;
		for (const FSRPatternGenerationSourceSpec& Source : Request.Sources)
		{
			Hash = (Hash ^ Source.Pattern.GetStableHash()) * 16777619u;
		}
		return Hash;
	}
}

bool ASRSolarSystemGenerator::GetLastPatternGenerationValidationResult(
	FSRPatternGenerationValidationResult& OutValidationResult) const
{
	OutValidationResult = bHasPatternGenerationValidationResult
		? LastPatternGenerationValidationResult
		: FSRPatternGenerationValidationResult();
	return bHasPatternGenerationValidationResult;
}

void ASRSolarSystemGenerator::ValidateRuntimePatternGeneration(int32 RuntimeGenerationSeed)
{
	using namespace StarRovers::Simulation::PatternGeneration::Private;

	bHasPatternGenerationValidationResult = false;
	LastPatternGenerationValidationResult = FSRPatternGenerationValidationResult();
	const USRPatternGenerationProfileDataAsset* Profile = PatternGenerationProfileDataAsset.Get();
	if (!IsValid(Profile))
	{
		return;
	}

	FSRPatternGenerationValidationRequest Request;
	Request.MaxOperationDepth = Profile->MaxOperationDepth;
	Request.MaxReachableStates = Profile->MaxReachableStates;
	Request.bRequireInterBodyTransfer = Profile->bRequireInterBodyTransfer;
	for (TObjectPtr<ASRCelestialBody>& PlanetBody : RuntimePlanetBodies)
	{
		AppendBodyAndSources(PlanetBody.Get(), *Profile, Request);
	}
	for (TObjectPtr<ASRCelestialBody>& MoonBody : RuntimeMoonBodies)
	{
		AppendBodyAndSources(MoonBody.Get(), *Profile, Request);
	}

	ASRStar* RuntimeStar = Cast<ASRStar>(RuntimeStarBody.Get());
	if (!IsValid(RuntimeStar))
	{
		LastPatternGenerationValidationResult.bValidationPerformed = true;
		LastPatternGenerationValidationResult.bSolvable = false;
		LastPatternGenerationValidationResult.Failure = ESRPatternGenerationValidationFailure::InvalidRequest;
		LastPatternGenerationValidationResult.CandidateContractCount = Profile->CandidateStellarContracts.Num();
		bHasPatternGenerationValidationResult = true;
		SR_LOG(SolarSystem,
			LogTemp,
			Error,
			TEXT("Pattern generation validation requires the generated primary star class to derive from ASRStar."));
		return;
	}

	const TArray<FSRStellarPatternContract>& CandidateContracts = Profile->CandidateStellarContracts;

	const int32 CandidateCount = CandidateContracts.Num();
	const uint32 GeneratedSystemSignature = BuildContractSelectionHash(Request);
	TArray<int32> CandidateOrder;
	StarRovers::PatternGeneration::BuildSeededCandidateOrder(
		CandidateCount,
		RuntimeGenerationSeed,
		GeneratedSystemSignature,
		CandidateOrder);
	SR_LOG(SolarSystem,
		LogTemp,
		Display,
		TEXT("Selecting a solvable stellar Pattern contract: RunSeed=%d Candidates=%d SystemSignature=%u"),
		RuntimeGenerationSeed,
		CandidateCount,
		GeneratedSystemSignature);
	FSRPatternGenerationValidationResult FirstAttemptResult;
	bool bHasFirstAttemptResult = false;
	FSRStellarPatternContract FirstValidContract;
	bool bHasFirstValidContract = false;
	TSet<FName> AttemptedContractIds;
	int32 AttemptedContractCount = 0;
	for (const int32 CandidateIndex : CandidateOrder)
	{
		const FSRStellarPatternContract& CandidateContract = CandidateContracts[CandidateIndex];
		SR_LOG(SolarSystem,
			LogTemp,
			Display,
			TEXT("Trying Pattern contract candidate: Attempt=%d/%d CandidateIndex=%d Contract=%s"),
			AttemptedContractCount + 1,
			CandidateCount,
			CandidateIndex,
			*CandidateContract.ContractId.ToString());
		FString ContractFailureReason;
		if (AttemptedContractIds.Contains(CandidateContract.ContractId)
			|| !FSRStellarPatternContractResolver::ValidateContract(CandidateContract, ContractFailureReason))
		{
			SR_LOG(SolarSystem,
				LogTemp,
				Warning,
				TEXT("Pattern contract candidate %d/%d was skipped: Contract=%s Reason=%s"),
				CandidateIndex + 1,
				CandidateCount,
				*CandidateContract.ContractId.ToString(),
				AttemptedContractIds.Contains(CandidateContract.ContractId)
					? TEXT("Duplicate ContractId")
					: *ContractFailureReason);
			continue;
		}

		AttemptedContractIds.Add(CandidateContract.ContractId);
		++AttemptedContractCount;
		if (!bHasFirstValidContract)
		{
			FirstValidContract = CandidateContract;
			bHasFirstValidContract = true;
		}

		Request.Goal.RequiredPattern = CandidateContract.RequiredPattern;
		Request.Goal.RequiredMask = CandidateContract.RequiredMask;
		FSRPatternGenerationValidationResult CandidateResult = FSRPatternGenerationValidator::Validate(Request);
		CandidateResult.SelectedContractId = CandidateContract.ContractId;
		CandidateResult.AttemptedContractCount = AttemptedContractCount;
		CandidateResult.CandidateContractCount = CandidateCount;
		if (CandidateResult.bSolvable && !RuntimeStar->SetStellarPatternContract(CandidateContract))
		{
			CandidateResult.bSolvable = false;
			CandidateResult.Failure = ESRPatternGenerationValidationFailure::InvalidRequest;
		}
		if (!bHasFirstAttemptResult)
		{
			FirstAttemptResult = CandidateResult;
			bHasFirstAttemptResult = true;
		}
		if (!CandidateResult.bSolvable)
		{
			SR_LOG(SolarSystem,
				LogTemp,
				Warning,
				TEXT("Pattern contract candidate failed: Contract=%s Failure=%d ExploredStates=%d"),
				*CandidateContract.ContractId.ToString(),
				static_cast<int32>(CandidateResult.Failure),
				CandidateResult.ExploredStateCount);
			continue;
		}

		LastPatternGenerationValidationResult = CandidateResult;
		break;
	}

	if (!LastPatternGenerationValidationResult.bSolvable)
	{
		LastPatternGenerationValidationResult = bHasFirstAttemptResult
			? FirstAttemptResult
			: FSRPatternGenerationValidationResult();
		LastPatternGenerationValidationResult.bValidationPerformed = true;
		LastPatternGenerationValidationResult.Failure = bHasFirstAttemptResult
			? LastPatternGenerationValidationResult.Failure
			: ESRPatternGenerationValidationFailure::InvalidRequest;
		LastPatternGenerationValidationResult.AttemptedContractCount = AttemptedContractCount;
		LastPatternGenerationValidationResult.CandidateContractCount = CandidateCount;
		if (bHasFirstValidContract)
		{
			RuntimeStar->SetStellarPatternContract(FirstValidContract);
			LastPatternGenerationValidationResult.SelectedContractId = FirstValidContract.ContractId;
		}
	}
	bHasPatternGenerationValidationResult = true;
	if (!LastPatternGenerationValidationResult.bSolvable)
	{
		SR_LOG(SolarSystem,
			LogTemp,
			Error,
			TEXT("Pattern generation validation failed: Failure=%d Contract=%s Attempts=%d/%d Sources=%d Bodies=%d ExploredStates=%d"),
			static_cast<int32>(LastPatternGenerationValidationResult.Failure),
			*LastPatternGenerationValidationResult.SelectedContractId.ToString(),
			LastPatternGenerationValidationResult.AttemptedContractCount,
			LastPatternGenerationValidationResult.CandidateContractCount,
			Request.Sources.Num(),
			Request.Bodies.Num(),
			LastPatternGenerationValidationResult.ExploredStateCount);
		for (const FSRPatternGenerationBodySpec& BodySpec : Request.Bodies)
		{
			SR_LOG(SolarSystem,
				LogTemp,
				Display,
				TEXT("Pattern validation body: Body=%s Environment=%s Transforms=%d Separators=%d Synthesis=%s"),
				*BodySpec.BodyId.ToString(),
				*BodySpec.Environment.EnvironmentId.ToString(),
				BodySpec.TransformOperators.Num(),
				BodySpec.SeparationOperators.Num(),
				BodySpec.bAllowSynthesis ? TEXT("true") : TEXT("false"));
		}
		for (const FSRPatternGenerationSourceSpec& SourceSpec : Request.Sources)
		{
			SR_LOG(SolarSystem,
				LogTemp,
				Display,
				TEXT("Pattern validation source: Source=%s Body=%s Pattern=%s"),
				*SourceSpec.SourceId.ToString(),
				*SourceSpec.BodyId.ToString(),
				*SourceSpec.Pattern.ToCompactString());
		}
		return;
	}

	SR_LOG(SolarSystem,
		LogTemp,
		Display,
		TEXT("Pattern generation validation succeeded: RunSeed=%d Contract=%s Attempts=%d/%d Sources=%d Bodies=%d Depth=%d GoalBody=%s InterBody=%s ExploredStates=%d"),
		RuntimeGenerationSeed,
		*LastPatternGenerationValidationResult.SelectedContractId.ToString(),
		LastPatternGenerationValidationResult.AttemptedContractCount,
		LastPatternGenerationValidationResult.CandidateContractCount,
		Request.Sources.Num(),
		Request.Bodies.Num(),
		LastPatternGenerationValidationResult.MinimumOperationDepth,
		*LastPatternGenerationValidationResult.GoalBodyId.ToString(),
		LastPatternGenerationValidationResult.bRequiresInterBodyTransfer ? TEXT("true") : TEXT("false"),
		LastPatternGenerationValidationResult.ExploredStateCount);
}
