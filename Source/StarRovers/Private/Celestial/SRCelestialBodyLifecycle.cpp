#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Engine/World.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

DEFINE_LOG_CATEGORY(LogStarRoversCelestial);

void ASRCelestialBody::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyData();
}

void ASRCelestialBody::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld(); World && World->IsGameWorld() && GetDynamicMeshRuntimeCacheWorld() != World)
	{
		ClearDynamicMeshRuntimeCaches(TEXT("BeginPlay.NewGameWorld"));
		SetDynamicMeshRuntimeCacheWorld(World);
	}

	if (!bHasAppliedData)
	{
		LogMissingDataErrorOnce(TEXT("BeginPlay"));
		return;
	}

	ApplyData();

	if (USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry())
	{
		CelestialRegistry->RegisterCelestialBody(this);
	}
}

void ASRCelestialBody::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld(); World && World->IsGameWorld() && GetDynamicMeshRuntimeCacheWorld() == World)
	{
		ClearDynamicMeshRuntimeCaches(TEXT("EndPlay.GameWorld"));
		SetDynamicMeshRuntimeCacheWorld(nullptr);
	}

	if (USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry())
	{
		CelestialRegistry->UnregisterCelestialBody(this);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void ASRCelestialBody::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyData();
}
#endif

void ASRCelestialBody::LogMissingDataErrorOnce(const TCHAR* Context) const
{
	if (bHasLoggedMissingDataError)
	{
		return;
	}

	bHasLoggedMissingDataError = true;
	UE_LOG(
		LogStarRoversCelestial,
		Error,
		TEXT("%s '%s' requires body data before runtime use. SetData() was never called. Configure it through a data asset-driven spawn path instead of Blueprint defaults."),
		Context ? Context : TEXT("ASRCelestialBody"),
		*GetName());
}

USRCelestialBodyRegistrySubsystem* ASRCelestialBody::FindCelestialRegistry() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>() : nullptr;
}

bool ASRCelestialBody::IsStellarBody() const
{
	return BodyCategory == ESRCelestialBodyCategory::Star;
}
