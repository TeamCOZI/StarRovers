#include "Celestial/SRStar.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Simulation/SRTimeControlSubsystem.h"

ASRStar::ASRStar()
{
	BodyCategory = ESRCelestialBodyCategory::Star;
	StarPointLightIntensity = 100.0f;
	StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	StarPointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StarPointLight"));
	StarPointLight->SetupAttachment(SceneRoot);
	StarPointLight->SetMobility(EComponentMobility::Movable);
	StarPointLight->SetVisibility(true);
	StarPointLight->SetUseInverseSquaredFalloff(false);
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
	StoredStellarFuel = FMath::Max(0.0, NewData.InitialStoredStellarFuel);
	RequiredStellarFuelPerCycle = FMath::Max(0.0, NewData.RequiredStellarFuelPerCycle);
	StellarFuelRequirementGrowthPerCycle = FMath::Max(0.0, NewData.StellarFuelRequirementGrowthPerCycle);
	RedGiantPressure = FMath::Max(0.0, NewData.InitialRedGiantPressure);
	RedGiantPressurePerMissingFuel = FMath::Max(0.0, NewData.RedGiantPressurePerMissingFuel);
	LastSettledCycleIndex = 0;
	LastCycleFuelConsumed = 0.0;
	LastCycleFuelDeficit = 0.0;
	bLastCycleMetRequirement = true;

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
	CurrentData.RequiredStellarFuelPerCycle = RequiredStellarFuelPerCycle;
	CurrentData.StellarFuelRequirementGrowthPerCycle = StellarFuelRequirementGrowthPerCycle;
	CurrentData.InitialRedGiantPressure = RedGiantPressure;
	CurrentData.RedGiantPressurePerMissingFuel = RedGiantPressurePerMissingFuel;
	return CurrentData;
}

void ASRStar::AddStellarFuel(double FuelAmount)
{
	StoredStellarFuel = FMath::Max(0.0, StoredStellarFuel + FMath::Max(0.0, FuelAmount));
}

bool ASRStar::CanAcceptStellarFuelResource(const FSRResourceInstance& ResourceInstance) const
{
	return CalculateStellarFuelValueForResource(ResourceInstance) > UE_DOUBLE_SMALL_NUMBER;
}

double ASRStar::CalculateStellarFuelValueForResource(const FSRResourceInstance& ResourceInstance) const
{
	if (ResourceInstance.ResourceKind != ESRResourceKind::Energy || ResourceInstance.StackCount <= 0)
	{
		return 0.0;
	}

	bool bCountsAsStellarFuel = ResourceInstance.bCountsAsStellarFuel;
	double FuelMultiplier = ResourceInstance.StellarFuelValueMultiplier;
	if (!bCountsAsStellarFuel)
	{
		if (const USRResourceDataAsset* ResourceDataAsset = ResourceInstance.ResourceDataAsset.Get())
		{
			bCountsAsStellarFuel = ResourceDataAsset->bCountsAsStellarFuel;
			FuelMultiplier = ResourceDataAsset->StellarFuelValueMultiplier;
		}
	}

	if (!bCountsAsStellarFuel)
	{
		return 0.0;
	}

	const double FuelValue = ResourceInstance.EnergyValue
		* FMath::Max(0.0, FuelMultiplier)
		* static_cast<double>(FMath::Max(1, ResourceInstance.StackCount));
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
	StellarFuelRequirementGrowthPerCycle = FMath::Max(0.0, NewRequirementGrowthPerCycle);
}

void ASRStar::SetRedGiantPressure(double NewRedGiantPressure)
{
	RedGiantPressure = FMath::Max(0.0, NewRedGiantPressure);
}

void ASRStar::SettleStellarFuelCycle(int32 CurrentCycleIndex)
{
	const double CycleRequirement = FMath::Max(0.0, RequiredStellarFuelPerCycle);
	LastCycleFuelConsumed = FMath::Min(StoredStellarFuel, CycleRequirement);
	StoredStellarFuel = FMath::Max(0.0, StoredStellarFuel - LastCycleFuelConsumed);
	LastCycleFuelDeficit = FMath::Max(0.0, CycleRequirement - LastCycleFuelConsumed);
	bLastCycleMetRequirement = LastCycleFuelDeficit <= UE_DOUBLE_SMALL_NUMBER;
	if (!bLastCycleMetRequirement)
	{
		RedGiantPressure = FMath::Max(0.0, RedGiantPressure + LastCycleFuelDeficit * RedGiantPressurePerMissingFuel);
	}

	RequiredStellarFuelPerCycle = FMath::Max(0.0, RequiredStellarFuelPerCycle + StellarFuelRequirementGrowthPerCycle);
	LastSettledCycleIndex = FMath::Max(0, CurrentCycleIndex);
}

FSRStellarFuelState ASRStar::GetStellarFuelState() const
{
	FSRStellarFuelState State;
	State.StoredFuel = StoredStellarFuel;
	State.RequiredFuelPerCycle = RequiredStellarFuelPerCycle;
	State.RequirementGrowthPerCycle = StellarFuelRequirementGrowthPerCycle;
	State.RedGiantPressure = RedGiantPressure;
	State.RedGiantPressurePerMissingFuel = RedGiantPressurePerMissingFuel;
	State.LastSettledCycleIndex = LastSettledCycleIndex;
	State.LastCycleFuelConsumed = LastCycleFuelConsumed;
	State.LastCycleFuelDeficit = LastCycleFuelDeficit;
	State.bLastCycleMetRequirement = bLastCycleMetRequirement;
	return State;
}

void ASRStar::HandleGameCycleAdvanced(int32 CurrentCycleIndex)
{
	SettleStellarFuelCycle(CurrentCycleIndex);
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
	USRTimeControlSubsystem* TimeControlSubsystem = BoundTimeControlSubsystem.Get();
	if (IsValid(TimeControlSubsystem))
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
