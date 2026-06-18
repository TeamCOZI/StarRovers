#include "Simulation/SRSolarSystemGenerator.h"

#include "Simulation/SRSolarSystemGeneratorInternal.h"

#include "Celestial/SRMoonDataAsset.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Celestial/SRStarDataAsset.h"
#include "Gravity/SRGravityParent.h"

using namespace StarRoversSolarSystemGeneratorInternal;
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
	StarCelestialBodyRequest.BodyData.ParentBody = nullptr;
	StarCelestialBodyRequest.BodyData.OrbitRadius = 0.0f;
	StarCelestialBodyRequest.BodyData.OrbitPeriod = 0.0f;
	StarCelestialBodyRequest.BodyData.InitialAngle = 0.0f;
	if (ShouldRandomizeBodyGenerationSeed(StarCelestialBodyRequest.BodyData))
	{
		ApplyResolvedGenerationSeed(StarCelestialBodyRequest.BodyData, CreateRuntimeRandomGenerationSeed());
	}

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
		? ComputeOrbitWorldLocation(ParentBody, CelestialBodyRequest.BodyData.OrbitRadius, CelestialBodyRequest.BodyData.InitialAngle)
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

	GeneratedCelestialBody->SetData(CelestialBodyRequest.BodyData);
	GeneratedCelestialBody->FinishSpawning(SpawnTransform);

#if WITH_EDITOR
	if (!CelestialBodyRequest.BodyData.VariableName.IsEmpty())
	{
		GeneratedCelestialBody->SetActorLabel(CelestialBodyRequest.BodyData.VariableName.ToString());
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
		CandidateCelestialBodies[Index].BodyData.OrbitRadius = PackedOrbitRadii[Index];
		CandidateCelestialBodies[Index].BodyData.InitialAngle = RandomStream.FRandRange(0.0f, 360.0f);
		const int32 ResolvedGenerationSeed = ShouldRandomizeBodyGenerationSeed(CandidateCelestialBodies[Index].BodyData)
			? CreateRuntimeRandomGenerationSeed()
			: RandomStream.RandRange(1, TNumericLimits<int32>::Max() - 1);
		ApplyResolvedGenerationSeed(CandidateCelestialBodies[Index].BodyData, ResolvedGenerationSeed);
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

	const float ParentBodyRadius = ComputeScaledBodyRadius(ParentBody);
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
		if (!TryComputeScaledBodyRadiusFromCelestialBodyRequest(CelestialBodyRequests[BodyIndex], BodyRadius))
		{
			return false;
		}
		const float GravityRadius = ComputeGravityRadiusFromCelestialBodyRequest(CelestialBodyRequests[BodyIndex]);
		FSROrbitInfo& OrbitInfo = OrbitInfos.AddDefaulted_GetRef();
		OrbitInfo.OrbitingBodyExtent = FMath::Max(BodyRadius, GravityRadius);
		OrbitInfo.DesiredOrbitRadius = InitialOrbit + (OrbitIncrease * static_cast<float>(BodyIndex));
	}

	OutOrbitRadii.SetNumUninitialized(OrbitInfos.Num());
	float NextMinimumInnerEdge = ParentBodyRadius;
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
	const float OrbitingBodyRadius = ComputeScaledBodyRadius(OrbitingBody);
	const USRGravityParent* OrbitingBodyGravityParent = OrbitingBody->GetGravityParent();
	if (!IsValid(OrbitingBodyGravityParent))
	{
		UE_LOG(LogTemp, Error, TEXT("Solar system generation requires GravityParent on '%s'."), *OrbitingBody->GetName());
		return;
	}
	const float OrbitingBodyGravityRadius = OrbitingBodyGravityParent->GetGravityRadius();
	const float OrbitingBodyExtent = FMath::Max(OrbitingBodyRadius, OrbitingBodyGravityRadius);
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

FVector ASRSolarSystemGenerator::ComputeOrbitWorldLocation(const AActor* ParentBody, float OrbitRadius, float InitialAngleDegrees) const
{
	const FVector ParentLocation = IsValid(ParentBody) ? ParentBody->GetActorLocation() : GetActorLocation();
	const float PhaseRadians = FMath::DegreesToRadians(InitialAngleDegrees);

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

		PlanetCelestialBodyRequest.BodyData.ParentBody = ParentStar;
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

		MoonCelestialBodyRequest.BodyData.ParentBody = ParentPlanet;
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
