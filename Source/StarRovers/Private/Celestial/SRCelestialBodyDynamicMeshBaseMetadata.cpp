#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

#include "HAL/CriticalSection.h"
#include "Utility/SRTimingLog.h"

namespace StarRovers::Celestial::DynamicMesh
{
uint32 HashSourcePosition(const FVector& Position)
{
	const FVector Direction = Position.GetSafeNormal();
	constexpr float DirectionQuantizationScale = 100000.0f;
	const int32 QuantizedX = FMath::RoundToInt(Direction.X * DirectionQuantizationScale);
	const int32 QuantizedY = FMath::RoundToInt(Direction.Y * DirectionQuantizationScale);
	const int32 QuantizedZ = FMath::RoundToInt(Direction.Z * DirectionQuantizationScale);
	return HashCombine(HashCombine(::GetTypeHash(QuantizedX), ::GetTypeHash(QuantizedY)), ::GetTypeHash(QuantizedZ));
}

TSharedRef<const FSRCelestialBodyBaseSourceMetadataCacheEntry> BuildBaseSourceMetadataCacheEntry(
	int32 FaceResolution,
	const TArray<FSRPlanetSurfaceGridCell>& BaseCells)
{
	TSharedRef<FSRCelestialBodyBaseSourceMetadataCacheEntry> CacheEntry = MakeShared<FSRCelestialBodyBaseSourceMetadataCacheEntry>();
	CacheEntry->FaceResolution = FaceResolution;
	CacheEntry->Cells.Reserve(BaseCells.Num());

	for (const FSRPlanetSurfaceGridCell& BaseCell : BaseCells)
	{
		FSRDynamicMeshBaseSourceMetadataCell& CellMetadata = CacheEntry->Cells.AddDefaulted_GetRef();
		CellMetadata.CornerHash00 = HashSourcePosition(BaseCell.Corner00);
		CellMetadata.CornerHash10 = HashSourcePosition(BaseCell.Corner10);
		CellMetadata.CornerHash11 = HashSourcePosition(BaseCell.Corner11);
		CellMetadata.CornerHash01 = HashSourcePosition(BaseCell.Corner01);
	}

	return CacheEntry;
}

const FSRCelestialBodyBaseSourceMetadataCacheEntry& GetBaseSourceMetadataCacheEntry(
	int32 FaceResolution,
	const TArray<FSRPlanetSurfaceGridCell>& BaseCells)
{
	static TMap<int32, TSharedPtr<const FSRCelestialBodyBaseSourceMetadataCacheEntry>> CacheByResolution;
	static FCriticalSection CacheCriticalSection;

	const double LockStart = SRCelestialNowSeconds();
	CacheCriticalSection.Lock();
	const double LockWaitMs = SRCelestialElapsedMilliseconds(LockStart);
	if (const TSharedPtr<const FSRCelestialBodyBaseSourceMetadataCacheEntry>* CachedEntry = CacheByResolution.Find(FaceResolution))
	{
		if (CachedEntry->IsValid())
		{
			const FSRCelestialBodyBaseSourceMetadataCacheEntry* Result = CachedEntry->Get();
			CacheCriticalSection.Unlock();
			if (LockWaitMs > 1.0)
			{
				FSRTimingLog::AddLine(FString::Printf(
					TEXT("BaseSourceMetadata CacheHit Resolution=%d LockWait=%.2f ms"),
					FaceResolution,
					LockWaitMs));
			}
			return *Result;
		}
	}

	const double BuildStart = SRCelestialNowSeconds();
	const TSharedRef<const FSRCelestialBodyBaseSourceMetadataCacheEntry> NewEntry =
		BuildBaseSourceMetadataCacheEntry(FaceResolution, BaseCells);
	const double BuildMs = SRCelestialElapsedMilliseconds(BuildStart);
	CacheByResolution.Add(FaceResolution, NewEntry);
	CacheCriticalSection.Unlock();
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("BaseSourceMetadata CacheMissBuild Resolution=%d LockWait=%.2f ms Build=%.2f ms Cells=%d"),
		FaceResolution,
		LockWaitMs,
		BuildMs,
		NewEntry->Cells.Num()));
	return NewEntry.Get();
}
}
