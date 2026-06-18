#include "Celestial/SRCelestialBody.h"

#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

using namespace StarRovers::Celestial::DynamicMesh;

namespace StarRovers::Celestial::DynamicMesh
{
	namespace
	{
		TMap<uint32, FSRCelestialBodyDynamicMeshRuntimeCacheEntry> GCelestialBodyDynamicMeshRuntimeCache;
		TWeakObjectPtr<UWorld> GCelestialBodyRuntimeCacheWorld;
	}

	FSRCelestialBodyDynamicMeshRuntimeCacheEntry* FindCelestialBodyDynamicMeshRuntimeCache(uint32 BuildHash)
	{
		return GCelestialBodyDynamicMeshRuntimeCache.Find(BuildHash);
	}

	FSRCelestialBodyDynamicMeshRuntimeCacheEntry& StoreCelestialBodyDynamicMeshRuntimeCache(
		uint32 BuildHash,
		FSRCelestialBodyDynamicMeshRuntimeCacheEntry&& Entry)
	{
		if (!GCelestialBodyDynamicMeshRuntimeCache.Contains(BuildHash)
			&& GCelestialBodyDynamicMeshRuntimeCache.Num() >= MaxRuntimeDynamicMeshCacheEntries)
		{
			GCelestialBodyDynamicMeshRuntimeCache.Reset();
		}

		FSRCelestialBodyDynamicMeshRuntimeCacheEntry& StoredEntry = GCelestialBodyDynamicMeshRuntimeCache.FindOrAdd(BuildHash);
		StoredEntry = MoveTemp(Entry);
		return StoredEntry;
	}

	void ClearCelestialBodyDynamicMeshRuntimeCache()
	{
		GCelestialBodyDynamicMeshRuntimeCache.Empty();
	}

	int32 GetCelestialBodyDynamicMeshRuntimeCacheEntryCount()
	{
		return GCelestialBodyDynamicMeshRuntimeCache.Num();
	}

	UWorld* GetCelestialBodyDynamicMeshRuntimeCacheWorld()
	{
		return GCelestialBodyRuntimeCacheWorld.Get();
	}

	void SetCelestialBodyDynamicMeshRuntimeCacheWorld(UWorld* World)
	{
		GCelestialBodyRuntimeCacheWorld = World;
	}
}

void ASRCelestialBody::ClearDynamicMeshRuntimeCaches(const TCHAR* Reason)
{
	const int32 DynamicMeshCacheEntries = GetCelestialBodyDynamicMeshRuntimeCacheEntryCount();
	if (DynamicMeshCacheEntries <= 0)
	{
		return;
	}

	ClearCelestialBodyDynamicMeshRuntimeCache();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Celestial body runtime caches cleared. Reason=%s DynamicMeshEntries=%d"),
		Reason ? Reason : TEXT("Unknown"),
		DynamicMeshCacheEntries);
}

UWorld* ASRCelestialBody::GetDynamicMeshRuntimeCacheWorld()
{
	return GetCelestialBodyDynamicMeshRuntimeCacheWorld();
}

void ASRCelestialBody::SetDynamicMeshRuntimeCacheWorld(UWorld* World)
{
	SetCelestialBodyDynamicMeshRuntimeCacheWorld(World);
}

void ASRCelestialBody::AppendRuntimeMemoryDiagnostics(TArray<FString>& OutLines)
{
	OutLines.Add(FString::Printf(
		TEXT("CelestialRuntimeCache DynamicMeshEntries=%d CacheWorld=%s"),
		GetCelestialBodyDynamicMeshRuntimeCacheEntryCount(),
		*GetNameSafe(GetCelestialBodyDynamicMeshRuntimeCacheWorld())));
}
