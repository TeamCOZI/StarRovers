#include "Celestial/SRStar.h"

#include "Utility/SRLog.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Simulation/SRTimeControlSubsystem.h"

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
	InitialStageStellarFuel = FMath::Max(0.0, NewData.InitialStoredStellarFuel);
	StoredStellarFuel = InitialStageStellarFuel;
	InitialStellarFuelDecreasePerSecond = FMath::Max(0.0, NewData.InitialStellarFuelDecreasePerSecond);
	RequiredStellarFuelPerCycle = InitialStellarFuelDecreasePerSecond;
	StellarFuelRequirementGrowthPerCycle = 0.0;
	LastFuelDecreaseRateCycleIndex = 0;
	RedGiantPressure = FMath::Max(0.0, NewData.InitialRedGiantPressure);
	RedGiantPressurePerMissingFuel = FMath::Max(0.0, NewData.RedGiantPressurePerMissingFuel);
	StellarEvolutionStage = ESRStellarEvolutionStage::MainSequence;
	bSupernovaGameOver = false;
	LastSettledSecondIndex = 0;
	LastSecondFuelConsumed = 0.0;
	LastSecondFuelDecrease = 0.0;
	LastSecondFuelDeficit = 0.0;
	bLastSecondSurvived = true;
	StellarFuelSecondAccumulator = 0.0f;

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
	if (StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		return;
	}

	StoredStellarFuel = FMath::Max(0.0, StoredStellarFuel + FMath::Max(0.0, FuelAmount));
}

ESRStellarEvolutionStage ASRStar::GetStellarEvolutionStage() const
{
	return StellarEvolutionStage;
}

bool ASRStar::HasTriggeredSupernovaGameOver() const
{
	return bSupernovaGameOver;
}

bool ASRStar::CanAcceptStellarFuelResource(const FSRResourceInstance& ResourceInstance) const
{
	return CalculateStellarFuelValueForResource(ResourceInstance) > UE_DOUBLE_SMALL_NUMBER;
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
	if (OutFuelAmount <= UE_DOUBLE_SMALL_NUMBER)
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
	StoredStellarFuel = FMath::Max(0.0, NewStoredFuel);
}

void ASRStar::SetStellarFuelRequirement(double NewRequiredFuelPerCycle, double NewRequirementGrowthPerCycle)
{
	RequiredStellarFuelPerCycle = FMath::Max(0.0, NewRequiredFuelPerCycle);
	InitialStellarFuelDecreasePerSecond = FMath::Max(0.0, NewRequiredFuelPerCycle);
	StellarFuelRequirementGrowthPerCycle = FMath::Max(0.0, NewRequirementGrowthPerCycle);
}

void ASRStar::SetRedGiantPressure(double NewRedGiantPressure)
{
	RedGiantPressure = FMath::Max(0.0, NewRedGiantPressure);
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
	State.LastFuelDecreaseRateCycleIndex = LastFuelDecreaseRateCycleIndex;
	State.RedGiantPressure = RedGiantPressure;
	State.RedGiantPressurePerMissingFuel = RedGiantPressurePerMissingFuel;
	State.LastSettledSecondIndex = LastSettledSecondIndex;
	State.LastSecondFuelConsumed = LastSecondFuelConsumed;
	State.LastSecondFuelDecrease = LastSecondFuelDecrease;
	State.LastSecondFuelDeficit = LastSecondFuelDeficit;
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

	StellarFuelSecondAccumulator += GetEffectiveStellarFuelDeltaSeconds(DeltaSeconds);
	const int32 SecondsToSettle = FMath::FloorToInt(StellarFuelSecondAccumulator);
	if (SecondsToSettle <= 0)
	{
		return;
	}

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

void ASRStar::UpdateStellarFuelDecreaseRateForCycle(int32 CurrentCycleIndex)
{
	if (bSupernovaGameOver || StellarEvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		return;
	}

	const int32 PreviousCycleIndex = FMath::Max(0, CurrentCycleIndex);
	const double PreviousCycleFuelDecrease = FMath::Max(0.0, RequiredStellarFuelPerCycle);
	StellarFuelRequirementGrowthPerCycle = FMath::Max(0.0, 200.0 + static_cast<double>(PreviousCycleIndex)) / 100.0;
	RequiredStellarFuelPerCycle = CalculateNextStellarFuelDecrease(PreviousCycleFuelDecrease, PreviousCycleIndex);
	LastFuelDecreaseRateCycleIndex = PreviousCycleIndex;
}

double ASRStar::CalculateNextStellarFuelDecrease(double PreviousCycleFuelDecrease, int32 PreviousCycleIndex) const
{
	const double GrowthPercent = 200.0 + static_cast<double>(FMath::Max(0, PreviousCycleIndex));
	return FMath::Max(0.0, PreviousCycleFuelDecrease * (GrowthPercent / 100.0));
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
		StoredStellarFuel = InitialStageStellarFuel;
		SR_LOG(Celestial, LogTemp, Warning, TEXT("Star '%s' evolved from main sequence to red giant at stellar fuel second %d."), *GetName(), LastSettledSecondIndex);
		return;
	}

	if (StellarEvolutionStage == ESRStellarEvolutionStage::RedGiant)
	{
		SetStellarEvolutionStage(ESRStellarEvolutionStage::Supernova);
		StoredStellarFuel = 0.0;
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

	SR_LOG(Celestial, LogTemp, Error, TEXT("Star '%s' reached supernova at stellar fuel second %d. Game over."), *GetName(), LastSettledSecondIndex);
	OnStellarSupernovaGameOver.Broadcast(this);
}
