#include "Simulation/SRCelestialBodyRegistrySubsystem.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "EngineUtils.h"

namespace
{
	bool AreActorListsEquivalent(const TArray<TObjectPtr<AActor>>& PreviousActors, const TArray<TObjectPtr<AActor>>& CurrentActors)
	{
		TSet<const AActor*> CurrentValidActors;
		CurrentValidActors.Reserve(CurrentActors.Num());
		int32 CurrentValidActorCount = 0;
		for (const TObjectPtr<AActor>& CurrentActor : CurrentActors)
		{
			if (IsValid(CurrentActor))
			{
				++CurrentValidActorCount;
				CurrentValidActors.Add(CurrentActor.Get());
			}
		}

		int32 PreviousValidActorCount = 0;
		for (const TObjectPtr<AActor>& PreviousActor : PreviousActors)
		{
			if (!IsValid(PreviousActor))
			{
				continue;
			}

			++PreviousValidActorCount;
			if (!CurrentValidActors.Contains(PreviousActor.Get()))
			{
				return false;
			}
		}

		return PreviousValidActorCount == CurrentValidActorCount;
	}
}

void USRCelestialBodyRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USRCelestialBodyRegistrySubsystem::Deinitialize()
{
	CelestialBodies.Reset();
	PrimaryStarActor = nullptr;
	CelestialBodiesChangedEvent.Clear();
	PrimaryStarActorChangedEvent.Clear();

	Super::Deinitialize();
}

void USRCelestialBodyRegistrySubsystem::RegisterCelestialBody(AActor* BodyActor)
{
	if (!IsValid(BodyActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(BodyActor))
	{
		return;
	}

	CompactCelestialBodies();
	const int32 PreviousBodyCount = CelestialBodies.Num();
	CelestialBodies.AddUnique(BodyActor);
	if (CelestialBodies.Num() != PreviousBodyCount)
	{
		CelestialBodiesChangedEvent.Broadcast();
	}

	if (!IsValid(PrimaryStarActor) && USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
	{
		SetResolvedPrimaryStarActor(BodyActor, true);
	}
}

void USRCelestialBodyRegistrySubsystem::UnregisterCelestialBody(AActor* BodyActor)
{
	if (!IsValid(BodyActor))
	{
		return;
	}

	const int32 PreviousBodyCount = CelestialBodies.Num();
	CelestialBodies.RemoveSingleSwap(BodyActor);
	const bool bBodiesChanged = CelestialBodies.Num() != PreviousBodyCount;
	if (PrimaryStarActor == BodyActor)
	{
		AActor* ReplacementPrimaryStarActor = nullptr;
		for (AActor* RemainingBodyActor : CelestialBodies)
		{
			if (IsValid(RemainingBodyActor) && USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(RemainingBodyActor))
			{
				ReplacementPrimaryStarActor = RemainingBodyActor;
				break;
			}
		}

		SetResolvedPrimaryStarActor(ReplacementPrimaryStarActor, true);
	}

	if (bBodiesChanged)
	{
		CelestialBodiesChangedEvent.Broadcast();
	}
}

void USRCelestialBodyRegistrySubsystem::RefreshCelestialBodies()
{
	const TArray<TObjectPtr<AActor>> PreviousCelestialBodies = CelestialBodies;
	CelestialBodies.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		const bool bBodiesChanged = !AreActorListsEquivalent(PreviousCelestialBodies, CelestialBodies);
		SetResolvedPrimaryStarActor(nullptr, true);
		if (bBodiesChanged)
		{
			CelestialBodiesChangedEvent.Broadcast();
		}
		return;
	}

	AActor* FirstStarActor = nullptr;
	AActor* PreviousPrimaryStarActor = PrimaryStarActor.Get();
	bool bFoundPreviousPrimaryStarActor = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* BodyActor = *It;
		if (!IsValid(BodyActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(BodyActor))
		{
			continue;
		}

		CelestialBodies.Add(BodyActor);
		if (USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			if (!FirstStarActor)
			{
				FirstStarActor = BodyActor;
			}
			if (BodyActor == PreviousPrimaryStarActor)
			{
				bFoundPreviousPrimaryStarActor = true;
			}
		}
	}

	AActor* ResolvedPrimaryStarActor = bFoundPreviousPrimaryStarActor ? PreviousPrimaryStarActor : FirstStarActor;
	SetResolvedPrimaryStarActor(ResolvedPrimaryStarActor, true);
	if (!AreActorListsEquivalent(PreviousCelestialBodies, CelestialBodies))
	{
		CelestialBodiesChangedEvent.Broadcast();
	}
}

void USRCelestialBodyRegistrySubsystem::GetCelestialBodies(TArray<AActor*>& OutCelestialBodies) const
{
	OutCelestialBodies.Reset();

	for (AActor* BodyActor : CelestialBodies)
	{
		if (IsValid(BodyActor))
		{
			OutCelestialBodies.Add(BodyActor);
		}
	}
}

void USRCelestialBodyRegistrySubsystem::SetPrimaryStarActor(AActor* NewPrimaryStarActor)
{
	if (IsValid(NewPrimaryStarActor) && !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(NewPrimaryStarActor))
	{
		return;
	}

	if (IsValid(NewPrimaryStarActor))
	{
		CompactCelestialBodies();
		const int32 PreviousBodyCount = CelestialBodies.Num();
		CelestialBodies.AddUnique(NewPrimaryStarActor);
		if (CelestialBodies.Num() != PreviousBodyCount)
		{
			CelestialBodiesChangedEvent.Broadcast();
		}
	}

	SetResolvedPrimaryStarActor(NewPrimaryStarActor, true);
}

AActor* USRCelestialBodyRegistrySubsystem::GetPrimaryStarActor() const
{
	if (IsValid(PrimaryStarActor) && USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(PrimaryStarActor))
	{
		return PrimaryStarActor;
	}

	for (AActor* BodyActor : CelestialBodies)
	{
		if (IsValid(BodyActor) && USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			return BodyActor;
		}
	}

	return nullptr;
}

FSRCelestialRegistryBodiesChangedSignature& USRCelestialBodyRegistrySubsystem::OnCelestialBodiesChanged()
{
	return CelestialBodiesChangedEvent;
}

FSRCelestialRegistryPrimaryStarChangedSignature& USRCelestialBodyRegistrySubsystem::OnPrimaryStarActorChanged()
{
	return PrimaryStarActorChangedEvent;
}

void USRCelestialBodyRegistrySubsystem::CompactCelestialBodies()
{
	for (int32 Index = CelestialBodies.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(CelestialBodies[Index]))
		{
			CelestialBodies.RemoveAtSwap(Index);
		}
	}
}

void USRCelestialBodyRegistrySubsystem::SetResolvedPrimaryStarActor(AActor* NewPrimaryStarActor, bool bBroadcast)
{
	if (PrimaryStarActor == NewPrimaryStarActor)
	{
		return;
	}

	PrimaryStarActor = NewPrimaryStarActor;
	if (bBroadcast)
	{
		PrimaryStarActorChangedEvent.Broadcast(PrimaryStarActor.Get());
	}
}
