#include "Celestial/SRStar.h"

#include "Utility/SRLog.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRStellarDemandModel.h"
#include "Simulation/SRTimeControlSubsystem.h"

namespace
{
	constexpr double StellarFuelIncomeWindowSeconds = 30.0;
}

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
	AdvanceStellarFuelTimer(DeltaSeconds);
}

void ASRStar::BeginPlay()
{
	Super::BeginPlay();

	BindToTimeControlSubsystem();
}

void ASRStar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromTimeControlSubsystem();

	Super::EndPlay(EndPlayReason);
}

void ASRStar::SetData(const FSRCelestialBodyData& NewData)
{
	StarPointLightIntensity = NewData.StarPointLightIntensity;
	StarPointLightColor = NewData.StarPointLightColor;
	const USRSimulationSettings* SimulationSettings = GetDefault<USRSimulationSettings>();
	bUsesStellarPressureCurveV2 = IsValid(SimulationSettings)
		&& SimulationSettings->bUseStellarPressureCurveV2;
	StellarDemandCurveV2 = IsValid(SimulationSettings)
		? SimulationSettings->BuildStellarDemandCurveV2()
		: FSRStellarDemandModel::SanitizeCurveV2(FSRStellarDemandCurveV2());
	StellarPressureRulesV2 = IsValid(SimulationSettings)
		? SimulationSettings->BuildStellarPressureRulesV2()
		: FSRStellarDemandModel::SanitizePressureRulesV2(FSRStellarPressureRulesV2());
	InitialStageStellarFuel = bUsesStellarPressureCurveV2
		? StellarPressureRulesV2.FuelReserveCapacity
		: FMath::Max(0.0, NewData.InitialStoredStellarFuel);
	StoredStellarFuel = InitialStageStellarFuel;
	InitialStellarFuelDecreasePerSecond = bUsesStellarPressureCurveV2
		? StellarDemandCurveV2.InitialDemandPerSecond
		: FMath::Max(0.0, NewData.InitialStellarFuelDecreasePerSecond);
	RequiredStellarFuelPerCycle = InitialStellarFuelDecreasePerSecond;
	StellarFuelRequirementGrowthPerCycle = bUsesStellarPressureCurveV2
		? FSRStellarDemandModel::CalculateNextCycleMultiplierV2(StellarDemandCurveV2, 0)
		: 0.0;
	LastFuelDecreaseRateCycleIndex = 0;
	RedGiantPressure = bUsesStellarPressureCurveV2
		? 0.0
		: FMath::Max(0.0, NewData.InitialRedGiantPressure);
	RedGiantPressurePerMissingFuel = bUsesStellarPressureCurveV2
		? 100.0 / StellarPressureRulesV2.FuelReserveCapacity
		: FMath::Max(0.0, NewData.RedGiantPressurePerMissingFuel);
	StellarEvolutionStage = ESRStellarEvolutionStage::MainSequence;
	bSupernovaGameOver = false;
	LastSettledSecondIndex = 0;
	LastSecondFuelConsumed = 0.0;
	LastSecondFuelDecrease = 0.0;
	LastSecondFuelDeficit = 0.0;
	bLastSecondSurvived = true;
	StellarFuelSecondAccumulator = 0.0f;
	StellarFuelElapsedSimulationSeconds = 0.0;
	LastFuelDeliverySimulationSeconds = -1.0;
	LastFuelDeliveryAmount = 0.0;
	LastFuelReserveGain = 0.0;
	LastFuelReserveOverflow = 0.0;
	TotalDeliveredFuel = 0.0;
	StellarFuelDeliverySamples.Reset();
	StellarRunContract = IsValid(SimulationSettings)
		? SimulationSettings->BuildStellarRunContractV2()
		: FSRStellarRunContractModel::Sanitize(FSRStellarRunContract());
	StellarRunProgress = FSRStellarRunContractModel::MakeInitialProgress(StellarRunContract);

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
	CurrentData.InitialStoredStellarFuel = StoredStellarFuel;
	CurrentData.InitialStellarFuelDecreasePerSecond = InitialStellarFuelDecreasePerSecond;
	CurrentData.RequiredStellarFuelPerCycle = RequiredStellarFuelPerCycle;
	CurrentData.StellarFuelRequirementGrowthPerCycle = StellarFuelRequirementGrowthPerCycle;
	CurrentData.InitialRedGiantPressure = RedGiantPressure;
	CurrentData.RedGiantPressurePerMissingFuel = RedGiantPressurePerMissingFuel;
	return CurrentData;
}

void ASRStar::AddStellarFuel(double FuelAmount)
{
	if (StellarEvolutionStage == ESRStellarEvolutionStage::Supernova
		|| StellarRunProgress.HasEnded())
	{
		return;
	}

	const double SafeFuelAmount = FMath::IsFinite(FuelAmount)
		? FMath::Max(0.0, FuelAmount)
		: 0.0;
	if (SafeFuelAmount <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const double PreviousStoredFuel = StoredStellarFuel;
	StoredStellarFuel = bUsesStellarPressureCurveV2
		? FSRStellarDemandModel::ClampFuelReserveV2(
			StoredStellarFuel + SafeFuelAmount,
			StellarPressureRulesV2)
		: FMath::Max(0.0, StoredStellarFuel + SafeFuelAmount);
	LastFuelReserveGain = FMath::Max(0.0, StoredStellarFuel - PreviousStoredFuel);
	LastFuelReserveOverflow = FMath::Max(0.0, SafeFuelAmount - LastFuelReserveGain);
	LastFuelDeliverySimulationSeconds = StellarFuelElapsedSimulationSeconds;
	LastFuelDeliveryAmount = SafeFuelAmount;
	TotalDeliveredFuel += SafeFuelAmount;

	FSRStellarFuelDeliverySample& DeliverySample = StellarFuelDeliverySamples.AddDefaulted_GetRef();
	DeliverySample.SimulationTimeSeconds = StellarFuelElapsedSimulationSeconds;
	DeliverySample.FuelAmount = SafeFuelAmount;
	PruneStellarFuelDeliverySamples();
	RefreshStellarPressureState();
	OnStellarFuelDelivered.Broadcast(SafeFuelAmount, TotalDeliveredFuel);
	RefreshStellarRunProgress(0.0);
}

ESRStellarEvolutionStage ASRStar::GetStellarEvolutionStage() const
{
	return StellarEvolutionStage;
}

bool ASRStar::HasTriggeredSupernovaGameOver() const
{
	return bSupernovaGameOver;
}

FSRStellarRunProgress ASRStar::GetStellarRunProgress() const
{
	return StellarRunProgress;
}

ESRStellarRunOutcome ASRStar::GetStellarRunOutcome() const
{
	return StellarRunProgress.Outcome;
}

bool ASRStar::HasStellarRunEnded() const
{
	return StellarRunProgress.HasEnded();
}

bool ASRStar::CanAcceptStellarFuelResource(const FSRResourceInstance& ResourceInstance) const
{
	return !StellarRunProgress.HasEnded()
		&& CalculateStellarFuelValueForResource(ResourceInstance) > UE_DOUBLE_SMALL_NUMBER;
}

double ASRStar::CalculateStellarFuelValueForResource(const FSRResourceInstance& ResourceInstance) const
{
	if (ResourceInstance.StackCount <= 0)
	{
		return 0.0;
	}

	const double FuelValue = ResourceInstance.EnergyValue * static_cast<double>(FMath::Max(1, ResourceInstance.StackCount));
	return FMath::Max(0.0, FuelValue);
}

bool ASRStar::DeliverStellarFuelResource(const FSRResourceInstance& ResourceInstance, double& OutFuelAmount)
{
	OutFuelAmount = CalculateStellarFuelValueForResource(ResourceInstance);
	if (StellarRunProgress.HasEnded() || OutFuelAmount <= UE_DOUBLE_SMALL_NUMBER)
	{
		OutFuelAmount = 0.0;
		return false;
	}

	AddStellarFuel(OutFuelAmount);
	return true;
}

bool ASRStar::DebugDeliverStellarFuelFromFacilityOutput(
	USRFacilityNetworkComponent* FacilityNetwork,
	FName OccupantId,
	double& OutFuelAmount,
	FSRResourceInstance& OutResourceInstance)
{
	OutFuelAmount = 0.0;
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
	if (!CanAcceptStellarFuelResource(CandidateResource))
	{
		OutResourceInstance = CandidateResource;
		return false;
	}

	FSRResourceInstance ExtractedResource;
	if (!FacilityNetwork->ExtractOutputResource(OccupantId, ExtractedResource))
	{
		return false;
	}

	if (!DeliverStellarFuelResource(ExtractedResource, OutFuelAmount))
	{
		return false;
	}

	OutResourceInstance = ExtractedResource;
	return true;
}

void ASRStar::SetStoredStellarFuel(double NewStoredFuel)
{
	StoredStellarFuel = bUsesStellarPressureCurveV2
		? FSRStellarDemandModel::ClampFuelReserveV2(NewStoredFuel, StellarPressureRulesV2)
		: FMath::Max(0.0, NewStoredFuel);
	RefreshStellarPressureState();
}

void ASRStar::SetStellarFuelRequirement(double NewRequiredFuelPerCycle, double NewRequirementGrowthPerCycle)
{
	RequiredStellarFuelPerCycle = FMath::Max(0.0, NewRequiredFuelPerCycle);
	InitialStellarFuelDecreasePerSecond = FMath::Max(0.0, NewRequiredFuelPerCycle);
	StellarFuelRequirementGrowthPerCycle = FMath::Max(0.0, NewRequirementGrowthPerCycle);
}

void ASRStar::SetRedGiantPressure(double NewRedGiantPressure)
{
	RedGiantPressure = bUsesStellarPressureCurveV2
		? FMath::Clamp(
			FMath::IsFinite(NewRedGiantPressure) ? NewRedGiantPressure : 0.0,
			0.0,
			100.0)
		: FMath::Max(0.0, NewRedGiantPressure);
}

void ASRStar::SettleStellarFuelSecond(int32 CurrentSecondIndex)
{
	if (bSupernovaGameOver || StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		return;
	}

	const double CycleFuelDecrease = FMath::Max(0.0, RequiredStellarFuelPerCycle);
	const double PreviousStoredFuel = StoredStellarFuel;
	LastSecondFuelDecrease = CycleFuelDecrease;
	LastSecondFuelConsumed = FMath::Min(PreviousStoredFuel, CycleFuelDecrease);
	StoredStellarFuel = FMath::Max(0.0, PreviousStoredFuel - CycleFuelDecrease);
	LastSecondFuelDeficit = FMath::Max(0.0, CycleFuelDecrease - PreviousStoredFuel);
	bLastSecondSurvived = StoredStellarFuel > UE_DOUBLE_SMALL_NUMBER;
	LastSettledSecondIndex = FMath::Max(0, CurrentSecondIndex);
	RefreshStellarPressureState();

	if (StoredStellarFuel <= UE_DOUBLE_SMALL_NUMBER)
	{
		AdvanceStellarEvolutionStage();
	}
}

FSRStellarFuelState ASRStar::GetStellarFuelState() const
{
	FSRStellarFuelState State;
	State.EvolutionStage = StellarEvolutionStage;
	State.StoredFuel = StoredStellarFuel;
	State.InitialStageFuel = InitialStageStellarFuel;
	State.InitialFuelDecreasePerSecond = InitialStellarFuelDecreasePerSecond;
	State.RequiredFuelPerCycle = RequiredStellarFuelPerCycle;
	State.RequirementGrowthPerCycle = StellarFuelRequirementGrowthPerCycle;
	State.bUsesStellarPressureCurveV2 = bUsesStellarPressureCurveV2;
	State.DemandCurveV2 = StellarDemandCurveV2;
	State.DemandPhase = bUsesStellarPressureCurveV2
		? FSRStellarDemandModel::ResolveDemandPhaseV2(
			StellarDemandCurveV2,
			LastFuelDecreaseRateCycleIndex)
		: ESRStellarDemandPhaseV2::Expansion;
	State.NextCycleDemandPerSecond = CalculateNextStellarFuelDecrease(
		RequiredStellarFuelPerCycle,
		LastFuelDecreaseRateCycleIndex + 1);
	State.FuelPressureRatio = bUsesStellarPressureCurveV2
		? FSRStellarDemandModel::CalculateFuelPressureRatioV2(
			StoredStellarFuel,
			StellarPressureRulesV2)
		: 0.0f;
	State.LastFuelDecreaseRateCycleIndex = LastFuelDecreaseRateCycleIndex;
	State.RedGiantPressure = RedGiantPressure;
	State.RedGiantPressurePerMissingFuel = RedGiantPressurePerMissingFuel;
	State.LastSettledSecondIndex = LastSettledSecondIndex;
	State.LastSecondFuelConsumed = LastSecondFuelConsumed;
	State.LastSecondFuelDecrease = LastSecondFuelDecrease;
	State.LastSecondFuelDeficit = LastSecondFuelDeficit;
	State.RecentFuelIncomePerSecond = CalculateRecentFuelIncomePerSecond();
	State.FuelIncomeWindowSeconds = StellarFuelIncomeWindowSeconds;
	State.LastFuelDeliveryAmount = LastFuelDeliveryAmount;
	State.LastFuelReserveGain = LastFuelReserveGain;
	State.LastFuelReserveOverflow = LastFuelReserveOverflow;
	State.SecondsSinceLastFuelDelivery = LastFuelDeliverySimulationSeconds >= 0.0
		? FMath::Max(0.0, StellarFuelElapsedSimulationSeconds - LastFuelDeliverySimulationSeconds)
		: -1.0;
	State.TotalDeliveredFuel = TotalDeliveredFuel;
	State.RunProgress = StellarRunProgress;
	State.bLastSecondSurvived = bLastSecondSurvived;
	State.bSupernovaGameOver = bSupernovaGameOver;
	return State;
}

void ASRStar::HandleGameCycleAdvanced(int32 CurrentCycleIndex)
{
	UpdateStellarFuelDecreaseRateForCycle(CurrentCycleIndex);
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
}

void ASRStar::UnbindFromTimeControlSubsystem()
{
	if (USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get())
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &ASRStar::HandleGameCycleAdvanced);
	}
	BoundTimeControlSubsystem.Reset();
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

float ASRStar::GetEffectiveStellarFuelDeltaSeconds(float DeltaSeconds) const
{
	const float ClampedDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	if (const USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get())
	{
		return ClampedDeltaSeconds * FMath::Max(0.0f, TimeControlSubsystem->GetEffectiveTimeScale());
	}

	return ClampedDeltaSeconds;
}

void ASRStar::AdvanceStellarFuelTimer(float DeltaSeconds)
{
	if (bSupernovaGameOver || StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		return;
	}

	const float EffectiveDeltaSeconds = GetEffectiveStellarFuelDeltaSeconds(DeltaSeconds);
	StellarFuelElapsedSimulationSeconds += static_cast<double>(EffectiveDeltaSeconds);
	StellarFuelSecondAccumulator += EffectiveDeltaSeconds;
	PruneStellarFuelDeliverySamples();
	const int32 SecondsToSettle = FMath::FloorToInt(StellarFuelSecondAccumulator);
	if (SecondsToSettle > 0)
	{
		StellarFuelSecondAccumulator -= static_cast<float>(SecondsToSettle);
		for (int32 SecondIndex = 0; SecondIndex < SecondsToSettle; ++SecondIndex)
		{
			SettleStellarFuelSecond(LastSettledSecondIndex + 1);
			if (bSupernovaGameOver || StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
			{
				StellarFuelSecondAccumulator = 0.0f;
				return;
			}
		}
	}

	// Survival pressure resolves before an objective completion on the same
	// simulation boundary, so a depleted Star cannot win by a tie-frame.
	RefreshStellarRunProgress(static_cast<double>(EffectiveDeltaSeconds));
	if (StellarRunProgress.HasEnded())
	{
		StellarFuelSecondAccumulator = 0.0f;
	}
}

void ASRStar::PruneStellarFuelDeliverySamples()
{
	const double OldestRelevantSimulationTime =
		StellarFuelElapsedSimulationSeconds - StellarFuelIncomeWindowSeconds;
	StellarFuelDeliverySamples.RemoveAll(
		[OldestRelevantSimulationTime](const FSRStellarFuelDeliverySample& Sample)
		{
			return Sample.SimulationTimeSeconds <= OldestRelevantSimulationTime;
		});
}

double ASRStar::CalculateRecentFuelIncomePerSecond() const
{
	double RecentDeliveredFuel = 0.0;
	const double OldestRelevantSimulationTime =
		StellarFuelElapsedSimulationSeconds - StellarFuelIncomeWindowSeconds;
	for (const FSRStellarFuelDeliverySample& Sample : StellarFuelDeliverySamples)
	{
		if (Sample.SimulationTimeSeconds > OldestRelevantSimulationTime)
		{
			RecentDeliveredFuel += FMath::Max(0.0, Sample.FuelAmount);
		}
	}

	return RecentDeliveredFuel / StellarFuelIncomeWindowSeconds;
}

void ASRStar::RefreshStellarRunProgress(
	double DeltaSimulationSeconds,
	bool bDefeatTriggered)
{
	const ESRStellarRunPhase PreviousPhase = StellarRunProgress.Phase;
	const ESRStellarRunOutcome PreviousOutcome = StellarRunProgress.Outcome;
	StellarRunProgress = FSRStellarRunContractModel::Advance(
		StellarRunContract,
		StellarRunProgress,
		TotalDeliveredFuel,
		CalculateRecentFuelIncomePerSecond(),
		DeltaSimulationSeconds,
		StellarFuelElapsedSimulationSeconds,
		bDefeatTriggered);

	if (StellarRunProgress.Phase != PreviousPhase)
	{
		OnStellarRunPhaseChanged.Broadcast(PreviousPhase, StellarRunProgress.Phase);
	}

	if (StellarRunProgress.Outcome == PreviousOutcome
		|| StellarRunProgress.Outcome == ESRStellarRunOutcome::InProgress)
	{
		return;
	}

	if (USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get())
	{
		TimeControlSubsystem->PauseSimulation();
	}

	if (StellarRunProgress.Outcome == ESRStellarRunOutcome::Victory)
	{
		SR_LOG(Celestial, LogTemp, Log,
			TEXT("Star '%s' completed stellar stabilization at simulation second %.1f after receiving %.0f fuel."),
			*GetName(),
			StellarRunProgress.CompletionSimulationSeconds,
			StellarRunProgress.TotalDeliveredFuel);
	}
	OnStellarRunCompleted.Broadcast(this);
}

void ASRStar::RefreshStellarPressureState()
{
	if (!bUsesStellarPressureCurveV2)
	{
		return;
	}
	RedGiantPressure = 100.0 * static_cast<double>(
		FSRStellarDemandModel::CalculateFuelPressureRatioV2(
			StoredStellarFuel,
			StellarPressureRulesV2));
	RedGiantPressurePerMissingFuel =
		100.0 / StellarPressureRulesV2.FuelReserveCapacity;
}

void ASRStar::UpdateStellarFuelDecreaseRateForCycle(int32 CurrentCycleIndex)
{
	if (bSupernovaGameOver
		|| StellarEvolutionStage == ESRStellarEvolutionStage::Supernova
		|| StellarRunProgress.HasEnded())
	{
		return;
	}

	const int32 SafeCycleIndex = FMath::Max(0, CurrentCycleIndex);
	const double PreviousCycleFuelDecrease = FMath::Max(0.0, RequiredStellarFuelPerCycle);
	RequiredStellarFuelPerCycle = CalculateNextStellarFuelDecrease(
		PreviousCycleFuelDecrease,
		SafeCycleIndex);
	StellarFuelRequirementGrowthPerCycle = bUsesStellarPressureCurveV2
		? FSRStellarDemandModel::CalculateNextCycleMultiplierV2(
			StellarDemandCurveV2,
			SafeCycleIndex)
		: FMath::Max(0.0, 200.0 + static_cast<double>(SafeCycleIndex)) / 100.0;
	LastFuelDecreaseRateCycleIndex = SafeCycleIndex;
}

double ASRStar::CalculateNextStellarFuelDecrease(
	double PreviousCycleFuelDecrease,
	int32 CurrentCycleIndex) const
{
	if (bUsesStellarPressureCurveV2)
	{
		return FSRStellarDemandModel::CalculateDemandForCycleV2(
			StellarDemandCurveV2,
			CurrentCycleIndex);
	}
	return FSRStellarDemandModel::CalculateLegacyNextCycleDemand(
		PreviousCycleFuelDecrease,
		CurrentCycleIndex);
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
		StoredStellarFuel = bUsesStellarPressureCurveV2
			? FSRStellarDemandModel::ResolveRedGiantEmergencyReserveV2(
				StellarPressureRulesV2)
			: InitialStageStellarFuel;
		RefreshStellarPressureState();
		SR_LOG(Celestial, LogTemp, Warning, TEXT("Star '%s' evolved from main sequence to red giant at stellar fuel second %d."), *GetName(), LastSettledSecondIndex);
		return;
	}

	if (StellarEvolutionStage == ESRStellarEvolutionStage::RedGiant)
	{
		SetStellarEvolutionStage(ESRStellarEvolutionStage::Supernova);
		StoredStellarFuel = 0.0;
		RefreshStellarPressureState();
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
	RefreshStellarRunProgress(0.0, true);

	SR_LOG(Celestial, LogTemp, Error, TEXT("Star '%s' reached supernova at stellar fuel second %d. Game over."), *GetName(), LastSettledSecondIndex);
	OnStellarSupernovaGameOver.Broadcast(this);
}
