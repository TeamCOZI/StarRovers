#include "Celestial/SRStar.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Components/PointLightComponent.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Simulation/SRRunModifierSubsystem.h"
#include "Utility/SRLog.h"

ASRStar::ASRStar()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	BodyCategory = ESRCelestialBodyCategory::Star;
	StarPointLightIntensity = 100.0f;
	StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	StarPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StarPointLight"));
	StarPointLight->SetupAttachment(SceneRoot);
	StarPointLight->SetMobility(EComponentMobility::Movable);
	StarPointLight->SetVisibility(true);
	StarPointLight->SetUseInverseSquaredFalloff(false);
}

void ASRStar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceStellarHealthTimer(DeltaSeconds);
}

void ASRStar::BeginPlay()
{
	Super::BeginPlay();
	BindToTimeControlSubsystem();
	BindToRunModifierSubsystem();
	SnapshotContractRunModifierContext();
}

void ASRStar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromRunModifierSubsystem();
	UnbindFromTimeControlSubsystem();
	Super::EndPlay(EndPlayReason);
}

void ASRStar::SetData(const FSRCelestialBodyData& NewData)
{
	StarPointLightIntensity = NewData.StarPointLightIntensity;
	StarPointLightColor = NewData.StarPointLightColor;

	FString ContractFailureReason;
	if (FSRStellarPatternContractResolver::ValidateContract(NewData.StellarPatternContract, ContractFailureReason))
	{
		ActiveStellarPatternContract = NewData.StellarPatternContract;
	}
	else
	{
		ActiveStellarPatternContract = FSRStellarPatternContract();
		SR_LOG(Celestial,
			LogTemp,
			Warning,
			TEXT("Star '%s' received an invalid Pattern contract and will use the deterministic default: %s"),
			*GetName(),
			*ContractFailureReason);
	}

	StellarEvolutionStage = ESRStellarEvolutionStage::MainSequence;
	bSupernovaGameOver = false;
	ResetContractRuntimeState();

	Super::SetData(NewData);
	ApplyStarAppearance();
}

void ASRStar::ApplyData()
{
	Super::ApplyData();
	ApplyStarAppearance();
}

FSRCelestialBodyData ASRStar::GetData() const
{
	FSRCelestialBodyData CurrentData = Super::GetData();
	CurrentData.StarPointLightIntensity = StarPointLightIntensity;
	CurrentData.StarPointLightColor = StarPointLightColor;
	CurrentData.StellarPatternContract = ActiveStellarPatternContract;
	return CurrentData;
}

ESRStellarEvolutionStage ASRStar::GetStellarEvolutionStage() const
{
	return StellarEvolutionStage;
}

bool ASRStar::HasTriggeredSupernovaGameOver() const
{
	return bSupernovaGameOver;
}

bool ASRStar::SetStellarPatternContract(const FSRStellarPatternContract& NewContract)
{
	FString FailureReason;
	if (!FSRStellarPatternContractResolver::ValidateContract(NewContract, FailureReason))
	{
		SR_LOG(Celestial,
			LogTemp,
			Warning,
			TEXT("Star '%s' rejected invalid Pattern contract '%s': %s"),
			*GetName(),
			*NewContract.ContractId.ToString(),
			*FailureReason);
		return false;
	}

	ActiveStellarPatternContract = NewContract;
	const USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get();
	ResetContractRuntimeState(IsValid(TimeControlSubsystem) ? TimeControlSubsystem->GetCurrentCycleIndex() : ActiveContractCycleIndex);
	SR_LOG(Celestial,
		LogTemp,
		Display,
		TEXT("Star '%s' activated Pattern contract '%s' at cycle %d."),
		*GetName(),
		*ActiveStellarPatternContract.ContractId.ToString(),
		ActiveContractCycleIndex);
	return true;
}

FSRStellarPatternContract ASRStar::GetStellarPatternContract() const
{
	return ActiveStellarPatternContract;
}

FSRStellarContractState ASRStar::GetStellarContractState() const
{
	FSRStellarContractState State;
	State.EvolutionStage = StellarEvolutionStage;
	State.ContractId = ActiveStellarPatternContract.ContractId;
	State.ActiveCycleIndex = ActiveContractCycleIndex;
	State.RequiredScoreThisCycle = FSRStellarPatternContractResolver::GetRequiredScoreForCycle(
		ActiveStellarPatternContract,
		ActiveContractCycleIndex,
		ResolveContractModifiers());
	State.CurrentCycleScore = CurrentCycleScore;
	State.CurrentCycleBaseScore = CurrentCycleBaseScore;
	State.CurrentCycleBonusScore = CurrentCycleBonusScore;
	State.SubmittedPatternCount = CurrentCycleSubmittedPatternCount;
	State.RejectedPatternCount = CurrentCycleRejectedPatternCount;
	State.CurrentStellarHealth = CurrentStellarHealth;
	State.MaximumStellarHealth = ActiveStellarPatternContract.StellarHealthMaximum;
	State.CurrentStellarHealthDecreasePerSecond =
		FSRStellarPatternContractResolver::GetStellarHealthDecreasePerSecondForPeriod(
			ActiveStellarPatternContract,
			ActiveContractCycleIndex,
			ResolveContractModifiers());
	State.StellarHealthDecreaseMultiplierPerPeriod =
		ActiveStellarPatternContract.StellarHealthDecreaseMultiplierPerPeriod;
	State.LastSettledHealthSecondIndex = LastSettledHealthSecondIndex;
	State.LastSecondStellarHealthDecrease = LastSecondStellarHealthDecrease;
	State.LastPatternStellarHealthRestored = LastPatternStellarHealthRestored;
	State.LastSubmission = LastPatternSubmission;
	State.LastCycleSettlement = LastContractCycleSettlement;
	State.bSupernovaGameOver = bSupernovaGameOver;
	return State;
}

void ASRStar::ExportSaveData(FSRStarSaveData& OutSaveData) const
{
	OutSaveData = FSRStarSaveData();
	OutSaveData.EvolutionStage = StellarEvolutionStage;
	OutSaveData.bSupernovaGameOver = bSupernovaGameOver;
	OutSaveData.ActiveContract = ActiveStellarPatternContract;
	OutSaveData.ActiveContractCycleIndex = ActiveContractCycleIndex;
	OutSaveData.CurrentCycleScore = CurrentCycleScore;
	OutSaveData.CurrentCycleBaseScore = CurrentCycleBaseScore;
	OutSaveData.CurrentCycleBonusScore = CurrentCycleBonusScore;
	OutSaveData.SubmittedPatternCount = CurrentCycleSubmittedPatternCount;
	OutSaveData.RejectedPatternCount = CurrentCycleRejectedPatternCount;
	OutSaveData.CurrentStellarHealth = CurrentStellarHealth;
	OutSaveData.LastSettledHealthSecondIndex = LastSettledHealthSecondIndex;
	OutSaveData.StellarHealthSecondAccumulator = StellarHealthSecondAccumulator;
	OutSaveData.LastSecondStellarHealthDecrease = LastSecondStellarHealthDecrease;
	OutSaveData.LastPatternStellarHealthRestored = LastPatternStellarHealthRestored;
	OutSaveData.LastPatternSubmission = LastPatternSubmission;
	OutSaveData.LastCycleSettlement = LastContractCycleSettlement;
	OutSaveData.ActiveContractRunModifierContext = ActiveContractRunModifierContext;
}

bool ASRStar::CanImportSaveData(const FSRStarSaveData& SaveData, FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!StarRovers::Save::Star::IsSupportedVersion(SaveData.Version))
	{
		OutFailureReason = FString::Printf(TEXT("Unsupported star save version %d."), SaveData.Version);
		return false;
	}
	if (!StaticEnum<ESRStellarEvolutionStage>()->IsValidEnumValue(static_cast<int64>(SaveData.EvolutionStage)))
	{
		OutFailureReason = TEXT("Saved stellar evolution stage is invalid.");
		return false;
	}
	if ((SaveData.EvolutionStage == ESRStellarEvolutionStage::Supernova) != SaveData.bSupernovaGameOver)
	{
		OutFailureReason = TEXT("Supernova stage and game-over state disagree.");
		return false;
	}
	if (!FSRStellarPatternContractResolver::ValidateContract(SaveData.ActiveContract, OutFailureReason))
	{
		return false;
	}
	if (SaveData.ActiveContractCycleIndex < 0
		|| SaveData.CurrentCycleScore < 0
		|| SaveData.CurrentCycleBaseScore < 0
		|| SaveData.CurrentCycleBonusScore < 0
		|| SaveData.SubmittedPatternCount < 0
		|| SaveData.RejectedPatternCount < 0
		|| SaveData.LastSettledHealthSecondIndex < 0)
	{
		OutFailureReason = TEXT("Star Cycle, score, and health-second counters cannot be negative.");
		return false;
	}
	const int64 ExpectedScore = SaveData.CurrentCycleBonusScore > MAX_int64 - SaveData.CurrentCycleBaseScore
		? MAX_int64
		: SaveData.CurrentCycleBaseScore + SaveData.CurrentCycleBonusScore;
	if (SaveData.CurrentCycleScore != ExpectedScore)
	{
		OutFailureReason = TEXT("Star total score does not equal its saved base and bonus score.");
		return false;
	}
	if (!FMath::IsFinite(SaveData.CurrentStellarHealth)
		|| SaveData.CurrentStellarHealth < 0.0
		|| SaveData.CurrentStellarHealth > SaveData.ActiveContract.StellarHealthMaximum)
	{
		OutFailureReason = TEXT("Saved stellar health is outside the active contract bounds.");
		return false;
	}
	if (SaveData.bSupernovaGameOver != (SaveData.CurrentStellarHealth <= UE_DOUBLE_SMALL_NUMBER))
	{
		OutFailureReason = TEXT("Saved stellar health and game-over state disagree.");
		return false;
	}
	if (!FMath::IsFinite(SaveData.StellarHealthSecondAccumulator)
		|| SaveData.StellarHealthSecondAccumulator < 0.0
		|| SaveData.StellarHealthSecondAccumulator >= 1.0
		|| !FMath::IsFinite(SaveData.LastSecondStellarHealthDecrease)
		|| SaveData.LastSecondStellarHealthDecrease < 0.0
		|| !FMath::IsFinite(SaveData.LastPatternStellarHealthRestored)
		|| SaveData.LastPatternStellarHealthRestored < 0.0)
	{
		OutFailureReason = TEXT("Saved stellar-health timer and transfer values must be finite and non-negative.");
		return false;
	}
	for (const FSRPatternHandMatch& Match : SaveData.LastPatternSubmission.HandMatches)
	{
		if (Match.RuleId.IsNone() || Match.BonusScore < 0 || !Match.MatchedCells.IsCanonical())
		{
			OutFailureReason = TEXT("Last stellar submission contains a malformed bonus-hand match.");
			return false;
		}
	}
	FSRRunModifierContext CanonicalContext;
	if (!FSRRunModifierResolver::BuildContext(
		SaveData.ActiveContractRunModifierContext.ActiveSources,
		SaveData.ActiveContractRunModifierContext.Revision,
		CanonicalContext,
		OutFailureReason))
	{
		return false;
	}
	return true;
}

bool ASRStar::ImportSaveData(const FSRStarSaveData& SaveData)
{
	FString FailureReason;
	if (!CanImportSaveData(SaveData, FailureReason))
	{
		SR_LOG(Celestial, LogTemp, Error, TEXT("Star '%s' save import rejected: %s"), *GetName(), *FailureReason);
		return false;
	}

	FSRRunModifierContext CanonicalContext;
	FSRRunModifierResolver::BuildContext(
		SaveData.ActiveContractRunModifierContext.ActiveSources,
		SaveData.ActiveContractRunModifierContext.Revision,
		CanonicalContext,
		FailureReason);
	StellarEvolutionStage = SaveData.EvolutionStage;
	bSupernovaGameOver = SaveData.bSupernovaGameOver;
	ActiveStellarPatternContract = SaveData.ActiveContract;
	ActiveContractCycleIndex = SaveData.ActiveContractCycleIndex;
	CurrentCycleScore = SaveData.CurrentCycleScore;
	CurrentCycleBaseScore = SaveData.CurrentCycleBaseScore;
	CurrentCycleBonusScore = SaveData.CurrentCycleBonusScore;
	CurrentCycleSubmittedPatternCount = SaveData.SubmittedPatternCount;
	CurrentCycleRejectedPatternCount = SaveData.RejectedPatternCount;
	CurrentStellarHealth = SaveData.CurrentStellarHealth;
	LastSettledHealthSecondIndex = SaveData.LastSettledHealthSecondIndex;
	StellarHealthSecondAccumulator = SaveData.StellarHealthSecondAccumulator;
	LastSecondStellarHealthDecrease = SaveData.LastSecondStellarHealthDecrease;
	LastPatternStellarHealthRestored = SaveData.LastPatternStellarHealthRestored;
	LastPatternSubmission = SaveData.LastPatternSubmission;
	LastContractCycleSettlement = SaveData.LastCycleSettlement;
	ActiveContractRunModifierContext = MoveTemp(CanonicalContext);
	ApplyStarAppearance();
	return true;
}

FSRStellarPatternScoreResult ASRStar::PreviewStellarPatternSubmission(
	const FSRResourceInstance& ResourceInstance) const
{
	FSRStellarPatternScoreResult Result = FSRStellarPatternContractResolver::ScorePattern(
		ResourceInstance.Pattern,
		ResourceInstance.StackCount,
		ActiveStellarPatternContract,
		ResolveContractModifiers());
	if (!StarRovers::PatternRouting::IsValidPatternPayload(ResourceInstance))
	{
		Result.bPatternValid = false;
		Result.bMatchesDemand = false;
		Result.BaseScorePerPattern = 0;
		Result.BonusScorePerPattern = 0;
		Result.ScorePerPattern = 0;
		Result.TotalScore = 0;
		Result.HandMatches.Reset();
		Result.FailureReason = TEXT("Stellar cargo requires a resource ID, positive stack count, and non-empty canonical Pattern.");
		return Result;
	}
	if (bSupernovaGameOver || StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		Result.bMatchesDemand = false;
		Result.BaseScorePerPattern = 0;
		Result.BonusScorePerPattern = 0;
		Result.ScorePerPattern = 0;
		Result.TotalScore = 0;
		Result.HandMatches.Reset();
		Result.FailureReason = TEXT("A supernova star cannot accept Pattern submissions.");
	}
	return Result;
}

bool ASRStar::CanAcceptStellarFuelResource(const FSRResourceInstance& ResourceInstance) const
{
	const FSRStellarPatternScoreResult Result = PreviewStellarPatternSubmission(ResourceInstance);
	return Result.bContractValid
		&& Result.bPatternValid
		&& Result.bMatchesDemand
		&& Result.TotalScore > 0;
}

bool ASRStar::SubmitStellarPatternResource(
	const FSRResourceInstance& ResourceInstance,
	FSRStellarPatternScoreResult& OutScoreResult)
{
	OutScoreResult = PreviewStellarPatternSubmission(ResourceInstance);
	LastPatternSubmission = OutScoreResult;
	const bool bAccepted = OutScoreResult.bContractValid
		&& OutScoreResult.bPatternValid
		&& OutScoreResult.bMatchesDemand
		&& OutScoreResult.TotalScore > 0;
	if (!bAccepted)
	{
		LastPatternStellarHealthRestored = 0.0;
		CurrentCycleRejectedPatternCount = SaturatingAddScore(
			CurrentCycleRejectedPatternCount,
			FMath::Max(1, ResourceInstance.StackCount));
		OnStellarPatternSubmitted.Broadcast(OutScoreResult);
		return false;
	}

	const int64 BaseScore = static_cast<int64>(OutScoreResult.BaseScorePerPattern)
		* static_cast<int64>(OutScoreResult.StackCount);
	const int64 BonusScore = OutScoreResult.BonusScorePerPattern > 0
		&& OutScoreResult.StackCount > 0
		&& OutScoreResult.BonusScorePerPattern > MAX_int64 / static_cast<int64>(OutScoreResult.StackCount)
		? MAX_int64
		: OutScoreResult.BonusScorePerPattern * static_cast<int64>(OutScoreResult.StackCount);
	CurrentCycleBaseScore = SaturatingAddScore(CurrentCycleBaseScore, BaseScore);
	CurrentCycleBonusScore = SaturatingAddScore(CurrentCycleBonusScore, BonusScore);
	CurrentCycleScore = SaturatingAddScore(CurrentCycleScore, OutScoreResult.TotalScore);
	CurrentCycleSubmittedPatternCount = SaturatingAddScore(
		CurrentCycleSubmittedPatternCount,
		OutScoreResult.StackCount);
	const double HealthBeforeSubmission = CurrentStellarHealth;
	const double RequestedHealthRestoration =
		FSRStellarPatternContractResolver::GetStellarHealthRestorationForScore(
			ActiveStellarPatternContract,
			OutScoreResult.TotalScore,
			ResolveContractModifiers());
	CurrentStellarHealth = FMath::Clamp(
		HealthBeforeSubmission + RequestedHealthRestoration,
		0.0,
		ActiveStellarPatternContract.StellarHealthMaximum);
	LastPatternStellarHealthRestored = CurrentStellarHealth - HealthBeforeSubmission;
	OnStellarPatternSubmitted.Broadcast(OutScoreResult);
	return true;
}

void ASRStar::SetCurrentStellarHealth(double NewHealth)
{
	CurrentStellarHealth = FMath::Clamp(
		FMath::IsFinite(NewHealth) ? NewHealth : 0.0,
		0.0,
		ActiveStellarPatternContract.StellarHealthMaximum);
	if (CurrentStellarHealth <= UE_DOUBLE_SMALL_NUMBER)
	{
		AdvanceStellarEvolutionStage();
	}
}

void ASRStar::SettleStellarContractCycle(int32 NewCurrentCycleIndex)
{
	if (NewCurrentCycleIndex <= ActiveContractCycleIndex
		|| bSupernovaGameOver
		|| StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		return;
	}

	while (ActiveContractCycleIndex < NewCurrentCycleIndex
		&& !bSupernovaGameOver
		&& StellarEvolutionStage != ESRStellarEvolutionStage::Supernova)
	{
		SettleOneContractCycle();
	}
}

bool ASRStar::DebugSubmitStellarPatternFromFacilityOutput(
	USRFacilityNetworkComponent* FacilityNetwork,
	FName OccupantId,
	FSRStellarPatternScoreResult& OutScoreResult,
	FSRResourceInstance& OutResourceInstance)
{
	OutScoreResult = FSRStellarPatternScoreResult();
	OutResourceInstance = FSRResourceInstance();
	if (!IsValid(FacilityNetwork) || OccupantId.IsNone())
	{
		return false;
	}

	FSRFacilityInstance FacilitySnapshot;
	if (!FacilityNetwork->GetFacilityInstance(OccupantId, FacilitySnapshot)
		|| FacilitySnapshot.OutputInventory.IsEmpty())
	{
		return false;
	}

	const FSRResourceInstance CandidateResource = FacilitySnapshot.OutputInventory[0];
	OutScoreResult = PreviewStellarPatternSubmission(CandidateResource);
	if (!OutScoreResult.bMatchesDemand)
	{
		OutResourceInstance = CandidateResource;
		return false;
	}

	FSRResourceInstance ExtractedResource;
	if (!FacilityNetwork->ExtractOutputResource(OccupantId, ExtractedResource))
	{
		return false;
	}

	if (!SubmitStellarPatternResource(ExtractedResource, OutScoreResult))
	{
		return false;
	}

	OutResourceInstance = ExtractedResource;
	return true;
}

void ASRStar::HandleGameCycleAdvanced(int32 CurrentCycleIndex)
{
	SettleStellarContractCycle(CurrentCycleIndex);
}

void ASRStar::HandleRunModifierContextChanged(const FSRRunModifierContext& Context)
{
	// A contract Cycle never mixes score formulae. Boundary changes (including an
	// Augment chosen while the simulation is paused) may update an empty Cycle;
	// otherwise the new context starts after settlement.
	if (CurrentCycleScore == 0
		&& CurrentCycleSubmittedPatternCount == 0
		&& CurrentCycleRejectedPatternCount == 0)
	{
		ActiveContractRunModifierContext = Context;
	}
}

void ASRStar::BindToTimeControlSubsystem()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	USRTimeControlSubsystem* TimeControlSubsystem = World->GetSubsystem<USRTimeControlSubsystem>();
	if (!IsValid(TimeControlSubsystem))
	{
		return;
	}

	TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &ASRStar::HandleGameCycleAdvanced);
	TimeControlSubsystem->OnGameCycleAdvanced.AddDynamic(this, &ASRStar::HandleGameCycleAdvanced);
	BoundTimeControlSubsystem = TimeControlSubsystem;
	ResetContractRuntimeState(TimeControlSubsystem->GetCurrentCycleIndex());
}

void ASRStar::UnbindFromTimeControlSubsystem()
{
	if (USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get())
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &ASRStar::HandleGameCycleAdvanced);
	}
	BoundTimeControlSubsystem.Reset();
}

void ASRStar::BindToRunModifierSubsystem()
{
	USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	if (IsValid(RunModifierSubsystem))
	{
		RunModifierSubsystem->OnRunModifierContextChanged.RemoveDynamic(this, &ASRStar::HandleRunModifierContextChanged);
		RunModifierSubsystem->OnRunModifierContextChanged.AddDynamic(this, &ASRStar::HandleRunModifierContextChanged);
	}
}

void ASRStar::UnbindFromRunModifierSubsystem()
{
	USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	if (IsValid(RunModifierSubsystem))
	{
		RunModifierSubsystem->OnRunModifierContextChanged.RemoveDynamic(this, &ASRStar::HandleRunModifierContextChanged);
	}
}

void ASRStar::SnapshotContractRunModifierContext()
{
	ActiveContractRunModifierContext = USRRunModifierSubsystem::GetContextForObject(this);
}

FSRStellarPatternContractModifiers ASRStar::ResolveContractModifiers() const
{
	FSRRunModifierQuery Query;
	Query.ContractId = ActiveStellarPatternContract.ContractId;
	const FSRResolvedRunModifiers RunModifiers = FSRRunModifierResolver::Resolve(
		ActiveContractRunModifierContext,
		Query);
	FSRStellarPatternContractModifiers Result;
	Result.BaseScoreMultiplier = RunModifiers.StellarBaseScoreMultiplier;
	Result.BonusScoreMultiplier = RunModifiers.StellarBonusScoreMultiplier;
	Result.RequiredScoreMultiplier = RunModifiers.StellarRequiredScoreMultiplier;
	Result.HealthDamageMultiplier = RunModifiers.StellarHealthDamageMultiplier;
	Result.HealthRecoveryMultiplier = RunModifiers.StellarHealthRecoveryMultiplier;
	return Result;
}

void ASRStar::ApplyStarAppearance()
{
	StarPointLightIntensity = FMath::Max(0.0f, StarPointLightIntensity);
	if (UPointLightComponent* ActiveStarPointLight = StarPointLight)
	{
		ActiveStarPointLight->SetVisibility(true);
		ActiveStarPointLight->SetUseInverseSquaredFalloff(false);
		ActiveStarPointLight->SetIntensityUnits(ELightUnits::Candelas);
		ActiveStarPointLight->SetIntensity(StarPointLightIntensity);
		ActiveStarPointLight->SetLightColor(StarPointLightColor);
	}
}

float ASRStar::GetEffectiveStellarHealthDeltaSeconds(float DeltaSeconds) const
{
	const float ClampedDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	if (const USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get())
	{
		return ClampedDeltaSeconds * FMath::Max(0.0f, TimeControlSubsystem->GetEffectiveTimeScale());
	}
	return ClampedDeltaSeconds;
}

void ASRStar::AdvanceStellarHealthTimer(float DeltaSeconds)
{
	if (bSupernovaGameOver || StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		return;
	}

	StellarHealthSecondAccumulator += static_cast<double>(GetEffectiveStellarHealthDeltaSeconds(DeltaSeconds));
	const int32 SecondsToSettle = FMath::FloorToInt(StellarHealthSecondAccumulator);
	if (SecondsToSettle <= 0)
	{
		return;
	}

	StellarHealthSecondAccumulator -= static_cast<double>(SecondsToSettle);
	for (int32 SecondIndex = 0; SecondIndex < SecondsToSettle; ++SecondIndex)
	{
		SettleStellarHealthSecond();
		if (bSupernovaGameOver || StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
		{
			StellarHealthSecondAccumulator = 0.0;
			return;
		}
	}
}

void ASRStar::SettleStellarHealthSecond()
{
	if (bSupernovaGameOver || StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		return;
	}

	LastSecondStellarHealthDecrease =
		FSRStellarPatternContractResolver::GetStellarHealthDecreasePerSecondForPeriod(
			ActiveStellarPatternContract,
			ActiveContractCycleIndex,
			ResolveContractModifiers());
	CurrentStellarHealth = FMath::Max(0.0, CurrentStellarHealth - LastSecondStellarHealthDecrease);
	++LastSettledHealthSecondIndex;
	if (CurrentStellarHealth <= UE_DOUBLE_SMALL_NUMBER)
	{
		AdvanceStellarEvolutionStage();
	}
}

void ASRStar::ResetContractRuntimeState(int32 NewActiveCycleIndex)
{
	ActiveContractCycleIndex = FMath::Max(0, NewActiveCycleIndex);
	CurrentCycleScore = 0;
	CurrentCycleBaseScore = 0;
	CurrentCycleBonusScore = 0;
	CurrentCycleSubmittedPatternCount = 0;
	CurrentCycleRejectedPatternCount = 0;
	CurrentStellarHealth = FMath::Clamp(
		ActiveStellarPatternContract.StartingStellarHealth,
		0.0,
		ActiveStellarPatternContract.StellarHealthMaximum);
	LastSettledHealthSecondIndex = 0;
	StellarHealthSecondAccumulator = 0.0;
	LastSecondStellarHealthDecrease = 0.0;
	LastPatternStellarHealthRestored = 0.0;
	LastPatternSubmission = FSRStellarPatternScoreResult();
	LastContractCycleSettlement = FSRStellarContractCycleSettlement();
	SnapshotContractRunModifierContext();
}

void ASRStar::SettleOneContractCycle()
{
	FSRStellarContractCycleSettlement Settlement = FSRStellarPatternContractResolver::SettleCycle(
		ActiveStellarPatternContract,
		ActiveContractCycleIndex,
		CurrentCycleScore,
		ResolveContractModifiers());
	if (!Settlement.bValid)
	{
		SR_LOG(Celestial,
			LogTemp,
			Error,
			TEXT("Star '%s' could not settle Pattern contract cycle %d: %s"),
			*GetName(),
			ActiveContractCycleIndex,
			*Settlement.FailureReason);
		++ActiveContractCycleIndex;
		return;
	}

	LastContractCycleSettlement = Settlement;
	SR_LOG(Celestial,
		LogTemp,
		Display,
		TEXT("Star '%s' settled Pattern contract '%s': Period=%d Score=%lld/%lld Missing=%lld Surplus=%lld Health=%.3f DrainRate=%.3f/s"),
		*GetName(),
		*ActiveStellarPatternContract.ContractId.ToString(),
		Settlement.CycleIndex,
		static_cast<long long>(Settlement.SubmittedScore),
		static_cast<long long>(Settlement.RequiredScore),
		static_cast<long long>(Settlement.MissingScore),
		static_cast<long long>(Settlement.SurplusScore),
		CurrentStellarHealth,
		FSRStellarPatternContractResolver::GetStellarHealthDecreasePerSecondForPeriod(
			ActiveStellarPatternContract,
			ActiveContractCycleIndex,
			ResolveContractModifiers()));

	++ActiveContractCycleIndex;
	CurrentCycleScore = 0;
	CurrentCycleBaseScore = 0;
	CurrentCycleBonusScore = 0;
	CurrentCycleSubmittedPatternCount = 0;
	CurrentCycleRejectedPatternCount = 0;
	SnapshotContractRunModifierContext();

	OnStellarContractCycleSettled.Broadcast(LastContractCycleSettlement);
}

void ASRStar::SetStellarEvolutionStage(ESRStellarEvolutionStage NewStage)
{
	if (StellarEvolutionStage == NewStage)
	{
		return;
	}

	const ESRStellarEvolutionStage PreviousStage = StellarEvolutionStage;
	StellarEvolutionStage = NewStage;
	OnStellarEvolutionStageChanged.Broadcast(PreviousStage, StellarEvolutionStage);
}

void ASRStar::AdvanceStellarEvolutionStage()
{
	if (StellarEvolutionStage == ESRStellarEvolutionStage::MainSequence)
	{
		SetStellarEvolutionStage(ESRStellarEvolutionStage::RedGiant);
		CurrentStellarHealth = FMath::Clamp(
			ActiveStellarPatternContract.StartingStellarHealth,
			0.0,
			ActiveStellarPatternContract.StellarHealthMaximum);
		SR_LOG(Celestial,
			LogTemp,
			Warning,
			TEXT("Star '%s' exhausted its main-sequence health at simulated health second %lld and evolved into a red giant with %.2f / %.2f health."),
			*GetName(),
			static_cast<long long>(LastSettledHealthSecondIndex),
			CurrentStellarHealth,
			ActiveStellarPatternContract.StellarHealthMaximum);
		return;
	}

	if (StellarEvolutionStage == ESRStellarEvolutionStage::RedGiant)
	{
		CurrentStellarHealth = 0.0;
		SetStellarEvolutionStage(ESRStellarEvolutionStage::Supernova);
		TriggerSupernovaGameOver();
	}
}

void ASRStar::TriggerSupernovaGameOver()
{
	if (bSupernovaGameOver)
	{
		return;
	}

	bSupernovaGameOver = true;
	if (USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get())
	{
		TimeControlSubsystem->PauseSimulation();
	}

	SR_LOG(Celestial,
		LogTemp,
		Error,
		TEXT("Star '%s' exhausted its red-giant health at simulated health second %lld. Supernova game over."),
		*GetName(),
		static_cast<long long>(LastSettledHealthSecondIndex));
	OnStellarSupernovaGameOver.Broadcast(this);
}

int64 ASRStar::SaturatingAddScore(int64 Left, int64 Right)
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
