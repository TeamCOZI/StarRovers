#include "SRCameraSpaceBoundaryResolver.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace
{
	constexpr double SpaceBoundaryFullScanIntervalSeconds = 1.0;
}

bool FSRCameraSpaceBoundaryResolver::Resolve(
	UWorld* World,
	const AActor* ExcludedActor,
	FSRCameraSpaceBoundaryCacheState& Cache,
	FVector& OutCenter,
	float& OutRadius)
{
	if (!World)
	{
		Cache.Reset();
		return false;
	}

	OutCenter = FVector::ZeroVector;
	OutRadius = 0.0f;

	if (Cache.bHasResult && Cache.Frame == GFrameCounter)
	{
		if (!Cache.bFound)
		{
			return false;
		}

		OutCenter = Cache.Center;
		OutRadius = Cache.Radius;
		return OutRadius > KINDA_SMALL_NUMBER;
	}

	if (Cache.bFound && IsValid(Cache.Actor.Get()))
	{
		if (TryResolveSpaceBoundaryActor(Cache.Actor.Get(), OutCenter, OutRadius))
		{
			Cache.Center = OutCenter;
			Cache.Radius = OutRadius;
			Cache.Frame = GFrameCounter;
			Cache.bHasResult = true;
			return true;
		}
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (Cache.bHasResult
		&& !Cache.bFound
		&& CurrentTime - Cache.FullScanTime < SpaceBoundaryFullScanIntervalSeconds)
	{
		Cache.Frame = GFrameCounter;
		return false;
	}

	AActor* BestBoundaryActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == ExcludedActor)
		{
			continue;
		}

		FVector CandidateCenter = FVector::ZeroVector;
		float CandidateRadius = 0.0f;
		if (TryResolveSpaceBoundaryActor(Candidate, CandidateCenter, CandidateRadius)
			&& CandidateRadius > OutRadius)
		{
			OutCenter = CandidateCenter;
			OutRadius = CandidateRadius;
			BestBoundaryActor = const_cast<AActor*>(Candidate);
		}
	}

	Cache.Actor = BestBoundaryActor;
	Cache.Center = OutCenter;
	Cache.Radius = OutRadius;
	Cache.FullScanTime = CurrentTime;
	Cache.Frame = GFrameCounter;
	Cache.bFound = OutRadius > KINDA_SMALL_NUMBER;
	Cache.bHasResult = true;
	return Cache.bFound;
}

bool FSRCameraSpaceBoundaryResolver::IsSpaceBoundaryActor(const AActor* Candidate)
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	const FString CandidateName = Candidate->GetName();
	const FString CandidateClassName = Candidate->GetClass() ? Candidate->GetClass()->GetName() : FString();
	return CandidateName.Contains(TEXT("SpaceSphere"), ESearchCase::IgnoreCase)
		|| CandidateName.Contains(TEXT("SpaceSkySphere"), ESearchCase::IgnoreCase)
		|| CandidateName.Contains(TEXT("Space Sky Sphere"), ESearchCase::IgnoreCase)
		|| CandidateName.Equals(TEXT("BP_Space"), ESearchCase::IgnoreCase)
		|| CandidateName.StartsWith(TEXT("BP_Space_"), ESearchCase::IgnoreCase)
		|| CandidateClassName.Contains(TEXT("SpaceSphere"), ESearchCase::IgnoreCase)
		|| CandidateClassName.Contains(TEXT("SpaceSkySphere"), ESearchCase::IgnoreCase)
		|| CandidateClassName.Equals(TEXT("BP_Space_C"), ESearchCase::IgnoreCase);
}

bool FSRCameraSpaceBoundaryResolver::TryResolveSpaceBoundaryActor(const AActor* Candidate, FVector& OutCenter, float& OutRadius)
{
	OutCenter = FVector::ZeroVector;
	OutRadius = 0.0f;

	if (!IsSpaceBoundaryActor(Candidate))
	{
		return false;
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Candidate);
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		const float CandidateRadius = PrimitiveComponent->Bounds.SphereRadius;
		if (CandidateRadius > OutRadius)
		{
			OutCenter = PrimitiveComponent->Bounds.Origin;
			OutRadius = CandidateRadius;
		}
	}

	if (OutRadius <= KINDA_SMALL_NUMBER)
	{
		FVector ActorOrigin = FVector::ZeroVector;
		FVector ActorExtent = FVector::ZeroVector;
		Candidate->GetActorBounds(false, ActorOrigin, ActorExtent);
		const float ActorRadius = ActorExtent.Size();
		if (ActorRadius > OutRadius)
		{
			OutCenter = ActorOrigin;
			OutRadius = ActorRadius;
		}
	}

	return OutRadius > KINDA_SMALL_NUMBER;
}
