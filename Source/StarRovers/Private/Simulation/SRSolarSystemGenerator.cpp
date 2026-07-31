#include "Simulation/SRSolarSystemGenerator.h"
#include "Simulation/SRSolarSystemGeneratorPipeline.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRMoonDataAsset.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Celestial/SRStar.h"
#include "Celestial/SRStarDataAsset.h"
#include "Components/SceneComponent.h"
#include "Pattern/SRPatternEnvironmentDataAsset.h"
#include "Pattern/SRPatternGenerationProfileDataAsset.h"
#include "Pattern/SRStellarPatternContract.h"
#include "TimerManager.h"
#include "UI/SRLoadingScreenWidget.h"
#include "Utility/SRLog.h"

using namespace StarRovers::Simulation::SolarSystemGeneration;

namespace
{
	template <typename TAsset>
	bool HasOnlyValidAssets(const TArray<TObjectPtr<TAsset>>& Assets)
	{
		if (Assets.IsEmpty())
		{
			return false;
		}

		for (const TObjectPtr<TAsset>& Asset : Assets)
		{
			if (!IsValid(Asset.Get()))
			{
				return false;
			}
		}
		return true;
	}

#if WITH_EDITOR
	template <typename TAsset>
	bool MatchesAssetArray(
		const TArray<TObjectPtr<TAsset>>& ActualAssets,
		const TArray<TAsset*>& ExpectedAssets)
	{
		if (ActualAssets.Num() != ExpectedAssets.Num())
		{
			return false;
		}
		for (int32 AssetIndex = 0; AssetIndex < ActualAssets.Num(); ++AssetIndex)
		{
			if (ActualAssets[AssetIndex].Get() != ExpectedAssets[AssetIndex])
			{
				return false;
			}
		}
		return true;
	}
#endif

	void ResizeOrbitPeriodsToCount(TArray<float>& OrbitPeriods, int32 RequestedCount)
	{
		const int32 ResolvedCount = FMath::Max(0, RequestedCount);
		const int32 PreviousCount = OrbitPeriods.Num();
		OrbitPeriods.SetNum(ResolvedCount);

		for (int32 Index = PreviousCount; Index < ResolvedCount; ++Index)
		{
			OrbitPeriods[Index] = static_cast<float>(Index + 1);
		}

		for (float& OrbitPeriod : OrbitPeriods)
		{
			OrbitPeriod = FMath::Max(0.0f, OrbitPeriod);
		}
	}

	float ResolveIndexedOrbitPeriod(const TArray<float>& OrbitPeriods, int32 BodyIndex)
	{
		if (BodyIndex < 0)
		{
			return 1.0f;
		}

		return OrbitPeriods.IsValidIndex(BodyIndex)
			? FMath::Max(0.0f, OrbitPeriods[BodyIndex])
			: static_cast<float>(BodyIndex + 1);
	}
}

ASRSolarSystemGenerator::ASRSolarSystemGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GenerationSeed = 1000;
	bRandomizeGenerationSeedEachRun = true;
	MinPlanet = 3;
	MaxPlanet = 7;
	MinMoon = 0;
	MaxMoon = 1;
	PlanetInitialOrbit = 30000.0f;
	PlanetOrbitIncrease = 20000.0f;
	MoonInitialOrbit = 6000.0f;
	MoonOrbitIncrease = 4000.0f;
	NormalizeOrbitPeriodSettings();
	bGenerateNaturalStructures = true;
	LoadingScreenWidgetClass = USRLoadingScreenWidget::StaticClass();
	LoadingScreenZOrder = 10000;
	bEnableMemoryDiagnostics = true;
	bParallelDynamicMeshPreparation = true;
	DynamicMeshPreparationMaxConcurrency = 4;
}

#if WITH_EDITOR
void ASRSolarSystemGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NormalizeOrbitPeriodSettings();
}

void ASRSolarSystemGenerator::ConfigurePatternContentForEditor(
	USRPatternGenerationProfileDataAsset* InPatternGenerationProfile,
	TSubclassOf<ASRCelestialBody> InStarClass,
	TSubclassOf<ASRCelestialBody> InPlanetClass,
	const TArray<USRStarDataAsset*>& InStarDataAssets,
	const TArray<USRPlanetDataAsset*>& InPlanetDataAssets,
	const TArray<USRMoonDataAsset*>& InMoonDataAssets,
	int32 InGenerationSeed)
{
	Modify();
	PatternGenerationProfileDataAsset = InPatternGenerationProfile;
	StarClass = InStarClass;
	PlanetClass = InPlanetClass;
	StarDataAssets = InStarDataAssets;
	PlanetDataAssets = InPlanetDataAssets;
	MoonDataAssets = InMoonDataAssets;
	GenerationSeed = FMath::Max(0, InGenerationSeed);
	bRandomizeGenerationSeedEachRun = true;
	NormalizeOrbitPeriodSettings();
	MarkPackageDirty();
}

bool ASRSolarSystemGenerator::MatchesPatternContentForEditor(
	const USRPatternGenerationProfileDataAsset* InPatternGenerationProfile,
	TSubclassOf<ASRCelestialBody> InStarClass,
	TSubclassOf<ASRCelestialBody> InPlanetClass,
	const TArray<USRStarDataAsset*>& InStarDataAssets,
	const TArray<USRPlanetDataAsset*>& InPlanetDataAssets,
	const TArray<USRMoonDataAsset*>& InMoonDataAssets) const
{
	return PatternGenerationProfileDataAsset.Get() == InPatternGenerationProfile
		&& StarClass == InStarClass
		&& PlanetClass == InPlanetClass
		&& bRandomizeGenerationSeedEachRun
		&& MatchesAssetArray(StarDataAssets, InStarDataAssets)
		&& MatchesAssetArray(PlanetDataAssets, InPlanetDataAssets)
		&& MatchesAssetArray(MoonDataAssets, InMoonDataAssets);
}
#endif

void ASRSolarSystemGenerator::PostLoad()
{
	Super::PostLoad();
	NormalizeOrbitPeriodSettings();
}

void ASRSolarSystemGenerator::BeginPlay()
{
	Super::BeginPlay();
	NormalizeOrbitPeriodSettings();
	if (!EnsurePatternContentLoadedFromClassDefaults())
	{
		return;
	}
	EnsureMemoryDiagnosticTrackedClasses();
	LogMemoryDiagnosticsSnapshot(TEXT("SolarSystemGenerator.BeginPlay.BeforeGeneration"));

	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	StartRuntimeSystemGenerationWithLoadingScreen();
}

bool ASRSolarSystemGenerator::ValidatePatternContentConfiguration(FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!StarClass || !StarClass->IsChildOf(ASRStar::StaticClass()))
	{
		OutFailureReason = TEXT("StarClass must derive from ASRStar.");
		return false;
	}
	if (!PlanetClass || !PlanetClass->IsChildOf(ASRCelestialBody::StaticClass()))
	{
		OutFailureReason = TEXT("PlanetClass must derive from ASRCelestialBody.");
		return false;
	}
	if (!HasOnlyValidAssets(StarDataAssets))
	{
		OutFailureReason = TEXT("StarDataAssets must contain only loaded Data Assets.");
		return false;
	}
	for (const TObjectPtr<USRStarDataAsset>& StarDataAsset : StarDataAssets)
	{
		FSRCelestialBodyGenerateRequest RuntimeRequest;
		if (!TryBuildRequestFromDataAsset(StarClass, StarDataAsset.Get(), RuntimeRequest))
		{
			OutFailureReason = FString::Printf(
				TEXT("Star Data Asset '%s' is missing data required by runtime generation."),
				*GetNameSafe(StarDataAsset.Get()));
			return false;
		}
		FString ContractFailureReason;
		if (!FSRStellarPatternContractResolver::ValidateContract(
			StarDataAsset->DefaultStellarPatternContract,
			ContractFailureReason))
		{
			OutFailureReason = FString::Printf(
				TEXT("Star Data Asset '%s' has an invalid default Pattern contract: %s"),
				*GetNameSafe(StarDataAsset.Get()),
				*ContractFailureReason);
			return false;
		}
	}
	if (!HasOnlyValidAssets(PlanetDataAssets))
	{
		OutFailureReason = TEXT("PlanetDataAssets must contain only loaded Data Assets.");
		return false;
	}
	for (const TObjectPtr<USRPlanetDataAsset>& PlanetDataAsset : PlanetDataAssets)
	{
		FSRCelestialBodyGenerateRequest RuntimeRequest;
		if (!TryBuildRequestFromDataAsset(PlanetClass, PlanetDataAsset.Get(), RuntimeRequest))
		{
			OutFailureReason = FString::Printf(
				TEXT("Planet Data Asset '%s' is missing data required by runtime generation."),
				*GetNameSafe(PlanetDataAsset.Get()));
			return false;
		}
		if (!IsValid(PlanetDataAsset->PatternEnvironmentDataAsset.Get())
			|| !PlanetDataAsset->PatternEnvironmentDataAsset->IsEnvironmentValid())
		{
			OutFailureReason = FString::Printf(
				TEXT("Planet Data Asset '%s' has no valid Pattern environment."),
				*GetNameSafe(PlanetDataAsset.Get()));
			return false;
		}
	}
	if (!HasOnlyValidAssets(MoonDataAssets))
	{
		OutFailureReason = TEXT("MoonDataAssets must contain only loaded Data Assets.");
		return false;
	}
	for (const TObjectPtr<USRMoonDataAsset>& MoonDataAsset : MoonDataAssets)
	{
		FSRCelestialBodyGenerateRequest RuntimeRequest;
		if (!TryBuildRequestFromDataAsset(PlanetClass, MoonDataAsset.Get(), RuntimeRequest))
		{
			OutFailureReason = FString::Printf(
				TEXT("Moon Data Asset '%s' is missing data required by runtime generation."),
				*GetNameSafe(MoonDataAsset.Get()));
			return false;
		}
		if (!IsValid(MoonDataAsset->PatternEnvironmentDataAsset.Get())
			|| !MoonDataAsset->PatternEnvironmentDataAsset->IsEnvironmentValid())
		{
			OutFailureReason = FString::Printf(
				TEXT("Moon Data Asset '%s' has no valid Pattern environment."),
				*GetNameSafe(MoonDataAsset.Get()));
			return false;
		}
	}

	const USRPatternGenerationProfileDataAsset* Profile = PatternGenerationProfileDataAsset.Get();
	if (!IsValid(Profile)
		|| Profile->CandidateStellarContracts.IsEmpty()
		|| Profile->AvailableFacilityDataAssets.IsEmpty()
		|| Profile->MaxValidationSourcesPerResourcePerBody < 1
		|| Profile->MaxValidationSourcesPerResourcePerBody > 8)
	{
		OutFailureReason = TEXT("PatternGenerationProfileDataAsset is missing, empty, or has invalid validation sampling bounds.");
		return false;
	}
	for (const TObjectPtr<USRFacilityDataAsset>& FacilityDataAsset : Profile->AvailableFacilityDataAssets)
	{
		if (!IsValid(FacilityDataAsset.Get()))
		{
			OutFailureReason = TEXT("Pattern Generation Profile contains an unloaded Facility Data Asset.");
			return false;
		}
	}
	for (const FSRStellarPatternContract& Contract : Profile->CandidateStellarContracts)
	{
		FString ContractFailureReason;
		if (!FSRStellarPatternContractResolver::ValidateContract(Contract, ContractFailureReason))
		{
			OutFailureReason = FString::Printf(
				TEXT("Pattern Generation Profile contract '%s' is invalid: %s"),
				*Contract.ContractId.ToString(),
				*ContractFailureReason);
			return false;
		}
	}
	return true;
}

bool ASRSolarSystemGenerator::EnsurePatternContentLoadedFromClassDefaults()
{
	FString FailureReason;
	if (ValidatePatternContentConfiguration(FailureReason))
	{
		return true;
	}

	const ASRSolarSystemGenerator* ClassDefaults = GetClass()
		? Cast<ASRSolarSystemGenerator>(GetClass()->GetDefaultObject())
		: nullptr;
	if (!IsValid(ClassDefaults) || ClassDefaults == this)
	{
		SR_LOG(SolarSystem, LogTemp, Error,
			TEXT("Solar-system Pattern content is invalid and no class defaults are available: %s"),
			*FailureReason);
		return false;
	}

	PatternGenerationProfileDataAsset = ClassDefaults->PatternGenerationProfileDataAsset;
	StarClass = ClassDefaults->StarClass;
	PlanetClass = ClassDefaults->PlanetClass;
	StarDataAssets = ClassDefaults->StarDataAssets;
	PlanetDataAssets = ClassDefaults->PlanetDataAssets;
	MoonDataAssets = ClassDefaults->MoonDataAssets;

	FString RepairedFailureReason;
	if (!ValidatePatternContentConfiguration(RepairedFailureReason))
	{
		SR_LOG(SolarSystem, LogTemp, Error,
			TEXT("Solar-system Pattern content could not be restored from class defaults. Initial=%s Restored=%s"),
			*FailureReason,
			*RepairedFailureReason);
		return false;
	}

	SR_LOG(SolarSystem, LogTemp, Warning,
		TEXT("Restored incomplete SolarSystem Generator Pattern content from '%s' class defaults. Initial=%s"),
		*GetNameSafe(GetClass()),
		*FailureReason);
	return true;
}

void ASRSolarSystemGenerator::Destroyed()
{
	bRuntimeGenerationInProgress = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredGenerateRuntimeSystemTimerHandle);
	}
	ClearRuntimeGeneratedBodies();
	HideLoadingScreen();
	Super::Destroyed();
}

void ASRSolarSystemGenerator::NormalizeOrbitPeriodSettings()
{
	const int32 ResolvedMaxPlanetCount = FMath::Max(FMath::Max(0, MinPlanet), MaxPlanet);
	const int32 ResolvedMaxMoonCount = FMath::Max(FMath::Max(0, MinMoon), MaxMoon);
	ResizeOrbitPeriodsToCount(PlanetOrbitPeriods, ResolvedMaxPlanetCount);
	ResizeOrbitPeriodsToCount(MoonOrbitPeriods, ResolvedMaxMoonCount);
}

float ASRSolarSystemGenerator::ResolvePlanetOrbitPeriod(int32 PlanetIndex) const
{
	return ResolveIndexedOrbitPeriod(PlanetOrbitPeriods, PlanetIndex);
}

float ASRSolarSystemGenerator::ResolveMoonOrbitPeriod(int32 MoonIndex) const
{
	return ResolveIndexedOrbitPeriod(MoonOrbitPeriods, MoonIndex);
}
