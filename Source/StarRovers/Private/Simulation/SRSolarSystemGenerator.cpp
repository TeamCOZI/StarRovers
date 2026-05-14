#include "Simulation/SRSolarSystemGenerator.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRMoonDataAsset.h"
#include "Celestial/SRPlanet.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Celestial/SRStarDataAsset.h"
#include "Celestial/SRStar.h"
#include "Components/SceneComponent.h"
#include "Gravity/SRGravityParent.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"

namespace
{
	struct FSROrbitInfo
	{
		float OrbitingBodyExtent = 0.0f;
		float DesiredOrbitRadius = 0.0f;
	};

	void LogGeneratorMissingData(const UObject* SourceObject, const TCHAR* FieldName)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Solar system generation requires %s on '%s'."),
			FieldName ? FieldName : TEXT("<UnknownField>"),
			IsValid(SourceObject) ? *SourceObject->GetName() : TEXT("<InvalidObject>"));
	}

	bool TryGetRequiredDisplayName(const UObject* DataAsset, const FText& DisplayName, FText& OutDisplayName)
	{
		OutDisplayName = FText::GetEmpty();
		if (!IsValid(DataAsset))
		{
			LogGeneratorMissingData(DataAsset, TEXT("DataAsset"));
			return false;
		}

		if (DisplayName.IsEmpty())
		{
			LogGeneratorMissingData(DataAsset, TEXT("DisplayName"));
			return false;
		}

		OutDisplayName = DisplayName;
		return true;
	}

	bool TryComputeApproximateRadiusFromCelestialBodyRequest(const FSRCelestialBodyGenerateRequest& CelestialBodyRequest, float& OutRadius)
	{
		OutRadius = 0.0f;
		const UStaticMesh* MeshAsset = CelestialBodyRequest.BodyMesh.Get();
		if (!IsValid(MeshAsset))
		{
			UE_LOG(LogTemp, Error, TEXT("Solar system generation requires BodyMesh for '%s'."), *CelestialBodyRequest.BodySpec.DisplayName.ToString());
			return false;
		}

		OutRadius = FMath::Max(0.0f, MeshAsset->GetBounds().SphereRadius * FMath::Max(0.0f, CelestialBodyRequest.BodySpec.BodyScale));
		return true;
	}

	float ComputeGravityRadiusFromCelestialBodyRequest(const FSRCelestialBodyGenerateRequest& CelestialBodyRequest)
	{
		return FMath::Max(0.0f, CelestialBodyRequest.BodySpec.Mass)
			* FMath::Max(0.0f, CelestialBodyRequest.BodySpec.GravityRadiusRatio);
	}

	bool TryBuildSpecFromClassDefaultsAndBiome(
		const TSubclassOf<ASRCelestialBody>& BodyClass,
		const FSRCelestialBodyBiomeSpec& BiomeSpec,
		const ESRCelestialBodyCategory BodyCategory,
		FSRCelestialBodySpec& OutSpec)
	{
		if (!BodyClass || BodyClass == ASRCelestialBody::StaticClass())
		{
			UE_LOG(LogTemp, Error, TEXT("Solar system generation requires a concrete celestial body class."));
			return false;
		}

		const UClass* BodyClassObject = BodyClass.Get();
		const ASRCelestialBody* DefaultBody = IsValid(BodyClassObject)
			? Cast<ASRCelestialBody>(BodyClassObject->GetDefaultObject())
			: nullptr;
		if (!IsValid(DefaultBody))
		{
			UE_LOG(LogTemp, Error, TEXT("Solar system generation requires '%s' to inherit ASRCelestialBody."), *GetNameSafe(BodyClassObject));
			return false;
		}

		OutSpec = DefaultBody->GetSpec();
		OutSpec.BodyCategory = BodyCategory;
		OutSpec.DisplayName = BiomeSpec.DisplayName;
		OutSpec.bUseProceduralTerrain = BiomeSpec.bUseProceduralTerrain;
		OutSpec.TerrainSeed = BiomeSpec.TerrainSeed;
		OutSpec.TerrainHeight = BiomeSpec.TerrainHeight;
		OutSpec.TerrainFrequency = BiomeSpec.TerrainFrequency;
		OutSpec.TerrainOctaves = BiomeSpec.TerrainOctaves;
		OutSpec.TerrainPersistence = BiomeSpec.TerrainPersistence;
		OutSpec.bHasOcean = BiomeSpec.bHasOcean;
		OutSpec.OceanMesh = BiomeSpec.OceanMesh;
		OutSpec.OceanMaterial = BiomeSpec.OceanMaterial;
		OutSpec.OceanScaleMultiplier = BiomeSpec.OceanScaleMultiplier;
		OutSpec.SurfaceGridSurfaceOffset = BiomeSpec.SurfaceGridSurfaceOffset;
		OutSpec.ShadowCasterScaleMultiplier = BiomeSpec.ShadowCasterScaleMultiplier;
		OutSpec.OrbitPeriodInPeriods = BiomeSpec.OrbitSpeed > KINDA_SMALL_NUMBER
			? 1.0f / BiomeSpec.OrbitSpeed
			: 0.0f;
		OutSpec.StarPointLightIntensity = BiomeSpec.StarPointLightIntensity;
		OutSpec.StarMaterialEmissiveStrength = BiomeSpec.StarMaterialEmissiveStrength;
		OutSpec.StarPointLightColor = BiomeSpec.StarPointLightColor;
		return true;
	}

	TSubclassOf<ASRCelestialBody> ValidateRuntimeCelestialClass(
		const TSubclassOf<ASRCelestialBody>& ConfiguredClass,
		const TCHAR* ClassPurpose)
	{
		if (ConfiguredClass && ConfiguredClass != ASRCelestialBody::StaticClass())
		{
			return ConfiguredClass;
		}

		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires a configured %s class."), ClassPurpose ? ClassPurpose : TEXT("celestial body"));
		return nullptr;
	}

	template <typename TDataAsset>
	bool TryBuildRequestFromDataAsset(
		const TSubclassOf<ASRCelestialBody>& BodyClass,
		const TDataAsset* DataAsset,
		FSRCelestialBodyGenerateRequest& OutRequest)
	{
		OutRequest = FSRCelestialBodyGenerateRequest();
		if (!IsValid(DataAsset))
		{
			LogGeneratorMissingData(DataAsset, TEXT("DataAsset"));
			return false;
		}

		OutRequest.BodyClass = BodyClass;
		if (!TryBuildSpecFromClassDefaultsAndBiome(BodyClass, DataAsset->BuildBiomeSpec(), DataAsset->BodyCategory, OutRequest.BodySpec))
		{
			return false;
		}

		OutRequest.BodySpec.BodyScale = FMath::Max(0.0f, DataAsset->BodyScale);
		OutRequest.BodySpec.Mass = FMath::Max(0.0f, DataAsset->Mass);
		OutRequest.BodySpec.GravityRatio = FMath::Max(0.0f, DataAsset->GravityRatio);
		OutRequest.BodySpec.GravityRadiusRatio = FMath::Max(0.0f, DataAsset->GravityRadiusRatio);
		OutRequest.BodyMesh = DataAsset->BodyMesh;
		OutRequest.BodyMaterial = DataAsset->BodyMaterial;

		if (!IsValid(OutRequest.BodyMesh))
		{
			LogGeneratorMissingData(DataAsset, TEXT("BodyMesh"));
			return false;
		}
		if (!IsValid(OutRequest.BodyMaterial))
		{
			LogGeneratorMissingData(DataAsset, TEXT("BodyMaterial"));
			return false;
		}

		FText DisplayName;
		if (!TryGetRequiredDisplayName(DataAsset, DataAsset->DisplayName, DisplayName))
		{
			return false;
		}
		OutRequest.BodySpec.DisplayName = DisplayName;
		return true;
	}

	template <typename TDataAsset>
	const TDataAsset* ResolveRandomDataAssetStrict(const TArray<TObjectPtr<TDataAsset>>& DataAssets, FRandomStream& RandomStream, const TCHAR* AssetTypeName)
	{
		TArray<const TDataAsset*> ValidAssets;
		for (const TDataAsset* Asset : DataAssets)
		{
			if (IsValid(Asset))
			{
				ValidAssets.Add(Asset);
			}
		}

		if (ValidAssets.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("ASRSolarSystemGenerator requires at least one valid %s data asset."), AssetTypeName ? AssetTypeName : TEXT("celestial body"));
			return nullptr;
		}

		return ValidAssets[RandomStream.RandRange(0, ValidAssets.Num() - 1)];
	}
}

ASRSolarSystemGenerator::ASRSolarSystemGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GenerationSeed = 1000;
	MinPlanet = 3;
	MaxPlanet = 7;
	MinMoon = 0;
	MaxMoon = 1;
	PlanetInitialOrbit = 30000.0f;
	PlanetOrbitIncrease = 20000.0f;
	MoonInitialOrbit = 5000.0f;
	MoonOrbitIncrease = 2500.0f;
}

void ASRSolarSystemGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	GenerateRuntimeSystem();
}

void ASRSolarSystemGenerator::Destroyed()
{
	ClearRuntimeGeneratedBodies();
	Super::Destroyed();
}

ASRCelestialBody* ASRSolarSystemGenerator::GenerateRuntimeSystem()
{
	if (!GetWorld())
	{
		return nullptr;
	}

	ClearRuntimeGeneratedBodies();
	FRandomStream RandomStream(GenerationSeed);
	const USRStarDataAsset* SelectedStarDataAsset = nullptr;
	RuntimeStarBody = SpawnPrimaryStar(RandomStream, SelectedStarDataAsset);
	if (!IsValid(RuntimeStarBody))
	{
		return nullptr;
	}

	SpawnPlanets(RuntimeStarBody, SelectedStarDataAsset, RandomStream, RuntimePlanetBodies);
	if (USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = GetWorld()->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
	{
		CelestialBodyRegistry->SetPrimaryStarActor(RuntimeStarBody);
	}

	return RuntimeStarBody;
}

void ASRSolarSystemGenerator::ClearRuntimeGeneratedBodies()
{
	DestroyTrackedActors(RuntimeMoonBodies);
	DestroyTrackedActors(RuntimePlanetBodies);
	DestroyTrackedActor(RuntimeStarBody);
	if (UWorld* World = GetWorld())
	{
		if (USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
		{
			CelestialBodyRegistry->SetPrimaryStarActor(nullptr);
		}
	}
}

ASRCelestialBody* ASRSolarSystemGenerator::SpawnPrimaryStar(FRandomStream& RandomStream, const USRStarDataAsset*& OutSelectedStarDataAsset)
{
	OutSelectedStarDataAsset = nullptr;
	UWorld* World = GetWorld();
	const TSubclassOf<ASRCelestialBody> ResolvedPrimaryStarClass = ValidateRuntimeCelestialClass(StarClass, TEXT("StarClass"));
	if (!World || !ResolvedPrimaryStarClass)
	{
		return nullptr;
	}

	const USRStarDataAsset* SelectedStarDataAsset = ResolveRandomDataAssetStrict(StarDataAssets, RandomStream, TEXT("star"));
	if (!IsValid(SelectedStarDataAsset))
	{
		return nullptr;
	}
	OutSelectedStarDataAsset = SelectedStarDataAsset;

	FSRCelestialBodyGenerateRequest StarCelestialBodyRequest;
	if (!TryBuildRequestFromDataAsset(ResolvedPrimaryStarClass, SelectedStarDataAsset, StarCelestialBodyRequest))
	{
		return nullptr;
	}
	StarCelestialBodyRequest.BodySpec.ParentBody = nullptr;
	StarCelestialBodyRequest.BodySpec.OrbitRadius = 0.0f;
	StarCelestialBodyRequest.BodySpec.OrbitPeriodInPeriods = 0.0f;
	StarCelestialBodyRequest.BodySpec.StartingPhase = 0.0f;

	return SpawnOrbitingBody(ResolvedPrimaryStarClass, StarCelestialBodyRequest, nullptr);
}

ASRCelestialBody* ASRSolarSystemGenerator::SpawnOrbitingBody(const TSubclassOf<ASRCelestialBody>& BodyClass, const FSRCelestialBodyGenerateRequest& CelestialBodyRequest, ASRCelestialBody* ParentBody)
{
	UWorld* World = GetWorld();
	if (!World || !BodyClass)
	{
		return nullptr;
	}

	const FVector SpawnLocation = IsValid(ParentBody)
		? ComputeOrbitWorldLocation(ParentBody, CelestialBodyRequest.BodySpec.OrbitRadius, CelestialBodyRequest.BodySpec.StartingPhase)
		: GetActorLocation();
	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	ASRCelestialBody* GeneratedCelestialBody = World->SpawnActorDeferred<ASRCelestialBody>(
		BodyClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(GeneratedCelestialBody))
	{
		return nullptr;
	}

	GeneratedCelestialBody->ApplySpec(CelestialBodyRequest.BodySpec);
	GeneratedCelestialBody->SetCelestialBodyAssets(CelestialBodyRequest.BodyMesh, CelestialBodyRequest.BodyMaterial);
	GeneratedCelestialBody->FinishSpawning(SpawnTransform);

#if WITH_EDITOR
	if (!CelestialBodyRequest.BodySpec.DisplayName.IsEmpty())
	{
		GeneratedCelestialBody->SetActorLabel(CelestialBodyRequest.BodySpec.DisplayName.ToString());
	}
#endif

	return GeneratedCelestialBody;
}

void ASRSolarSystemGenerator::BuildOrbitingBodyRequests(
	ASRCelestialBody* ParentBody,
	int32 RequestedBodyCount,
	const TArray<FSRCelestialBodyGenerateRequest>& CandidateCelestialBodyRequests,
	FRandomStream& RandomStream,
	TArray<FSRCelestialBodyGenerateRequest>& OutResolvedCelestialBodyRequests) const
{
	OutResolvedCelestialBodyRequests.Reset();
	if (!IsValid(ParentBody) || RequestedBodyCount <= 0 || CandidateCelestialBodyRequests.IsEmpty())
	{
		return;
	}

	const int32 CandidateCount = FMath::Min(RequestedBodyCount, CandidateCelestialBodyRequests.Num());
	TArray<FSRCelestialBodyGenerateRequest> CandidateCelestialBodies;
	CandidateCelestialBodies.Reserve(CandidateCount);
	for (int32 Index = 0; Index < CandidateCount; ++Index)
	{
		CandidateCelestialBodies.Add(CandidateCelestialBodyRequests[Index]);
	}

	TArray<float> PackedOrbitRadii;
	if (!TrySolvePackedOrbitRadii(ParentBody, CandidateCelestialBodies, PackedOrbitRadii))
	{
		return;
	}

	for (int32 Index = 0; Index < CandidateCelestialBodies.Num(); ++Index)
	{
		CandidateCelestialBodies[Index].BodySpec.OrbitRadius = PackedOrbitRadii[Index];
		CandidateCelestialBodies[Index].BodySpec.StartingPhase = RandomStream.FRandRange(0.0f, 360.0f);
	}

	OutResolvedCelestialBodyRequests = MoveTemp(CandidateCelestialBodies);
}

bool ASRSolarSystemGenerator::TrySolvePackedOrbitRadii(ASRCelestialBody* ParentBody, const TArray<FSRCelestialBodyGenerateRequest>& CelestialBodyRequests, TArray<float>& OutOrbitRadii) const
{
	OutOrbitRadii.Reset();

	if (!IsValid(ParentBody) || CelestialBodyRequests.IsEmpty())
	{
		return false;
	}

	const float ParentVisualRadius = USRCelestialBodyRuntimeLibrary::GetCelestialApproximateRadius(ParentBody);
	const USRGravityParent* ParentGravityParent = ParentBody->GetGravityParent();
	if (!IsValid(ParentGravityParent))
	{
		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires GravityParent on '%s'."), *ParentBody->GetName());
		return false;
	}
	const float ParentGravityRadius = ParentGravityParent->GetGravityRadius();

	TArray<FSROrbitInfo> OrbitInfos;
	OrbitInfos.Reserve(CelestialBodyRequests.Num());

	const bool bParentIsStar = ParentBody->GetBodyCategory() == ESRCelestialBodyCategory::Star;
	const float InitialOrbit = bParentIsStar ? PlanetInitialOrbit : MoonInitialOrbit;
	const float OrbitIncrease = bParentIsStar ? PlanetOrbitIncrease : MoonOrbitIncrease;

	for (int32 BodyIndex = 0; BodyIndex < CelestialBodyRequests.Num(); ++BodyIndex)
	{
		float BodyRadius = 0.0f;
		if (!TryComputeApproximateRadiusFromCelestialBodyRequest(CelestialBodyRequests[BodyIndex], BodyRadius))
		{
			return false;
		}
		const float GravityRadius = ComputeGravityRadiusFromCelestialBodyRequest(CelestialBodyRequests[BodyIndex]);
		FSROrbitInfo& OrbitInfo = OrbitInfos.AddDefaulted_GetRef();
		OrbitInfo.OrbitingBodyExtent = FMath::Max(BodyRadius, GravityRadius);
		OrbitInfo.DesiredOrbitRadius = InitialOrbit + (OrbitIncrease * static_cast<float>(BodyIndex));
	}

	OutOrbitRadii.SetNumUninitialized(OrbitInfos.Num());
	float NextMinimumInnerEdge = ParentVisualRadius;
	float RequiredParentGravityRadius = NextMinimumInnerEdge;
	for (int32 BodyIndex = 0; BodyIndex < OrbitInfos.Num(); ++BodyIndex)
	{
		const FSROrbitInfo& OrbitInfo = OrbitInfos[BodyIndex];
		const float MinimumCenterRadius = NextMinimumInnerEdge + OrbitInfo.OrbitingBodyExtent;
		const float OrbitRadius = FMath::Max(OrbitInfo.DesiredOrbitRadius, MinimumCenterRadius);
		OutOrbitRadii[BodyIndex] = OrbitRadius;

		const float OuterEdge = OrbitRadius + OrbitInfo.OrbitingBodyExtent;
		RequiredParentGravityRadius = OuterEdge;
		NextMinimumInnerEdge = OuterEdge;
	}

	if (ParentGravityRadius + KINDA_SMALL_NUMBER < RequiredParentGravityRadius)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Solar system generation requires '%s' gravity radius %.2f to be at least %.2f for %d orbiting bodies."),
			*ParentBody->GetName(),
			ParentGravityRadius,
			RequiredParentGravityRadius,
			CelestialBodyRequests.Num());
		return false;
	}

	return true;
}

void ASRSolarSystemGenerator::EnsureParentGravityContainsOrbitingBody(ASRCelestialBody* ParentBody, const ASRCelestialBody* OrbitingBody) const
{
	if (!IsValid(ParentBody) || !IsValid(OrbitingBody))
	{
		return;
	}

	const USRGravityParent* ParentGravityParent = ParentBody->GetGravityParent();
	if (!IsValid(ParentGravityParent))
	{
		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires GravityParent on '%s'."), *ParentBody->GetName());
		return;
	}
	const float ParentGravityRadius = ParentGravityParent->GetGravityRadius();

	const float OrbitRadius = FVector::Dist(ParentBody->GetActorLocation(), OrbitingBody->GetActorLocation());
	const float OrbitingBodyVisualRadius = USRCelestialBodyRuntimeLibrary::GetCelestialApproximateRadius(OrbitingBody);
	const USRGravityParent* OrbitingBodyGravityParent = OrbitingBody->GetGravityParent();
	if (!IsValid(OrbitingBodyGravityParent))
	{
		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires GravityParent on '%s'."), *OrbitingBody->GetName());
		return;
	}
	const float OrbitingBodyGravityRadius = OrbitingBodyGravityParent->GetGravityRadius();
	const float OrbitingBodyExtent = FMath::Max(OrbitingBodyVisualRadius, OrbitingBodyGravityRadius);
	const float RequiredParentGravityRadius = OrbitRadius + OrbitingBodyExtent;
	if (ParentGravityRadius + KINDA_SMALL_NUMBER >= RequiredParentGravityRadius)
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("Solar system generation requires '%s' gravity radius %.2f to be at least %.2f to contain orbiting body '%s'."),
		*ParentBody->GetName(),
		ParentGravityRadius,
		RequiredParentGravityRadius,
		*OrbitingBody->GetName());
}

FVector ASRSolarSystemGenerator::ComputeOrbitWorldLocation(const AActor* ParentBody, float OrbitRadius, float StartingPhaseDegrees) const
{
	const FVector ParentLocation = IsValid(ParentBody) ? ParentBody->GetActorLocation() : GetActorLocation();
	const float PhaseRadians = FMath::DegreesToRadians(StartingPhaseDegrees);

	return FVector(
		ParentLocation.X,
		ParentLocation.Y + (FMath::Cos(PhaseRadians) * OrbitRadius),
		ParentLocation.Z + (FMath::Sin(PhaseRadians) * OrbitRadius));
}

void ASRSolarSystemGenerator::SpawnPlanets(ASRCelestialBody* ParentStar, const USRStarDataAsset* SourceStarDataAsset, FRandomStream& RandomStream, TArray<TObjectPtr<ASRCelestialBody>>& OutGeneratedPlanets)
{
	OutGeneratedPlanets.Reset();
	if (!IsValid(ParentStar) || !IsValid(SourceStarDataAsset))
	{
		return;
	}

	const int32 ResolvedMinPlanetCount = FMath::Max(0, MinPlanet);
	const int32 ResolvedMaxPlanetCount = FMath::Max(ResolvedMinPlanetCount, MaxPlanet);
	const int32 RequestedPlanetCount = RandomStream.RandRange(ResolvedMinPlanetCount, ResolvedMaxPlanetCount);
	if (RequestedPlanetCount <= 0)
	{
		return;
	}

	TArray<FSRCelestialBodyGenerateRequest> CandidatePlanetCelestialBodyRequests;
	CandidatePlanetCelestialBodyRequests.Reserve(RequestedPlanetCount);
	const TSubclassOf<ASRCelestialBody> ResolvedPlanetClass = ValidateRuntimeCelestialClass(PlanetClass, TEXT("PlanetClass"));
	if (!ResolvedPlanetClass)
	{
		return;
	}

	for (int32 PlanetIndex = 0; PlanetIndex < RequestedPlanetCount; ++PlanetIndex)
	{
		const USRPlanetDataAsset* SelectedPlanetData = ResolveRandomDataAssetStrict(PlanetDataAssets, RandomStream, TEXT("planet"));
		if (!IsValid(SelectedPlanetData))
		{
			return;
		}

		FSRCelestialBodyGenerateRequest PlanetCelestialBodyRequest;
		if (!TryBuildRequestFromDataAsset(ResolvedPlanetClass, SelectedPlanetData, PlanetCelestialBodyRequest))
		{
			return;
		}

		PlanetCelestialBodyRequest.BodySpec.ParentBody = ParentStar;
		CandidatePlanetCelestialBodyRequests.Add(PlanetCelestialBodyRequest);
	}

	if (CandidatePlanetCelestialBodyRequests.IsEmpty())
	{
		return;
	}

	{
		TArray<FSRCelestialBodyGenerateRequest> ResolvedPlanetCelestialBodyRequests;
		BuildOrbitingBodyRequests(ParentStar, CandidatePlanetCelestialBodyRequests.Num(), CandidatePlanetCelestialBodyRequests, RandomStream, ResolvedPlanetCelestialBodyRequests);
		for (int32 PlanetIndex = 0; PlanetIndex < ResolvedPlanetCelestialBodyRequests.Num(); ++PlanetIndex)
		{
			if (ASRCelestialBody* GeneratedPlanet = SpawnOrbitingBody(ResolvedPlanetCelestialBodyRequests[PlanetIndex].BodyClass, ResolvedPlanetCelestialBodyRequests[PlanetIndex], ParentStar))
			{
				OutGeneratedPlanets.Add(GeneratedPlanet);
				SpawnMoons(GeneratedPlanet, RandomStream, RuntimeMoonBodies);
				EnsureParentGravityContainsOrbitingBody(ParentStar, GeneratedPlanet);
			}
		}
	}
}

void ASRSolarSystemGenerator::SpawnMoons(ASRCelestialBody* ParentPlanet, FRandomStream& RandomStream, TArray<TObjectPtr<ASRCelestialBody>>& OutGeneratedMoons)
{
	if (!IsValid(ParentPlanet))
	{
		return;
	}

	const int32 ResolvedMinMoonCount = FMath::Max(0, MinMoon);
	const int32 ResolvedMaxMoonCount = FMath::Max(ResolvedMinMoonCount, MaxMoon);
	const int32 RequestedMoonCount = RandomStream.RandRange(ResolvedMinMoonCount, ResolvedMaxMoonCount);
	if (RequestedMoonCount <= 0)
	{
		return;
	}

	TArray<FSRCelestialBodyGenerateRequest> CandidateMoonCelestialBodyRequests;
	CandidateMoonCelestialBodyRequests.Reserve(RequestedMoonCount);
	const TSubclassOf<ASRCelestialBody> ResolvedMoonClass = ValidateRuntimeCelestialClass(PlanetClass, TEXT("PlanetClass for moons"));
	if (!ResolvedMoonClass)
	{
		return;
	}

	for (int32 MoonIndex = 0; MoonIndex < RequestedMoonCount; ++MoonIndex)
	{
		const USRMoonDataAsset* SelectedMoonData = ResolveRandomDataAssetStrict(MoonDataAssets, RandomStream, TEXT("moon"));
		if (!IsValid(SelectedMoonData))
		{
			return;
		}

		FSRCelestialBodyGenerateRequest MoonCelestialBodyRequest;
		if (!TryBuildRequestFromDataAsset(ResolvedMoonClass, SelectedMoonData, MoonCelestialBodyRequest))
		{
			return;
		}

		MoonCelestialBodyRequest.BodySpec.ParentBody = ParentPlanet;
		CandidateMoonCelestialBodyRequests.Add(MoonCelestialBodyRequest);
	}

	if (CandidateMoonCelestialBodyRequests.IsEmpty())
	{
		return;
	}

	{
		TArray<FSRCelestialBodyGenerateRequest> ResolvedMoonCelestialBodyRequests;
		BuildOrbitingBodyRequests(ParentPlanet, CandidateMoonCelestialBodyRequests.Num(), CandidateMoonCelestialBodyRequests, RandomStream, ResolvedMoonCelestialBodyRequests);
		for (int32 MoonIndex = 0; MoonIndex < ResolvedMoonCelestialBodyRequests.Num(); ++MoonIndex)
		{
			if (ASRCelestialBody* GeneratedMoon = SpawnOrbitingBody(ResolvedMoonCelestialBodyRequests[MoonIndex].BodyClass, ResolvedMoonCelestialBodyRequests[MoonIndex], ParentPlanet))
			{
				OutGeneratedMoons.Add(GeneratedMoon);
			}
		}
	}
}

void ASRSolarSystemGenerator::DestroyTrackedActor(TObjectPtr<ASRCelestialBody>& ActorToDestroy)
{
	if (IsValid(ActorToDestroy) && GetWorld())
	{
		GetWorld()->DestroyActor(ActorToDestroy);
	}

	ActorToDestroy = nullptr;
}

void ASRSolarSystemGenerator::DestroyTrackedActors(TArray<TObjectPtr<ASRCelestialBody>>& ActorsToDestroy)
{
	for (TObjectPtr<ASRCelestialBody>& ActorToDestroy : ActorsToDestroy)
	{
		DestroyTrackedActor(ActorToDestroy);
	}

	ActorsToDestroy.Reset();
}
